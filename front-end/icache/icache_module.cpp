#include "include/icache_module.h"
#include <cstring>
#include <iostream>

using namespace icache_module_n;

namespace {
inline uint32_t xorshift32(uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

inline uint32_t clamp_latency(uint32_t v) { return (v < 1) ? 1 : v; }
} // namespace

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
  pipe1_to_pipe2.pc_w = 0;
  pipe1_to_pipe2.pc_r = 0;
  pipe2_to_pipe1.ready = true;

#if ICACHE_USE_SRAM_MODEL
  sram_pending_r = false;
  sram_pending_next = false;
  sram_delay_r = 0;
  sram_delay_next = 0;
  sram_index_r = 0;
  sram_index_next = 0;
  sram_pc_r = 0;
  sram_pc_next = 0;
  sram_seed_r = 1;
  sram_seed_next = 1;
  sram_load_fire = false;
#endif
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
  pipe1_to_pipe2.pc_w = 0;
  pipe1_to_pipe2.pc_r = 0;
  pipe2_to_pipe1.ready = true;

#if ICACHE_USE_SRAM_MODEL
  sram_pending_r = false;
  sram_pending_next = false;
  sram_delay_r = 0;
  sram_delay_next = 0;
  sram_index_r = 0;
  sram_index_next = 0;
  sram_pc_r = 0;
  sram_pc_next = 0;
  sram_seed_r = 1;
  sram_seed_next = 1;
  sram_load_fire = false;

#if ICACHE_SRAM_RANDOM_DELAY
  uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
  uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
  if (max_lat < min_lat) {
    max_lat = min_lat;
  }
  std::cout << "[icache] SRAM model: random latency " << min_lat << "-"
            << max_lat << " cycles" << std::endl;
#else
  uint32_t fixed_lat = clamp_latency(ICACHE_SRAM_FIXED_LATENCY);
  std::cout << "[icache] SRAM model: fixed latency " << fixed_lat
            << " cycles" << std::endl;
#endif
#else
  std::cout << "[icache] SRAM model: disabled (register lookup)" << std::endl;
#endif
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

void ICache::export_lookup_set_for_pc(
    uint32_t pc, uint32_t out_data[ICACHE_V1_WAYS][ICACHE_LINE_SIZE / 4],
    uint32_t out_tag[ICACHE_V1_WAYS], bool out_valid[ICACHE_V1_WAYS]) const {
  uint32_t pc_index = (pc >> offset_bits) & (set_num - 1u);
  uint32_t rd_index = pc_index;
#if ICACHE_USE_SRAM_MODEL
  rd_index = sram_pending_r ? sram_index_r : pc_index;
#endif

  for (uint32_t way = 0; way < way_cnt; ++way) {
    for (uint32_t word = 0; word < word_num; ++word) {
      out_data[way][word] = cache_data[rd_index][way][word];
    }
    out_tag[way] = cache_tag[rd_index][way];
    out_valid[way] = cache_valid[rd_index][way];
  }
}

void ICache::lookup_read_set(uint32_t lookup_index, bool gate_valid_with_req) {
  bool from_input = io.in.lookup_from_input;
  for (uint32_t way = 0; way < way_cnt; ++way) {
    for (uint32_t word = 0; word < word_num; ++word) {
      pipe1_to_pipe2.cache_set_data_w[way][word] =
          from_input ? io.in.lookup_set_data[way][word]
                     : cache_data[lookup_index][way][word];
    }
    pipe1_to_pipe2.cache_set_tag_w[way] =
        from_input ? io.in.lookup_set_tag[way] : cache_tag[lookup_index][way];
    bool valid_bit = from_input ? io.in.lookup_set_valid[way]
                                : cache_valid[lookup_index][way];
    if (gate_valid_with_req) {
      valid_bit = valid_bit && io.in.ifu_req_valid;
    }
    pipe1_to_pipe2.cache_set_valid_w[way] = valid_bit;
  }
  pipe1_to_pipe2.index_w = lookup_index;
}

void ICache::lookup(uint32_t index) {
#if ICACHE_USE_SRAM_MODEL
  // SRAM delay model: track pending lookup and delay before data is ready.
  sram_pending_next = sram_pending_r;
  sram_delay_next = sram_delay_r;
  sram_index_next = sram_index_r;
  sram_seed_next = sram_seed_r;
  sram_pc_next = sram_pc_r;
  sram_load_fire = false;

  uint32_t lookup_index = sram_pending_r ? sram_index_r : index;
  lookup_read_set(lookup_index, /*gate_valid_with_req=*/false);
  pipe1_to_pipe2.valid = io.in.ifu_req_valid;
  pipe1_to_pipe2.pc_w = sram_pending_r ? sram_pc_r : io.in.pc;

  if (io.in.refetch) {
    pipe1_to_pipe2.valid_next = false;
    io.out.ifu_req_ready = false;
    io.out.mmu_req_valid = false;
    sram_pending_next = false;
    sram_delay_next = 0;
    return;
  }

  // Accept new request only when no pending SRAM lookup.
  bool can_accept =
      io.in.ifu_req_valid && pipe2_to_pipe1.ready && !sram_pending_r;
  if (can_accept) {
    uint32_t latency = ICACHE_SRAM_FIXED_LATENCY;
#if ICACHE_SRAM_RANDOM_DELAY
    uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
    uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
    if (max_lat < min_lat) {
      max_lat = min_lat;
    }
    uint32_t seed = xorshift32(sram_seed_r);
    sram_seed_next = seed;
    uint32_t range = max_lat - min_lat + 1;
    latency = min_lat + (seed % range);
#endif
    latency = clamp_latency(latency); // treat 0 as 1
    if (latency <= 1) {
      // Data ready for next cycle
      sram_load_fire = true;
    } else {
      // Extra delay cycles beyond the base 1-cycle pipeline
      sram_pending_next = true;
      sram_delay_next = latency - 1;
      sram_index_next = index;
      sram_pc_next = io.in.pc;
    }
  }

  // Progress pending SRAM lookup
  if (sram_pending_r) {
    if (sram_delay_r <= 1) {
      sram_load_fire = true;
      sram_pending_next = false;
      sram_delay_next = 0;
    } else {
      sram_delay_next = sram_delay_r - 1;
    }
  }

  if (sram_load_fire) {
    pipe1_to_pipe2.valid_next = true;
  } else if (pipe2_to_pipe1.ready) {
    pipe1_to_pipe2.valid_next = false;
  } else {
    pipe1_to_pipe2.valid_next = pipe1_to_pipe2.valid_r;
  }

  // MMU request generation (match ICacheV2 top-level wiring).
  io.out.mmu_req_valid = false;
  if (can_accept) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = io.in.pc >> 12;
  } else if (pipe1_to_pipe2.valid_r && !io.in.ppn_valid) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = pipe1_to_pipe2.pc_r >> 12;
  } else if (sram_pending_next) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = sram_pc_next >> 12;
  }

  io.out.ifu_req_ready = pipe2_to_pipe1.ready && !sram_pending_r;
