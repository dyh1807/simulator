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

inline uint32_t clamp_latency(uint32_t v) { return (v < 1u) ? 1u : v; }

inline bool lookup_latency_enabled() { return ICACHE_LOOKUP_LATENCY > 0; }

inline uint32_t lookup_fixed_latency() {
  if (!lookup_latency_enabled()) {
    return 0;
  }
  return clamp_latency(ICACHE_LOOKUP_LATENCY);
}

inline bool lookup_random_delay_enabled() {
  return lookup_latency_enabled() && (ICACHE_SRAM_RANDOM_DELAY != 0);
}
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
  io.regs.state = static_cast<uint8_t>(IDLE);
  state_next = IDLE;
  io.regs.mem_axi_state = static_cast<uint8_t>(AXI_IDLE);
  mem_axi_state_next = AXI_IDLE;
  io.regs.mem_req_sent = false;
  io.regs.replace_idx = 0;
  replace_idx_next = 0;
  io.regs.ppn_r = 0;
  mem_gnt = 0;
  io.regs.pipe_valid_r = false;
  pipe1_to_pipe2.valid_next = false;
  pipe1_to_pipe2.pc_w = 0;
  pipe2_to_pipe1.ready = true;

  io.regs.sram_pending_r = false;
  sram_pending_next = false;
  io.regs.sram_delay_r = 0;
  sram_delay_next = 0;
  io.regs.sram_index_r = 0;
  sram_index_next = 0;
  io.regs.sram_pc_r = 0;
  sram_pc_next = 0;
  io.regs.sram_seed_r = 1;
  sram_seed_next = 1;
  sram_load_fire = false;
}

void ICache::reset() {
  io.regs.state = static_cast<uint8_t>(IDLE);
  state_next = IDLE;
  io.regs.mem_axi_state = static_cast<uint8_t>(AXI_IDLE);
  mem_axi_state_next = AXI_IDLE;
  io.regs.mem_req_sent = false;
  io.regs.replace_idx = 0;
  replace_idx_next = 0;
  io.regs.ppn_r = 0;
  mem_gnt = 0;
  io.regs.pipe_valid_r = false;
  pipe1_to_pipe2.valid_next = false;
  pipe1_to_pipe2.pc_w = 0;
  io.regs.pipe_pc_r = 0;
  io.regs.pipe_index_r = 0;
  pipe2_to_pipe1.ready = true;

  io.regs.sram_pending_r = false;
  sram_pending_next = false;
  io.regs.sram_delay_r = 0;
  sram_delay_next = 0;
  io.regs.sram_index_r = 0;
  sram_index_next = 0;
  io.regs.sram_pc_r = 0;
  sram_pc_next = 0;
  io.regs.sram_seed_r = 1;
  sram_seed_next = 1;
  sram_load_fire = false;

  if (lookup_latency_enabled()) {
    if (lookup_random_delay_enabled()) {
      uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
      uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
      if (max_lat < min_lat) {
        max_lat = min_lat;
      }
      std::cout << "[icache] SRAM model: random latency " << min_lat << "-"
                << max_lat << " cycles" << std::endl;
    } else {
      std::cout << "[icache] SRAM model: fixed latency "
                << lookup_fixed_latency() << " cycles" << std::endl;
    }
  } else {
    std::cout << "[icache] SRAM model: disabled (register lookup)" << std::endl;
  }
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
    // Default register write-back: hold values unless overwritten by comb logic.
    io.reg_write = io.regs;

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
  if (lookup_latency_enabled()) {
    rd_index = io.regs.sram_pending_r ? io.regs.sram_index_r : pc_index;
  }

  for (uint32_t way = 0; way < way_cnt; ++way) {
    for (uint32_t word = 0; word < word_num; ++word) {
      out_data[way][word] = cache_data[rd_index][way][word];
    }
    out_tag[way] = cache_tag[rd_index][way];
    out_valid[way] = cache_valid[rd_index][way];
  }
}

