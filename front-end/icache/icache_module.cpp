#include "include/icache_module.h"
#include <cstring>
#include <iostream>

using namespace icache_module_n;

ICache::ICache() {
  reset();

  // Initialize cache data, tags, and valid bits
  for (uint32_t i = 0; i < set_num; ++i) {
    for (uint32_t j = 0; j < way_cnt; ++j) {
      cache_valid[i][j] = false;
      cache_tag[i][j] = 0;
      for (uint32_t k = 0; k < word_num; ++k) {
        cache_data[i][j][k] = 0;
      }
    }
  }

  // Initialize state variables
  state = IDLE;
  state_next = IDLE;
  mem_axi_state = AXI_IDLE;
  mem_axi_state_next = AXI_IDLE;
  mem_req_sent = false;
  replace_idx = 0;
  replace_idx_next = 0;
  ppn_r = 0;
  mem_gnt = 0;
  pipe1_to_pipe2.valid_r = false;
  pipe1_to_pipe2.valid_next = false;
  pipe2_to_pipe1.ready = true;
}

void ICache::reset() {
  state = IDLE;
  state_next = IDLE;
  mem_axi_state = AXI_IDLE;
  mem_axi_state_next = AXI_IDLE;
  mem_req_sent = false;
  replace_idx = 0;
  replace_idx_next = 0;
  ppn_r = 0;
  mem_gnt = 0;
  pipe1_to_pipe2.valid_r = false;
  pipe1_to_pipe2.valid_next = false;
  pipe2_to_pipe1.ready = true;
}

void ICache::invalidate_all() {
  for (uint32_t i = 0; i < set_num; ++i) {
    for (uint32_t j = 0; j < way_cnt; ++j) {
      cache_valid[i][j] = false;
    }
  }
}

void ICache::comb() {
  pipe1_to_pipe2_t pipe1_to_pipe2_last = pipe1_to_pipe2;
  pipe2_to_pipe1_t pipe2_to_pipe1_last = pipe2_to_pipe1;
  int cnt = 0;
  while (true) {
    comb_pipe1();
    comb_pipe2();

    bool pipe1_equal =
        pipe1_to_pipe2_last.valid == pipe1_to_pipe2.valid &&
        pipe1_to_pipe2_last.index_w == pipe1_to_pipe2.index_w &&
        pipe1_to_pipe2_last.valid_next == pipe1_to_pipe2.valid_next;

    bool pipe2_equal = pipe2_to_pipe1_last.ready == pipe2_to_pipe1.ready;

    for (uint32_t way = 0; way < way_cnt; ++way) {
      for (uint32_t word = 0; word < word_num; ++word) {
        pipe1_equal =
            pipe1_equal && (pipe1_to_pipe2_last.cache_set_data_w[way][word] ==
                            pipe1_to_pipe2.cache_set_data_w[way][word]);
      }
      pipe1_equal = pipe1_equal &&
                    (pipe1_to_pipe2_last.cache_set_tag_w[way] ==
                     pipe1_to_pipe2.cache_set_tag_w[way]) &&
                    (pipe1_to_pipe2_last.cache_set_valid_w[way] ==
                     pipe1_to_pipe2.cache_set_valid_w[way]);
    }

    if (pipe1_equal && pipe2_equal) {
      break;
    }
    pipe1_to_pipe2_last = pipe1_to_pipe2;
    pipe2_to_pipe1_last = pipe2_to_pipe1;

    if (++cnt > 20) {
      std::cerr << "Warning: ICache combinational logic did not converge."
                << std::endl;
      exit(1);
    }
  }
}

void ICache::seq() { seq_pipe1(); }