#else
  lookup_read_set(index, /*gate_valid_with_req=*/true);
  pipe1_to_pipe2.valid = io.in.ifu_req_valid;
  pipe1_to_pipe2.pc_w = io.in.pc;

  if (io.in.refetch) {
    pipe1_to_pipe2.valid_next = false;
    io.out.ifu_req_ready = false;
    io.out.mmu_req_valid = false;
    return;
  }

  bool fire = io.in.ifu_req_valid && pipe2_to_pipe1.ready;
  if (fire) {
    pipe1_to_pipe2.valid_next = true;
  } else if (pipe2_to_pipe1.ready) {
    pipe1_to_pipe2.valid_next = false;
  } else {
    pipe1_to_pipe2.valid_next = pipe1_to_pipe2.valid_r;
  }
  io.out.ifu_req_ready = pipe2_to_pipe1.ready;

  // MMU request generation (match ICacheV2 top-level wiring).
  io.out.mmu_req_valid = false;
  if (fire) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = io.in.pc >> 12;
  } else if (pipe1_to_pipe2.valid_r && !io.in.ppn_valid) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = pipe1_to_pipe2.pc_r >> 12;
  }
#endif
}

void ICache::comb_pipe1() {
  // Logic for Pipe 1 (IFU Request -> Pipe Register)
  uint32_t index = (io.in.pc >> offset_bits) & (set_num - 1);

  lookup(index);
}

void ICache::comb_pipe2() {
  io.out.ppn_ready = false;
  io.out.mem_resp_ready = false;
  io.out.mem_req_valid = false;
  io.out.ifu_resp_valid = false;
  io.out.ifu_resp_pc = pipe1_to_pipe2.pc_r;
  io.out.ifu_page_fault = false;
  io.out.mem_req_id = 0;
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

#if ICACHE_USE_SRAM_MODEL
  if (sram_load_fire && !io.in.refetch) {
    // Load data after SRAM delay
    for (uint32_t way = 0; way < way_cnt; ++way) {
      for (uint32_t word = 0; word < word_num; ++word) {
        pipe1_to_pipe2.cache_set_data_r[way][word] =
            pipe1_to_pipe2.cache_set_data_w[way][word];
      }
      pipe1_to_pipe2.cache_set_tag_r[way] = pipe1_to_pipe2.cache_set_tag_w[way];
      pipe1_to_pipe2.cache_set_valid_r[way] =
          pipe1_to_pipe2.cache_set_valid_w[way];
    }
    pipe1_to_pipe2.pc_r = pipe1_to_pipe2.pc_w;
    pipe1_to_pipe2.index_r = pipe1_to_pipe2.index_w;
  }
#else
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
    pipe1_to_pipe2.pc_r = pipe1_to_pipe2.pc_w;
    pipe1_to_pipe2.index_r = pipe1_to_pipe2.index_w;
  }
#endif

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

#if ICACHE_USE_SRAM_MODEL
  sram_pending_r = sram_pending_next;
  sram_delay_r = sram_delay_next;
  sram_index_r = sram_index_next;
  sram_pc_r = sram_pc_next;
  sram_seed_r = sram_seed_next;
#endif
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