void ICache::lookup_read_set(uint32_t lookup_index, bool gate_valid_with_req) {
  constexpr bool from_input = (ICACHE_LOOKUP_FROM_INPUT != 0);
  bool resp_valid = true;
  if (from_input) {
    resp_valid = io.lookup_in.lookup_resp_valid;
  }
  for (uint32_t way = 0; way < way_cnt; ++way) {
    for (uint32_t word = 0; word < word_num; ++word) {
      pipe1_to_pipe2.cache_set_data_w[way][word] =
          from_input ? io.lookup_in.lookup_set_data[way][word]
                     : cache_data[lookup_index][way][word];
    }
    pipe1_to_pipe2.cache_set_tag_w[way] =
        from_input ? io.lookup_in.lookup_set_tag[way]
                   : cache_tag[lookup_index][way];
    bool valid_bit = from_input ? io.lookup_in.lookup_set_valid[way]
                                : cache_valid[lookup_index][way];
    if (from_input) {
      // When the lookup response is not transfer-valid, treat all entries as
      // invalid regardless of their tag/data/valid fields.
      valid_bit = resp_valid && valid_bit;
    }
    if (gate_valid_with_req) {
      valid_bit = valid_bit && io.in.ifu_req_valid;
    }
    pipe1_to_pipe2.cache_set_valid_w[way] = valid_bit;
  }
  pipe1_to_pipe2.index_w = lookup_index;
}

void ICache::lookup(uint32_t index) {
  const bool use_latency = lookup_latency_enabled();
  const bool kill_pipe = io.in.refetch;

  sram_pending_next = use_latency ? io.regs.sram_pending_r : false;
  sram_delay_next = use_latency ? io.regs.sram_delay_r : 0;
  sram_index_next = use_latency ? io.regs.sram_index_r : 0;
  sram_seed_next = io.regs.sram_seed_r;
  sram_pc_next = use_latency ? io.regs.sram_pc_r : 0;
  sram_load_fire = false;

  uint32_t lookup_index =
      (use_latency && io.regs.sram_pending_r) ? io.regs.sram_index_r : index;
  lookup_read_set(lookup_index, /*gate_valid_with_req=*/!use_latency);
  pipe1_to_pipe2.valid = io.in.ifu_req_valid;
  pipe1_to_pipe2.pc_w =
      (use_latency && io.regs.sram_pending_r) ? io.regs.sram_pc_r : io.in.pc;

  if (kill_pipe) {
    pipe1_to_pipe2.valid_next = false;
    io.out.ifu_req_ready = false;
    io.out.mmu_req_valid = false;
    io.reg_write.pipe_valid_r = pipe1_to_pipe2.valid_next;
    io.reg_write.sram_pending_r = false;
    io.reg_write.sram_delay_r = 0;
    io.reg_write.sram_index_r = 0;
    io.reg_write.sram_pc_r = 0;
    io.reg_write.sram_seed_r = sram_seed_next;
    return;
  }

  bool can_accept = io.in.ifu_req_valid && pipe2_to_pipe1.ready &&
                    (!use_latency || !io.regs.sram_pending_r);
  if (can_accept) {
    if (!use_latency) {
      sram_load_fire = true;
    } else {
      uint32_t latency = lookup_fixed_latency();
      if (lookup_random_delay_enabled()) {
        uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
        uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
        if (max_lat < min_lat) {
          max_lat = min_lat;
        }
        uint32_t seed = xorshift32(io.regs.sram_seed_r);
        sram_seed_next = seed;
        uint32_t range = max_lat - min_lat + 1u;
        latency = min_lat + (seed % range);
      }
      latency = clamp_latency(latency);
      if (latency <= 1u) {
        sram_load_fire = true;
      } else {
        sram_pending_next = true;
        sram_delay_next = latency - 1u;
        sram_index_next = index;
        sram_pc_next = io.in.pc;
      }
    }
  }

  if (use_latency && io.regs.sram_pending_r) {
    if (io.regs.sram_delay_r <= 1u) {
      sram_load_fire = true;
      sram_pending_next = false;
      sram_delay_next = 0;
    } else {
      sram_delay_next = io.regs.sram_delay_r - 1u;
    }
  }

  if (sram_load_fire) {
    pipe1_to_pipe2.valid_next = true;
  } else if (pipe2_to_pipe1.ready) {
    pipe1_to_pipe2.valid_next = false;
  } else {
    pipe1_to_pipe2.valid_next = io.regs.pipe_valid_r;
  }

  io.reg_write.pipe_valid_r = pipe1_to_pipe2.valid_next;
  io.reg_write.sram_pending_r = use_latency ? sram_pending_next : false;
  io.reg_write.sram_delay_r = use_latency ? sram_delay_next : 0;
  io.reg_write.sram_index_r = use_latency ? sram_index_next : 0;
  io.reg_write.sram_pc_r = use_latency ? sram_pc_next : 0;
  io.reg_write.sram_seed_r = sram_seed_next;

  if (sram_load_fire && !kill_pipe) {
    for (uint32_t way = 0; way < way_cnt; ++way) {
      for (uint32_t word = 0; word < word_num; ++word) {
        io.reg_write.pipe_cache_set_data_r[way][word] =
            pipe1_to_pipe2.cache_set_data_w[way][word];
      }
      io.reg_write.pipe_cache_set_tag_r[way] =
          pipe1_to_pipe2.cache_set_tag_w[way];
      io.reg_write.pipe_cache_set_valid_r[way] =
          pipe1_to_pipe2.cache_set_valid_w[way];
    }
    io.reg_write.pipe_pc_r = pipe1_to_pipe2.pc_w;
    io.reg_write.pipe_index_r = pipe1_to_pipe2.index_w;
  }

  io.out.mmu_req_valid = false;
  if (can_accept) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = io.in.pc >> 12;
  } else if (io.regs.pipe_valid_r && !io.in.ppn_valid) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = io.regs.pipe_pc_r >> 12;
  } else if (use_latency && sram_pending_next) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = sram_pc_next >> 12;
  }

  io.out.ifu_req_ready =
      pipe2_to_pipe1.ready && (!use_latency || !io.regs.sram_pending_r);
}