void ICache::comb_pipe1() {
  // Logic for Pipe 1 (IFU Request -> Pipe Register)

  uint32_t index = (io.in.pc >> offset_bits) & (set_num - 1);

  // Read cache arrays
  for (uint32_t way = 0; way < way_cnt; ++way) {
    for (uint32_t word = 0; word < word_num; ++word) {
      pipe1_to_pipe2.cache_set_data_w[way][word] = cache_data[index][way][word];
    }
    pipe1_to_pipe2.cache_set_tag_w[way] = cache_tag[index][way];
    pipe1_to_pipe2.cache_set_valid_w[way] =
        cache_valid[index][way] && io.in.ifu_req_valid;
  }
  pipe1_to_pipe2.index_w = index;
  pipe1_to_pipe2.valid = io.in.ifu_req_valid;

  // Flow control
  if (io.in.refetch) {
    pipe1_to_pipe2.valid_next = false;
    io.out.ifu_req_ready = false;
  } else {
    bool fire = io.in.ifu_req_valid && pipe2_to_pipe1.ready;

    if (fire) {
      pipe1_to_pipe2.valid_next = true;
    } else if (pipe2_to_pipe1.ready) {
      pipe1_to_pipe2.valid_next = false;
    } else {
      pipe1_to_pipe2.valid_next = pipe1_to_pipe2.valid_r;
    }
    io.out.ifu_req_ready = pipe2_to_pipe1.ready;
  }
}

void ICache::comb_pipe2() {
  io.out.ppn_ready = false;
  io.out.mem_resp_ready = false;
  io.out.mem_req_valid = false;
  io.out.ifu_resp_valid = false;
  io.out.ifu_page_fault = false;
  mem_axi_state_next = mem_axi_state;
  pipe2_to_pipe1.ready = false; // Default blocked

  switch (state) {
  case IDLE:
    if (io.in.refetch) {
      state_next = IDLE;
      pipe2_to_pipe1.ready = true;
      return;
    }

    if (pipe1_to_pipe2.valid_r && io.in.ppn_valid) {
      if (io.in.page_fault) {
        for (uint32_t word = 0; word < word_num; ++word) {
          io.out.rd_data[word] = 0;
        }
        io.out.ifu_resp_valid = true;
        io.out.ifu_page_fault = true;
        pipe2_to_pipe1.ready = true;
        state_next = IDLE;
        return;
      }

      bool hit = false;
      for (uint32_t way = 0; way < way_cnt; ++way) {
        if (pipe1_to_pipe2.cache_set_valid_r[way] &&
            pipe1_to_pipe2.cache_set_tag_r[way] == (io.in.ppn & 0xFFFFF)) {
          hit = true;
          for (uint32_t word = 0; word < word_num; ++word) {
            io.out.rd_data[word] = pipe1_to_pipe2.cache_set_data_r[way][word];
          }
          break;
        }
      }

      io.out.ifu_resp_valid = hit;
      pipe2_to_pipe1.ready = hit;
      state_next = hit ? IDLE : SWAP_IN;

    } else if (pipe1_to_pipe2.valid_r && !io.in.ppn_valid) {
      pipe2_to_pipe1.ready = false;
      state_next = IDLE;
    } else {
      pipe2_to_pipe1.ready = true;
      state_next = IDLE;
    }

    io.out.ppn_ready = pipe1_to_pipe2.valid_r;
    break;

  case SWAP_IN:
    if (io.in.refetch) {
      if (mem_axi_state != AXI_IDLE) {
        state_next = DRAIN;
        pipe2_to_pipe1.ready = false;
      } else {
        state_next = IDLE;
        pipe2_to_pipe1.ready = true;
      }
      return;
    }

    if (mem_axi_state == AXI_IDLE) {
      io.out.mem_req_valid = true;
      io.out.mem_req_addr =
          (ppn_r << 12) | (pipe1_to_pipe2.index_r << offset_bits);
      state_next = SWAP_IN;
      mem_axi_state_next =
          (io.out.mem_req_valid && io.in.mem_req_ready) ? AXI_BUSY : AXI_IDLE;
    } else {
      io.out.mem_req_valid = false;
      io.out.mem_resp_ready = true;

      mem_gnt = io.in.mem_resp_valid && io.out.mem_resp_ready;
      state_next = SWAP_IN;
      if (mem_gnt) {
        state_next = SWAP_IN_OKEY;
        mem_axi_state_next = AXI_IDLE;
        for (uint32_t offset = 0; offset < ICACHE_LINE_SIZE / 4; ++offset) {
          mem_resp_data_w[offset] = io.in.mem_resp_data[offset];
        }

        bool found_invalid = false;
        for (uint32_t way = 0; way < way_cnt; ++way) {
          if (!pipe1_to_pipe2.cache_set_valid_r[way]) {
            replace_idx_next = way;
            found_invalid = true;
            break;
          }
        }
        if (!found_invalid) {
          replace_idx_next = (replace_idx + 1) % way_cnt;
        }
      }
    }
    break;

  case SWAP_IN_OKEY:
    state_next = IDLE;
    if (!io.in.refetch) {
      io.out.ifu_resp_valid = true;
      for (uint32_t word = 0; word < word_num; ++word) {
        io.out.rd_data[word] = mem_resp_data_r[word];
      }
      pipe2_to_pipe1.ready = true;
    } else {
      pipe2_to_pipe1.ready = true;
    }
    break;

  case DRAIN:
    io.out.mem_resp_ready = true;
    io.out.mem_req_valid = false;

    if (io.in.mem_resp_valid) {
      mem_axi_state_next = AXI_IDLE;
      state_next = IDLE;
      pipe2_to_pipe1.ready = true;
    } else {
      state_next = DRAIN;
      pipe2_to_pipe1.ready = false;
    }
    break;

  default:
    std::cerr << "Error: Invalid state in ICache::comb_pipe2()" << std::endl;
    exit(1);
    break;
  }
}

void ICache::seq_pipe1() {
  if (state == SWAP_IN_OKEY) {
    // Update Cache
    for (uint32_t word = 0; word < word_num; ++word) {
      cache_data[pipe1_to_pipe2.index_r][replace_idx][word] =
          mem_resp_data_r[word];
    }
    cache_tag[pipe1_to_pipe2.index_r][replace_idx] = ppn_r;
    cache_valid[pipe1_to_pipe2.index_r][replace_idx] = true;
  }

  // SWAP_IN data latching
  if (state == SWAP_IN) {
    for (uint32_t offset = 0; offset < ICACHE_LINE_SIZE / 4; ++offset) {
      mem_resp_data_r[offset] = mem_resp_data_w[offset];
    }
    replace_idx = replace_idx_next;
  }

  // Update valid_r based on valid_next calculated in comb_pipe1
  pipe1_to_pipe2.valid_r = pipe1_to_pipe2.valid_next;

  if (pipe1_to_pipe2.valid && pipe2_to_pipe1.ready && !io.in.refetch) {
    // Load data
    for (uint32_t way = 0; way < way_cnt; ++way) {
      for (uint32_t word = 0; word < word_num; ++word) {
        pipe1_to_pipe2.cache_set_data_r[way][word] =
            pipe1_to_pipe2.cache_set_data_w[way][word];
      }
      pipe1_to_pipe2.cache_set_tag_r[way] = pipe1_to_pipe2.cache_set_tag_w[way];
      pipe1_to_pipe2.cache_set_valid_r[way] =
          pipe1_to_pipe2.cache_set_valid_w[way];
    }
    pipe1_to_pipe2.index_r = pipe1_to_pipe2.index_w;
  }

  // Save PPN
  if (io.in.ppn_valid && io.out.ppn_ready) {
    ppn_r = io.in.ppn;
  }

  // Track outstanding memory request status
  if (state == SWAP_IN && mem_axi_state == AXI_IDLE &&
      io.out.mem_req_valid && io.in.mem_req_ready) {
    mem_req_sent = true;
  }
  if (state == SWAP_IN && state_next == SWAP_IN_OKEY) {
    mem_req_sent = false;
  }
  if (state == DRAIN && io.in.mem_resp_valid) {
    mem_req_sent = false;
  }
  if (state == IDLE) {
    mem_req_sent = false;
  }

  // State update
  state = state_next;
  mem_axi_state = mem_axi_state_next;
}