void ICache::comb_pipe1() {
  // Logic for Pipe 1 (IFU Request -> Pipe Register)
  uint32_t index = (io.in.pc >> offset_bits) & (set_num - 1);

  lookup(index);
}

void ICache::comb_pipe2() {
  io.table_write.we = false;
  io.table_write.index = 0;
  io.table_write.way = 0;
  for (uint32_t word = 0; word < word_num; ++word) {
    io.table_write.data[word] = 0;
  }
  io.table_write.tag = 0;
  io.table_write.valid = false;

  ICacheState state = static_cast<ICacheState>(io.regs.state);
  AXIState mem_axi_state = static_cast<AXIState>(io.regs.mem_axi_state);
  mem_gnt = false;

  io.out.ppn_ready = false;
  io.out.mem_resp_ready = false;
  io.out.mem_req_valid = false;
  io.out.ifu_resp_valid = false;
  io.out.ifu_resp_pc = io.regs.pipe_pc_r;
  io.out.ifu_page_fault = false;
  io.out.mem_req_id = 0;
  state_next = state;
  mem_axi_state_next = mem_axi_state;
  pipe2_to_pipe1.ready = false; // Default blocked
  const bool kill_pipe = io.in.refetch;

  switch (state) {
  case IDLE:
    if (kill_pipe) {
      state_next = IDLE;
      pipe2_to_pipe1.ready = true;
      break;
    }

    if (io.regs.pipe_valid_r && io.in.ppn_valid) {
      if (io.in.page_fault) {
        for (uint32_t word = 0; word < word_num; ++word) {
          io.out.rd_data[word] = 0;
        }
        io.out.ifu_resp_valid = true;
        io.out.ifu_page_fault = true;
        pipe2_to_pipe1.ready = true;
        state_next = IDLE;
        break;
      }

      bool hit = false;
      for (uint32_t way = 0; way < way_cnt; ++way) {
        if (io.regs.pipe_cache_set_valid_r[way] &&
            io.regs.pipe_cache_set_tag_r[way] == (io.in.ppn & 0xFFFFF)) {
          hit = true;
          for (uint32_t word = 0; word < word_num; ++word) {
            io.out.rd_data[word] = io.regs.pipe_cache_set_data_r[way][word];
          }
          break;
        }
      }

      io.out.ifu_resp_valid = hit;
      pipe2_to_pipe1.ready = hit;
      state_next = hit ? IDLE : SWAP_IN;

    } else if (io.regs.pipe_valid_r && !io.in.ppn_valid) {
      pipe2_to_pipe1.ready = false;
      state_next = IDLE;
    } else {
      pipe2_to_pipe1.ready = true;
      state_next = IDLE;
    }

    io.out.ppn_ready = io.regs.pipe_valid_r;
    break;

  case SWAP_IN:
    if (kill_pipe) {
      if (mem_axi_state != AXI_IDLE) {
        state_next = DRAIN;
        pipe2_to_pipe1.ready = false;
      } else {
        state_next = IDLE;
        pipe2_to_pipe1.ready = true;
      }
      break;
    }

    if (mem_axi_state == AXI_IDLE) {
      io.out.mem_req_valid = true;
      io.out.mem_req_addr =
          (io.regs.ppn_r << 12) | (io.regs.pipe_index_r << offset_bits);
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
          if (!io.regs.pipe_cache_set_valid_r[way]) {
            replace_idx_next = way;
            found_invalid = true;
            break;
          }
        }
        if (!found_invalid) {
          replace_idx_next = (io.regs.replace_idx + 1) % way_cnt;
        }
      }
    }
    break;

  case SWAP_IN_OKEY:
    state_next = IDLE;

    if (!io.in.flush) {
      io.table_write.we = true;
      io.table_write.index = io.regs.pipe_index_r;
      io.table_write.way = io.regs.replace_idx;
      for (uint32_t word = 0; word < word_num; ++word) {
        io.table_write.data[word] = io.regs.mem_resp_data_r[word];
      }
      io.table_write.tag = io.regs.ppn_r;
      io.table_write.valid = true;
    }

    if (!io.in.refetch && !io.in.flush) {
      io.out.ifu_resp_valid = true;
      for (uint32_t word = 0; word < word_num; ++word) {
        io.out.rd_data[word] = io.regs.mem_resp_data_r[word];
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

  // Register write-back (applied in seq).
  io.reg_write.state = static_cast<uint8_t>(state_next);
  io.reg_write.mem_axi_state = static_cast<uint8_t>(mem_axi_state_next);

  // Save PPN (latched on ppn_valid && ppn_ready).
  if (io.in.ppn_valid && io.out.ppn_ready) {
    io.reg_write.ppn_r = io.in.ppn;
  }

  // Latch memory response data + replacement index when the response arrives.
  if (state == SWAP_IN && mem_gnt) {
    for (uint32_t offset = 0; offset < ICACHE_LINE_SIZE / 4; ++offset) {
      io.reg_write.mem_resp_data_r[offset] = mem_resp_data_w[offset];
    }
    io.reg_write.replace_idx = replace_idx_next;
  }

  // Track outstanding memory request status.
  bool mem_req_sent_next = io.regs.mem_req_sent;
  if (state == SWAP_IN && mem_axi_state == AXI_IDLE && io.out.mem_req_valid &&
      io.in.mem_req_ready) {
    mem_req_sent_next = true;
  }
  if (state == SWAP_IN && state_next == SWAP_IN_OKEY) {
    mem_req_sent_next = false;
  }
  if (state == DRAIN && io.in.mem_resp_valid) {
    mem_req_sent_next = false;
  }
  if (state == IDLE) {
    mem_req_sent_next = false;
  }
  io.reg_write.mem_req_sent = mem_req_sent_next;
}

void ICache::seq_pipe1() {
  if (io.in.flush) {
    // fence.i visibility point: invalidate cache lines at seq boundary.
    invalidate_all();
  }

  // Apply table write (register-based table backend).
  if (io.table_write.we && !io.in.flush) {
    uint32_t index = io.table_write.index;
    uint32_t way = io.table_write.way;
    if (index < set_num && way < way_cnt) {
      for (uint32_t word = 0; word < word_num; ++word) {
        cache_data[index][way][word] = io.table_write.data[word];
      }
      cache_tag[index][way] = io.table_write.tag;
      cache_valid[index][way] = io.table_write.valid;
    }
  }

  // Apply register write-back (unconditional in seq).
  io.regs = io.reg_write;
}

void ICache::log_state() {
  ICacheState state = static_cast<ICacheState>(io.regs.state);
  AXIState mem_axi_state = static_cast<AXIState>(io.regs.mem_axi_state);

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
  std::cout << "  pipe1_to_pipe2.valid_r: " << io.regs.pipe_valid_r
            << std::endl;
  std::cout << "  pipe1_to_pipe2.index_r: " << io.regs.pipe_index_r
            << std::endl;
  std::cout << "  ppn_r: 0x" << std::hex << io.regs.ppn_r << std::dec << std::endl;
  for (uint32_t way = 0; way < way_cnt; ++way) {
    std::cout << "  Way " << way
              << ": Valid=" << io.regs.pipe_cache_set_valid_r[way]
              << ", Tag=0x" << std::hex << io.regs.pipe_cache_set_tag_r[way]
              << std::dec << ", Data=[";
    for (uint32_t word = 0; word < word_num; ++word) {
      if (word > 0)
        std::cout << ", ";
      std::cout << "0x" << std::hex
                << io.regs.pipe_cache_set_data_r[way][word] << std::dec;
    }
    std::cout << "]" << std::endl;
  }
}