void ICache::log_state() {
  std::cout << "ICache State: ";
  switch (state) {
  case IDLE:
    std::cout << "IDLE";
    break;
  case SWAP_IN:
    std::cout << "SWAP_IN";
    break;
  case SWAP_IN_OKEY:
    std::cout << "SWAP_IN_OKEY";
    break;
  case DRAIN:
    std::cout << "DRAIN";
    break;
  default:
    std::cout << "UNKNOWN";
    break;
  }
  std::cout << " -> ";
  switch (state_next) {
  case IDLE:
    std::cout << "IDLE";
    break;
  case SWAP_IN:
    std::cout << "SWAP_IN";
    break;
  case SWAP_IN_OKEY:
    std::cout << "SWAP_IN_OKEY";
    break;
  case DRAIN:
    std::cout << "DRAIN";
    break;
  default:
    std::cout << "UNKNOWN";
    break;
  }
  std::cout << std::endl;
  std::cout << "  mem_axi_state: " << (mem_axi_state == AXI_IDLE ? "IDLE"
                                                               : "BUSY")
            << " mem_req_v=" << io.out.mem_req_valid
            << " mem_req_rdy=" << io.in.mem_req_ready
            << " mem_resp_v=" << io.in.mem_resp_valid
            << " mem_resp_rdy=" << io.out.mem_resp_ready << std::endl;
}
void ICache::log_tag(uint32_t index) {
  if (index >= set_num) {
    std::cerr << "Index out of bounds in log_tag: " << index << std::endl;
    return;
  }
  std::cout << "Cache Set Index: " << index << std::endl;
  for (uint32_t way = 0; way < way_cnt; ++way) {
    std::cout << "  Way " << way << ": Valid=" << cache_valid[index][way]
              << ", Tag=0x" << std::hex << cache_tag[index][way] << std::dec
              << ", Data=[";
    for (uint32_t word = 0; word < word_num; ++word) {
      if (word > 0)
        std::cout << ", ";
      std::cout << "0x" << std::hex << cache_data[index][way][word] << std::dec;
    }
    std::cout << "]" << std::endl;
  }
}
void ICache::log_valid(uint32_t index) {
  if (index >= set_num) {
    std::cerr << "Index out of bounds in log_valid: " << index << std::endl;
    return;
  }
  std::cout << "Cache Set Index: " << index << " Valid Bits: ";
  for (uint32_t way = 0; way < way_cnt; ++way) {
    std::cout << cache_valid[index][way] << " ";
  }
  std::cout << std::endl;
}
void ICache::log_pipeline() {
  std::cout << "Pipeline Registers:" << std::endl;
  std::cout << "  pipe1_to_pipe2.valid_r: " << pipe1_to_pipe2.valid_r
            << std::endl;
  std::cout << "  pipe1_to_pipe2.index_r: " << pipe1_to_pipe2.index_r
            << std::endl;
  std::cout << "  ppn_r: 0x" << std::hex << ppn_r << std::dec << std::endl;
  for (uint32_t way = 0; way < way_cnt; ++way) {
    std::cout << "  Way " << way
              << ": Valid=" << pipe1_to_pipe2.cache_set_valid_r[way]
              << ", Tag=0x" << std::hex << pipe1_to_pipe2.cache_set_tag_r[way]
              << std::dec << ", Data=[";
    for (uint32_t word = 0; word < word_num; ++word) {
      if (word > 0)
        std::cout << ", ";
      std::cout << "0x" << std::hex
                << pipe1_to_pipe2.cache_set_data_r[way][word] << std::dec;
    }
    std::cout << "]" << std::endl;
  }
}
