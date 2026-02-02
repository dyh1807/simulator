#include "include/icache_module_v2.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

// -----------------------------------------------------------------------------
// SRAM lookup latency model (reuse macro family from icache_module.h)
// -----------------------------------------------------------------------------
#ifndef ICACHE_USE_SRAM_MODEL
#define ICACHE_USE_SRAM_MODEL 0
#endif
#ifndef ICACHE_SRAM_FIXED_LATENCY
#define ICACHE_SRAM_FIXED_LATENCY 1
#endif
#ifndef ICACHE_SRAM_RANDOM_DELAY
#define ICACHE_SRAM_RANDOM_DELAY 0
#endif
#ifndef ICACHE_SRAM_RANDOM_MIN
#define ICACHE_SRAM_RANDOM_MIN 1
#endif
#ifndef ICACHE_SRAM_RANDOM_MAX
#define ICACHE_SRAM_RANDOM_MAX 4
#endif

namespace icache_module_v2_n {

namespace {
inline uint32_t clamp_latency(uint32_t v) { return (v < 1) ? 1u : v; }
} // namespace

bool ICacheV2::is_power_of_two(uint32_t v) { return v && ((v & (v - 1)) == 0); }

uint32_t ICacheV2::xorshift32(uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

ICacheV2::ICacheV2(ICacheV2Config cfg) : cfg_(cfg) {
  // External interface width is fixed by ICACHE_LINE_SIZE.
  cfg_.line_bytes = ICACHE_LINE_SIZE;

  if (!is_power_of_two(cfg_.line_bytes)) {
    std::cerr << "[icache_v2] line_bytes must be power-of-two" << std::endl;
    std::exit(1);
  }
  if (cfg_.line_bytes % 4 != 0) {
    std::cerr << "[icache_v2] line_bytes must be multiple of 4" << std::endl;
    std::exit(1);
  }
  if (cfg_.page_offset_bits < 1) {
    std::cerr << "[icache_v2] page_offset_bits invalid" << std::endl;
    std::exit(1);
  }
  if (cfg_.ways < 1) {
    std::cerr << "[icache_v2] ways must be >= 1" << std::endl;
    std::exit(1);
  }
  if (cfg_.mshr_num < 1) {
    std::cerr << "[icache_v2] mshr_num must be >= 1" << std::endl;
    std::exit(1);
  }
  if (cfg_.rob_depth < 1) {
    std::cerr << "[icache_v2] rob_depth must be >= 1" << std::endl;
    std::exit(1);
  }

  offset_bits_ = __builtin_ctz(cfg_.line_bytes);
  if (cfg_.page_offset_bits < offset_bits_) {
    std::cerr << "[icache_v2] page_offset_bits < offset_bits (line too big)"
              << std::endl;
    std::exit(1);
  }
  index_bits_ = cfg_.page_offset_bits - offset_bits_;
  set_num_ = 1u << index_bits_;
  word_num_ = cfg_.line_bytes / 4;
  waiters_words_ = (cfg_.rob_depth + 63u) / 64u;

  // Allocate arrays.
  cache_data_.assign(static_cast<size_t>(set_num_) * cfg_.ways * word_num_, 0);
  cache_tag_.assign(static_cast<size_t>(set_num_) * cfg_.ways, 0);
  cache_valid_.assign(static_cast<size_t>(set_num_) * cfg_.ways, 0);

  rr_ptr_r_.assign(set_num_, 0);
  rr_ptr_next_ = rr_ptr_r_;

  if (cfg_.repl == ReplPolicy::PLRU && !is_power_of_two(cfg_.ways)) {
    std::cerr << "[icache_v2] PLRU requires ways to be power-of-two; fallback to RR"
              << std::endl;
    cfg_.repl = ReplPolicy::RR;
  }
  if (cfg_.repl == ReplPolicy::PLRU && cfg_.ways > 1) {
    plru_bits_r_.assign(static_cast<size_t>(set_num_) * (cfg_.ways - 1), 0);
    plru_bits_next_ = plru_bits_r_;
  } else {
    plru_bits_r_.clear();
    plru_bits_next_.clear();
  }

  // ROB
  rob_valid_r_.assign(cfg_.rob_depth, 0);
  rob_valid_next_ = rob_valid_r_;
  rob_pc_r_.assign(cfg_.rob_depth, 0);
  rob_pc_next_ = rob_pc_r_;
  rob_state_r_.assign(cfg_.rob_depth, static_cast<uint8_t>(RobState::EMPTY));
  rob_state_next_ = rob_state_r_;
  rob_line_addr_r_.assign(cfg_.rob_depth, 0);
  rob_line_addr_next_ = rob_line_addr_r_;
  rob_mshr_idx_r_.assign(cfg_.rob_depth, 0);
  rob_mshr_idx_next_ = rob_mshr_idx_r_;
  rob_line_data_r_.assign(static_cast<size_t>(cfg_.rob_depth) * word_num_, 0);
  rob_line_data_next_ = rob_line_data_r_;

  // Lookup stage vectors
  set_data_w_.assign(static_cast<size_t>(cfg_.ways) * word_num_, 0);
  set_data_r_ = set_data_w_;
  set_tag_w_.assign(cfg_.ways, 0);
  set_tag_r_ = set_tag_w_;
  set_valid_w_.assign(cfg_.ways, 0);
  set_valid_r_ = set_valid_w_;
  cache_fill_words_.assign(word_num_, 0);

  // MSHR
  mshr_state_r_.assign(cfg_.mshr_num, static_cast<uint8_t>(MshrState::FREE));
  mshr_state_next_ = mshr_state_r_;
  mshr_line_addr_r_.assign(cfg_.mshr_num, 0);
  mshr_line_addr_next_ = mshr_line_addr_r_;
  mshr_is_prefetch_r_.assign(cfg_.mshr_num, 0);
  mshr_is_prefetch_next_ = mshr_is_prefetch_r_;
  mshr_txid_r_.assign(cfg_.mshr_num, 0);
  mshr_txid_next_ = mshr_txid_r_;
  mshr_txid_valid_r_.assign(cfg_.mshr_num, 0);
  mshr_txid_valid_next_ = mshr_txid_valid_r_;
  mshr_waiters_r_.assign(static_cast<size_t>(cfg_.mshr_num) * waiters_words_,
                         0);
  mshr_waiters_next_ = mshr_waiters_r_;

  reset();
}

uint32_t ICacheV2::mask_lsb(uint32_t bits) const {
  if (bits >= 32) {
    return 0xFFFFFFFFu;
  }
  if (bits == 0) {
    return 0;
  }
  return (1u << bits) - 1u;
}

uint32_t ICacheV2::get_index_from_pc(uint32_t pc) const {
  return (pc >> offset_bits_) & (set_num_ - 1u);
}

uint32_t ICacheV2::get_ppn_from_paddr(uint32_t paddr) const {
  return paddr >> cfg_.page_offset_bits;
}

uint32_t ICacheV2::get_line_addr(uint32_t ppn, uint32_t index) const {
  return (ppn << cfg_.page_offset_bits) | (index << offset_bits_);
}

uint32_t ICacheV2::cache_tag_at(uint32_t set, uint32_t way) const {
  size_t idx = static_cast<size_t>(set) * cfg_.ways + way;
  return cache_tag_[idx];
}

bool ICacheV2::cache_valid_at(uint32_t set, uint32_t way) const {
  size_t idx = static_cast<size_t>(set) * cfg_.ways + way;
  return cache_valid_[idx] != 0;
}

uint32_t ICacheV2::cache_data_at(uint32_t set, uint32_t way,
                                 uint32_t word) const {
  size_t idx = (static_cast<size_t>(set) * cfg_.ways + way) * word_num_ + word;
  return cache_data_[idx];
}

void ICacheV2::cache_write_line(uint32_t set, uint32_t way,
                                const uint32_t *line_words) {
  for (uint32_t w = 0; w < word_num_; ++w) {
    size_t idx =
        (static_cast<size_t>(set) * cfg_.ways + way) * word_num_ + w;
    cache_data_[idx] = line_words[w];
  }
}

void ICacheV2::cache_set_tag_valid(uint32_t set, uint32_t way, uint32_t tag,
                                   bool valid) {
  size_t idx = static_cast<size_t>(set) * cfg_.ways + way;
  cache_tag_[idx] = tag;
  cache_valid_[idx] = valid ? 1 : 0;
}

uint32_t ICacheV2::plru_choose(uint32_t set) const {
  if (cfg_.ways <= 1) {
    return 0;
  }
  const uint32_t ways = cfg_.ways;
  size_t base = static_cast<size_t>(set) * (ways - 1);
  uint32_t node = 0;
  uint32_t leaf_base = 0;
  uint32_t span = ways;
  while (span > 1) {
    uint8_t bit = plru_bits_r_[base + node];
    uint32_t half = span >> 1;
    bool go_left = (bit == 0); // 0 => left subtree is LRU
    if (!go_left) {
      leaf_base += half;
      node = 2 * node + 2;
    } else {
      node = 2 * node + 1;
    }
    span = half;
  }
  return leaf_base;
}

void ICacheV2::plru_update(uint32_t set, uint32_t way) {
  if (cfg_.ways <= 1) {
    return;
  }
  const uint32_t ways = cfg_.ways;
  size_t base = static_cast<size_t>(set) * (ways - 1);
  uint32_t node = 0;
  uint32_t leaf_base = 0;
  uint32_t span = ways;
  while (span > 1) {
    uint32_t half = span >> 1;
    bool hit_left = (way < (leaf_base + half));
    // If we accessed left, then right becomes LRU => bit=1. Vice versa.
    plru_bits_next_[base + node] = hit_left ? 1 : 0;
    if (hit_left) {
      node = 2 * node + 1;
    } else {
      leaf_base += half;
      node = 2 * node + 2;
    }
    span = half;
  }
}

uint32_t ICacheV2::choose_victim_way(uint32_t set) {
  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    if (!cache_valid_at(set, way)) {
      return way;
    }
  }

  if (cfg_.repl == ReplPolicy::PLRU && !plru_bits_r_.empty()) {
    return plru_choose(set);
  }

  if (cfg_.repl == ReplPolicy::RANDOM) {
    rand_seed_next_ = xorshift32(rand_seed_r_);
    return rand_seed_r_ % cfg_.ways;
  }

  // RR default
  return rr_ptr_r_[set] % cfg_.ways;
}

void ICacheV2::repl_on_access(uint32_t set, uint32_t way) {
  if (cfg_.repl == ReplPolicy::PLRU && !plru_bits_next_.empty()) {
    plru_update(set, way);
  }
}

uint32_t ICacheV2::free_mshr_count() const {
  uint32_t free_cnt = 0;
  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    if (static_cast<MshrState>(mshr_state_r_[i]) == MshrState::FREE) {
      free_cnt++;
    }
  }
  return free_cnt;
}

int ICacheV2::alloc_txid() {
  for (int k = 0; k < 16; ++k) {
    uint8_t id = static_cast<uint8_t>((txid_rr_r_ + k) & 0xF);
    if (!txid_inflight_next_[id]) {
      txid_inflight_next_[id] = true;
      txid_canceled_next_[id] = false;
      txid_mshr_valid_next_[id] = false;
      txid_rr_next_ = static_cast<uint8_t>((id + 1) & 0xF);
      return id;
    }
  }
  return -1;
}

void ICacheV2::free_txid(uint8_t txid) {
  txid &= 0xF;
  txid_inflight_next_[txid] = false;
  txid_canceled_next_[txid] = false;
  txid_mshr_valid_next_[txid] = false;
  txid_mshr_next_[txid] = 0;
}

bool ICacheV2::mshr_waiter_test(uint32_t mshr_idx, uint32_t rob_idx) const {
  uint32_t word = rob_idx / 64u;
  uint32_t bit = rob_idx % 64u;
  size_t idx = static_cast<size_t>(mshr_idx) * waiters_words_ + word;
  return ((mshr_waiters_r_[idx] >> bit) & 1ull) != 0ull;
}

void ICacheV2::mshr_waiter_set(uint32_t mshr_idx, uint32_t rob_idx) {
  uint32_t word = rob_idx / 64u;
  uint32_t bit = rob_idx % 64u;
  size_t idx = static_cast<size_t>(mshr_idx) * waiters_words_ + word;
  mshr_waiters_next_[idx] |= (1ull << bit);
}

void ICacheV2::invalidate_all() {
  std::fill(cache_valid_.begin(), cache_valid_.end(), 0);
}

void ICacheV2::reset() {
  epoch_r_ = 0;
  epoch_next_ = 0;

  mshr_peak_r_ = 0;
  mshr_peak_next_ = 0;

  rob_head_r_ = 0;
  rob_tail_r_ = 0;
  rob_count_r_ = 0;
  rob_head_next_ = 0;
  rob_tail_next_ = 0;
  rob_count_next_ = 0;
  std::fill(rob_valid_r_.begin(), rob_valid_r_.end(), 0);
  std::fill(rob_valid_next_.begin(), rob_valid_next_.end(), 0);
  std::fill(rob_pc_r_.begin(), rob_pc_r_.end(), 0);
  std::fill(rob_pc_next_.begin(), rob_pc_next_.end(), 0);
  std::fill(rob_state_r_.begin(), rob_state_r_.end(),
            static_cast<uint8_t>(RobState::EMPTY));
  std::fill(rob_state_next_.begin(), rob_state_next_.end(),
            static_cast<uint8_t>(RobState::EMPTY));
  std::fill(rob_line_addr_r_.begin(), rob_line_addr_r_.end(), 0);
  std::fill(rob_line_addr_next_.begin(), rob_line_addr_next_.end(), 0);
  std::fill(rob_mshr_idx_r_.begin(), rob_mshr_idx_r_.end(), 0);
  std::fill(rob_mshr_idx_next_.begin(), rob_mshr_idx_next_.end(), 0);
  std::fill(rob_line_data_r_.begin(), rob_line_data_r_.end(), 0);
  std::fill(rob_line_data_next_.begin(), rob_line_data_next_.end(), 0);

  lookup_valid_r_ = false;
  lookup_valid_next_ = false;
  lookup_pc_r_ = 0;
  lookup_pc_next_ = 0;
  lookup_index_r_ = 0;
  lookup_index_next_ = 0;
  lookup_rob_idx_r_ = 0;
  lookup_rob_idx_next_ = 0;

  std::fill(set_data_r_.begin(), set_data_r_.end(), 0);
  std::fill(set_tag_r_.begin(), set_tag_r_.end(), 0);
  std::fill(set_valid_r_.begin(), set_valid_r_.end(), 0);
  set_load_fire_ = false;
  cache_fill_fire_ = false;

  sram_pending_r_ = false;
  sram_delay_r_ = 0;
  sram_index_r_ = 0;
  sram_pc_r_ = 0;
  sram_rob_idx_r_ = 0;
  sram_seed_r_ = 1;

  memreq_latched_valid_r_ = false;
  memreq_latched_addr_r_ = 0;
  memreq_latched_id_r_ = 0;
  memreq_latched_mshr_r_ = 0;

  for (int i = 0; i < 16; ++i) {
    txid_inflight_r_[i] = false;
    txid_canceled_r_[i] = false;
    txid_mshr_r_[i] = 0;
    txid_mshr_valid_r_[i] = false;
  }
  txid_rr_r_ = 0;

  std::fill(rr_ptr_r_.begin(), rr_ptr_r_.end(), 0);
  if (!plru_bits_r_.empty()) {
    std::fill(plru_bits_r_.begin(), plru_bits_r_.end(), 0);
  }
  rand_seed_r_ = 1;

  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    mshr_state_r_[i] = static_cast<uint8_t>(MshrState::FREE);
    mshr_line_addr_r_[i] = 0;
    mshr_is_prefetch_r_[i] = 0;
    mshr_txid_r_[i] = 0;
    mshr_txid_valid_r_[i] = 0;
  }
  std::fill(mshr_waiters_r_.begin(), mshr_waiters_r_.end(), 0);

#if ICACHE_USE_SRAM_MODEL
  uint32_t fixed_lat = clamp_latency(ICACHE_SRAM_FIXED_LATENCY);
  std::cout << "[icache_v2] SRAM model: ";
#if ICACHE_SRAM_RANDOM_DELAY
  uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
  uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
  if (max_lat < min_lat) {
    max_lat = min_lat;
  }
  std::cout << "random latency " << min_lat << "-" << max_lat << " cycles"
            << std::endl;
#else
  std::cout << "fixed latency " << fixed_lat << " cycles" << std::endl;
#endif
#else
  std::cout << "[icache_v2] SRAM model: disabled (register lookup)" << std::endl;
#endif
  std::cout << "[icache_v2] ways=" << cfg_.ways << " sets=" << set_num_
            << " mshr=" << cfg_.mshr_num << " rob=" << cfg_.rob_depth
            << " repl=" << static_cast<int>(cfg_.repl)
            << " prefetch=" << (cfg_.prefetch_enable ? 1 : 0) << std::endl;
}

void ICacheV2::comb() {
  io.out = {};
  io.out.mem_resp_ready = true; // always drain
  set_load_fire_ = false;
  cache_fill_fire_ = false;

  // Initialize next state = current state
  epoch_next_ = epoch_r_;

  rr_ptr_next_ = rr_ptr_r_;
  plru_bits_next_ = plru_bits_r_;
  rand_seed_next_ = rand_seed_r_;

  rob_head_next_ = rob_head_r_;
  rob_tail_next_ = rob_tail_r_;
  rob_count_next_ = rob_count_r_;
  rob_valid_next_ = rob_valid_r_;
  rob_pc_next_ = rob_pc_r_;
  rob_state_next_ = rob_state_r_;
  rob_line_addr_next_ = rob_line_addr_r_;
  rob_mshr_idx_next_ = rob_mshr_idx_r_;
  rob_line_data_next_ = rob_line_data_r_;

  lookup_valid_next_ = lookup_valid_r_;
  lookup_pc_next_ = lookup_pc_r_;
  lookup_index_next_ = lookup_index_r_;
  lookup_rob_idx_next_ = lookup_rob_idx_r_;

  sram_pending_next_ = sram_pending_r_;
  sram_delay_next_ = sram_delay_r_;
  sram_index_next_ = sram_index_r_;
  sram_pc_next_ = sram_pc_r_;
  sram_rob_idx_next_ = sram_rob_idx_r_;
  sram_seed_next_ = sram_seed_r_;
  sram_load_fire_ = false;

  mshr_state_next_ = mshr_state_r_;
  mshr_line_addr_next_ = mshr_line_addr_r_;
  mshr_is_prefetch_next_ = mshr_is_prefetch_r_;
  mshr_txid_next_ = mshr_txid_r_;
  mshr_txid_valid_next_ = mshr_txid_valid_r_;
  mshr_waiters_next_ = mshr_waiters_r_;

  mshr_peak_next_ = mshr_peak_r_;

  for (int i = 0; i < 16; ++i) {
    txid_inflight_next_[i] = txid_inflight_r_[i];
    txid_canceled_next_[i] = txid_canceled_r_[i];
    txid_mshr_next_[i] = txid_mshr_r_[i];
    txid_mshr_valid_next_[i] = txid_mshr_valid_r_[i];
  }
  txid_rr_next_ = txid_rr_r_;

  memreq_latched_valid_next_ = memreq_latched_valid_r_;
  memreq_latched_addr_next_ = memreq_latched_addr_r_;
  memreq_latched_id_next_ = memreq_latched_id_r_;
  memreq_latched_mshr_next_ = memreq_latched_mshr_r_;

  // Refetch: kill inflight state and unblock next cycle.
  if (io.in.refetch) {
    epoch_next_ = epoch_r_ + 1;

    // If a memory response arrives in the refetch cycle, we will drop the data
    // (pipeline is killed). Make sure to free the txid now; otherwise, the
    // interconnect may consume the response (ready=1) and the txid would leak.
    if (io.in.mem_resp_valid) {
      uint8_t rid = static_cast<uint8_t>(io.in.mem_resp_id & 0xF);
      if (txid_inflight_next_[rid]) {
        free_txid(rid);
      }
    }

    // Cancel txids:
    // - If the request was already accepted by memory (MSHR in WAIT_RESP),
    //   keep txid inflight and mark canceled so the eventual response frees it.
    // - If the request was never accepted (ALLOC/SENDING or unbound), free txid
    //   immediately (otherwise refetch can leak all txids over time).
    for (int id = 0; id < 16; ++id) {
      if (!txid_inflight_next_[id]) {
        continue;
      }
      bool issued = false;
      if (txid_mshr_valid_r_[id]) {
        uint32_t mi = static_cast<uint32_t>(txid_mshr_r_[id]);
        if (mi < cfg_.mshr_num) {
          if (static_cast<MshrState>(mshr_state_r_[mi]) == MshrState::WAIT_RESP) {
            issued = true;
          }
        }
      }
      if (issued) {
        txid_canceled_next_[id] = true;
        txid_mshr_valid_next_[id] = false;
      } else {
        free_txid(static_cast<uint8_t>(id));
      }
    }

    // Drop ROB/lookup/SRAM.
    rob_head_next_ = 0;
    rob_tail_next_ = 0;
    rob_count_next_ = 0;
    std::fill(rob_valid_next_.begin(), rob_valid_next_.end(), 0);
    std::fill(rob_state_next_.begin(), rob_state_next_.end(),
              static_cast<uint8_t>(RobState::EMPTY));
    lookup_valid_next_ = false;
    sram_pending_next_ = false;
    sram_delay_next_ = 0;
    memreq_latched_valid_next_ = false;

    // Free all MSHRs.
    for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
      mshr_state_next_[i] = static_cast<uint8_t>(MshrState::FREE);
      mshr_txid_valid_next_[i] = 0;
      mshr_is_prefetch_next_[i] = 0;
      mshr_line_addr_next_[i] = 0;
    }
    std::fill(mshr_waiters_next_.begin(), mshr_waiters_next_.end(), 0);

    io.out.ifu_req_ready = false;
    io.out.ifu_resp_valid = false;
    io.out.ppn_ready = false;
    io.out.mem_req_valid = false;
    return;
  }

  auto find_mshr_by_line_next = [&](uint32_t line_addr) -> int {
    for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
      if (static_cast<MshrState>(mshr_state_next_[i]) == MshrState::FREE) {
        continue;
      }
      if (mshr_line_addr_next_[i] == line_addr) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };

  auto find_free_mshr_next = [&]() -> int {
    for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
      if (static_cast<MshrState>(mshr_state_next_[i]) == MshrState::FREE) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };

  auto cache_line_present = [&](uint32_t ppn, uint32_t set) -> bool {
    for (uint32_t way = 0; way < cfg_.ways; ++way) {
      if (!cache_valid_at(set, way)) {
        continue;
      }
      if (cache_tag_at(set, way) == ppn) {
        return true;
      }
    }
    return false;
  };

  auto mshr_waiter_test_next = [&](uint32_t mshr_idx, uint32_t rob_idx) -> bool {
    uint32_t word = rob_idx / 64u;
    uint32_t bit = rob_idx % 64u;
    size_t idx = static_cast<size_t>(mshr_idx) * waiters_words_ + word;
    return ((mshr_waiters_next_[idx] >> bit) & 1ull) != 0ull;
  };

  // -------------------------------------------------------------------------
  // IFU response (in-order ROB head) with a stage2 head-hit bypass.
  // - Normal: rob_head in READY/READY_FAULT drives response.
  // - Bypass: if stage2 resolves the current rob_head as a HIT/FAULT in this
  //   cycle, respond immediately without waiting an extra seq() update.
  // This significantly reduces hit-latency and avoids IPC regression on
  // hit-dominated workloads.
  // -------------------------------------------------------------------------
  bool resp_fire = false;
  bool stage2_head_resp_valid = false;
  bool stage2_head_page_fault = false;
  uint32_t stage2_head_pc = 0;
  uint32_t stage2_head_line[ICACHE_LINE_SIZE / 4] = {0};

  // -------------------------------------------------------------------------
  // SRAM/Register lookup stage1 array read (wire)
  // -------------------------------------------------------------------------
  uint32_t pc_index = get_index_from_pc(io.in.pc);
  uint32_t rd_index = sram_pending_r_ ? sram_index_r_ : pc_index;

  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    set_tag_w_[way] = cache_tag_at(rd_index, way);
    set_valid_w_[way] = cache_valid_at(rd_index, way) ? 1 : 0;
    for (uint32_t w = 0; w < word_num_; ++w) {
      set_data_w_[static_cast<size_t>(way) * word_num_ + w] =
          cache_data_at(rd_index, way, w);
    }
  }

  // -------------------------------------------------------------------------
  // Lookup stage2: consume lookup regs when translation is available
  // -------------------------------------------------------------------------
  io.out.ppn_ready = lookup_valid_r_;

  bool lookup_fire = false;
  if (lookup_valid_r_) {
    if (io.in.ppn_valid) {
      lookup_fire = true;
      uint32_t rob_idx = lookup_rob_idx_r_;
      if (rob_idx < cfg_.rob_depth && rob_valid_r_[rob_idx]) {
        if (io.in.page_fault) {
          rob_state_next_[rob_idx] =
              static_cast<uint8_t>(RobState::READY_FAULT);
          for (uint32_t w = 0; w < word_num_; ++w) {
            rob_line_data_next_[static_cast<size_t>(rob_idx) * word_num_ + w] =
                0;
          }
          if (rob_idx == rob_head_r_) {
            stage2_head_resp_valid = true;
            stage2_head_page_fault = true;
            stage2_head_pc = rob_pc_r_[rob_idx];
            // stage2_head_line stays zero.
          }
        } else {
          bool hit = false;
          uint32_t hit_way = 0;
          for (uint32_t way = 0; way < cfg_.ways; ++way) {
            if (set_valid_r_[way] && set_tag_r_[way] == io.in.ppn) {
              hit = true;
              hit_way = way;
              break;
            }
          }
          if (hit) {
            rob_state_next_[rob_idx] = static_cast<uint8_t>(RobState::READY);
            for (uint32_t w = 0; w < word_num_; ++w) {
              rob_line_data_next_[static_cast<size_t>(rob_idx) * word_num_ + w] =
                  set_data_r_[static_cast<size_t>(hit_way) * word_num_ + w];
            }
            repl_on_access(lookup_index_r_, hit_way);
            if (rob_idx == rob_head_r_) {
              stage2_head_resp_valid = true;
              stage2_head_page_fault = false;
              stage2_head_pc = rob_pc_r_[rob_idx];
              for (uint32_t w = 0; w < word_num_; ++w) {
                stage2_head_line[w] =
                    set_data_r_[static_cast<size_t>(hit_way) * word_num_ + w];
              }
            }
          } else {
            uint32_t line_addr = get_line_addr(io.in.ppn, lookup_index_r_);
            rob_line_addr_next_[rob_idx] = line_addr;

            int mshr_idx = find_mshr_by_line_next(line_addr);
            if (mshr_idx >= 0) {
              mshr_waiter_set(static_cast<uint32_t>(mshr_idx), rob_idx);
              rob_state_next_[rob_idx] =
                  static_cast<uint8_t>(RobState::WAIT_MSHR);
              rob_mshr_idx_next_[rob_idx] = static_cast<uint32_t>(mshr_idx);
            } else {
              int free_idx = find_free_mshr_next();
              if (free_idx >= 0) {
                uint32_t mi = static_cast<uint32_t>(free_idx);
                mshr_state_next_[mi] = static_cast<uint8_t>(MshrState::ALLOC);
                mshr_line_addr_next_[mi] = line_addr;
                mshr_is_prefetch_next_[mi] = 0;
                mshr_txid_valid_next_[mi] = 0;
                for (uint32_t w = 0; w < waiters_words_; ++w) {
                  mshr_waiters_next_[static_cast<size_t>(mi) * waiters_words_ + w] =
                      0;
                }
                mshr_waiter_set(mi, rob_idx);
                rob_state_next_[rob_idx] =
                    static_cast<uint8_t>(RobState::WAIT_MSHR);
                rob_mshr_idx_next_[rob_idx] = mi;

                // Next-line prefetch (simple next-line)
                if (cfg_.prefetch_enable) {
                  uint32_t free_cnt = 0;
                  for (uint32_t j = 0; j < cfg_.mshr_num; ++j) {
                    if (static_cast<MshrState>(mshr_state_next_[j]) ==
                        MshrState::FREE) {
                      free_cnt++;
                    }
                  }
                  if (free_cnt > cfg_.prefetch_reserve) {
                    uint32_t pf_addr =
                        line_addr + cfg_.prefetch_distance * cfg_.line_bytes;
                    uint32_t pf_set = (pf_addr >> offset_bits_) & (set_num_ - 1u);
                    uint32_t pf_ppn = get_ppn_from_paddr(pf_addr);
                    bool pf_hit = cache_line_present(pf_ppn, pf_set);
                    bool pf_in_mshr = (find_mshr_by_line_next(pf_addr) >= 0);
                    if (!pf_hit && !pf_in_mshr) {
                      int pf_free = find_free_mshr_next();
                      if (pf_free >= 0) {
                        uint32_t pfi = static_cast<uint32_t>(pf_free);
                        mshr_state_next_[pfi] =
                            static_cast<uint8_t>(MshrState::ALLOC);
                        mshr_line_addr_next_[pfi] = pf_addr;
                        mshr_is_prefetch_next_[pfi] = 1;
                        mshr_txid_valid_next_[pfi] = 0;
                        for (uint32_t w = 0; w < waiters_words_; ++w) {
                          mshr_waiters_next_[static_cast<size_t>(pfi) * waiters_words_ + w] =
                              0;
                        }
                      }
                    }
                  }
                }
              } else {
                rob_state_next_[rob_idx] =
                    static_cast<uint8_t>(RobState::WAIT_MSHR_ALLOC);
              }
            }
          }
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // IFU response select + pop (1 per cycle)
  // -------------------------------------------------------------------------
  bool resp_valid = false;
  bool resp_from_rob = false;
  bool resp_page_fault = false;
  uint32_t resp_pc = 0;
  uint32_t head = rob_head_r_;

  if (rob_count_r_ > 0 && rob_valid_r_[head]) {
    RobState st = static_cast<RobState>(rob_state_r_[head]);
    if (st == RobState::READY || st == RobState::READY_FAULT) {
      resp_valid = true;
      resp_from_rob = true;
      resp_pc = rob_pc_r_[head];
      resp_page_fault = (st == RobState::READY_FAULT);
    }
  }
  if (!resp_valid && stage2_head_resp_valid) {
    resp_valid = true;
    resp_from_rob = false;
    resp_pc = stage2_head_pc;
    resp_page_fault = stage2_head_page_fault;
  }

  if (resp_valid) {
    io.out.ifu_resp_valid = true;
    io.out.ifu_resp_pc = resp_pc;
    io.out.ifu_page_fault = resp_page_fault;
    for (uint32_t w = 0; w < word_num_; ++w) {
      if (resp_from_rob) {
        io.out.rd_data[w] =
            rob_line_data_r_[static_cast<size_t>(head) * word_num_ + w];
      } else {
        io.out.rd_data[w] = stage2_head_line[w];
      }
    }
    resp_fire = io.in.ifu_resp_ready;
  }

  if (resp_fire) {
    rob_valid_next_[head] = 0;
    rob_state_next_[head] = static_cast<uint8_t>(RobState::EMPTY);
    rob_head_next_ = (rob_head_r_ + 1) % cfg_.rob_depth;
    rob_count_next_ = rob_count_next_ - 1;
  }

  bool stage2_ready_for_new = (!lookup_valid_r_) || lookup_fire;

  // -------------------------------------------------------------------------
  // SRAM model progression (single outstanding lookup)
  // -------------------------------------------------------------------------
#if ICACHE_USE_SRAM_MODEL
  if (sram_pending_r_) {
    if (sram_delay_r_ <= 1) {
      sram_load_fire_ = true;
      sram_pending_next_ = false;
      sram_delay_next_ = 0;
    } else {
      sram_delay_next_ = sram_delay_r_ - 1;
    }
  }
#endif

  // -------------------------------------------------------------------------
  // Allocate/merge MSHR for WAIT_MSHR_ALLOC slots (at most one per cycle)
  // Also allow it to resolve if cache got filled by prefetch.
  // -------------------------------------------------------------------------
  for (uint32_t n = 0; n < cfg_.rob_depth; ++n) {
    uint32_t idx = (rob_head_r_ + n) % cfg_.rob_depth;
    if (!rob_valid_r_[idx]) {
      continue;
    }
    RobState st = static_cast<RobState>(rob_state_r_[idx]);
    if (st != RobState::WAIT_MSHR_ALLOC) {
      continue;
    }
    uint32_t line_addr = rob_line_addr_r_[idx];
    uint32_t set = (line_addr >> offset_bits_) & (set_num_ - 1u);
    uint32_t ppn = get_ppn_from_paddr(line_addr);

    bool hit = false;
    uint32_t hit_way = 0;
    for (uint32_t way = 0; way < cfg_.ways; ++way) {
      if (cache_valid_at(set, way) && cache_tag_at(set, way) == ppn) {
        hit = true;
        hit_way = way;
        break;
      }
    }
    if (hit) {
      rob_state_next_[idx] = static_cast<uint8_t>(RobState::READY);
      for (uint32_t w = 0; w < word_num_; ++w) {
        rob_line_data_next_[static_cast<size_t>(idx) * word_num_ + w] =
            cache_data_at(set, hit_way, w);
      }
      break;
    }

    int mshr_idx = find_mshr_by_line_next(line_addr);
    if (mshr_idx >= 0) {
      mshr_waiter_set(static_cast<uint32_t>(mshr_idx), idx);
      rob_state_next_[idx] = static_cast<uint8_t>(RobState::WAIT_MSHR);
      rob_mshr_idx_next_[idx] = static_cast<uint32_t>(mshr_idx);
      break;
    }
    int free_idx = find_free_mshr_next();
    if (free_idx >= 0) {
      uint32_t mi = static_cast<uint32_t>(free_idx);
      mshr_state_next_[mi] = static_cast<uint8_t>(MshrState::ALLOC);
      mshr_line_addr_next_[mi] = line_addr;
      mshr_is_prefetch_next_[mi] = 0;
      mshr_txid_valid_next_[mi] = 0;
      for (uint32_t w = 0; w < waiters_words_; ++w) {
        mshr_waiters_next_[static_cast<size_t>(mi) * waiters_words_ + w] = 0;
      }
      mshr_waiter_set(mi, idx);
      rob_state_next_[idx] = static_cast<uint8_t>(RobState::WAIT_MSHR);
      rob_mshr_idx_next_[idx] = mi;
      break;
    }
  }

  // -------------------------------------------------------------------------
  // IFU request accept (non-blocking under miss; stall on translation miss)
  // -------------------------------------------------------------------------
  uint32_t rob_free =
      (cfg_.rob_depth > rob_count_r_) ? (cfg_.rob_depth - rob_count_r_) : 0;
  if (resp_fire && rob_free < cfg_.rob_depth) {
    rob_free += 1;
  }

  bool sram_block = false;
#if ICACHE_USE_SRAM_MODEL
  sram_block = sram_pending_r_;
#endif
  io.out.ifu_req_ready = (rob_free > 0) && stage2_ready_for_new && !sram_block;

  bool ifu_fire = io.in.ifu_req_valid && io.out.ifu_req_ready;
  bool lookup_load_fire = false;
  uint32_t new_rob_idx = 0;
  bool new_rob_valid = false;

  if (ifu_fire) {
    new_rob_idx = rob_tail_next_;
    new_rob_valid = true;
    rob_valid_next_[new_rob_idx] = 1;
    rob_pc_next_[new_rob_idx] = io.in.pc;
    rob_state_next_[new_rob_idx] = static_cast<uint8_t>(RobState::LOOKUP);
    rob_line_addr_next_[new_rob_idx] = 0;
    rob_mshr_idx_next_[new_rob_idx] = 0;

    rob_tail_next_ = (rob_tail_next_ + 1) % cfg_.rob_depth;
    rob_count_next_ = rob_count_next_ + 1;
  }

#if ICACHE_USE_SRAM_MODEL
  if (ifu_fire) {
    uint32_t latency = ICACHE_SRAM_FIXED_LATENCY;
#if ICACHE_SRAM_RANDOM_DELAY
    uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
    uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
    if (max_lat < min_lat) {
      max_lat = min_lat;
    }
    uint32_t seed = xorshift32(sram_seed_r_);
    sram_seed_next_ = seed;
    uint32_t range = max_lat - min_lat + 1;
    latency = min_lat + (seed % range);
#endif
    latency = clamp_latency(latency);
    if (latency <= 1) {
      sram_load_fire_ = true;
    } else {
      sram_pending_next_ = true;
      sram_delay_next_ = latency - 1;
      sram_index_next_ = pc_index;
      sram_pc_next_ = io.in.pc;
      sram_rob_idx_next_ = new_rob_idx;
    }
  }

  if (sram_load_fire_) {
    lookup_load_fire = true;
    set_load_fire_ = true;
    if (sram_pending_r_) {
      lookup_pc_next_ = sram_pc_r_;
      lookup_index_next_ = sram_index_r_;
      lookup_rob_idx_next_ = sram_rob_idx_r_;
    } else {
      lookup_pc_next_ = io.in.pc;
      lookup_index_next_ = pc_index;
      lookup_rob_idx_next_ = new_rob_valid ? new_rob_idx : lookup_rob_idx_r_;
    }
  }
#else
  if (ifu_fire) {
    lookup_load_fire = true;
    set_load_fire_ = true;
    lookup_pc_next_ = io.in.pc;
    lookup_index_next_ = pc_index;
    lookup_rob_idx_next_ = new_rob_idx;
  }
#endif

  if (lookup_load_fire) {
    lookup_valid_next_ = true;
  } else if (lookup_fire) {
    lookup_valid_next_ = false;
  } else {
    lookup_valid_next_ = lookup_valid_r_;
  }

  // -------------------------------------------------------------------------
  // MMU request generation
  // - mmu_ifu_resp is 1-cycle delayed (registered), so drive the request for
  //   the lookup that will be consumed in the *next* cycle.
  // Priority:
  //   1) New lookup loaded this cycle (for next-cycle stage2)
  //   2) Replay current lookup if waiting for translation
  //   3) Pre-request translation while SRAM delay counts down (single outstanding)
  // -------------------------------------------------------------------------
  if (lookup_load_fire) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = lookup_pc_next_ >> cfg_.page_offset_bits;
  } else if (lookup_valid_r_ && !io.in.ppn_valid) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = lookup_pc_r_ >> cfg_.page_offset_bits;
  } else if (sram_pending_next_) {
    io.out.mmu_req_valid = true;
    io.out.mmu_req_vtag = sram_pc_next_ >> cfg_.page_offset_bits;
  }

  // -------------------------------------------------------------------------
  // Memory response handling (always ready; drop unknown/canceled)
  // -------------------------------------------------------------------------
  if (io.in.mem_resp_valid) {
    uint8_t txid = static_cast<uint8_t>(io.in.mem_resp_id & 0xF);
    if (!txid_inflight_r_[txid]) {
      // Unknown response: drop
    } else if (txid_canceled_r_[txid] || !txid_mshr_valid_r_[txid]) {
      free_txid(txid);
    } else {
      uint32_t mshr_idx = txid_mshr_r_[txid];
      if (mshr_idx >= cfg_.mshr_num) {
        free_txid(txid);
      } else {
        uint32_t line_addr = mshr_line_addr_r_[mshr_idx];
        uint32_t set = (line_addr >> offset_bits_) & (set_num_ - 1u);
        uint32_t ppn = get_ppn_from_paddr(line_addr);

        uint32_t victim = choose_victim_way(set);

        // Cache fill is applied in seq (comb->seq)
        cache_fill_fire_ = true;
        cache_fill_set_ = set;
        cache_fill_way_ = victim;
        cache_fill_tag_ = ppn;
        for (uint32_t w = 0; w < word_num_; ++w) {
          cache_fill_words_[w] = io.in.mem_resp_data[w];
        }

        repl_on_access(set, victim);
        if (cfg_.repl == ReplPolicy::RR) {
          rr_ptr_next_[set] = (victim + 1) % cfg_.ways;
        }

        // Wake waiters (use *_next_ view)
        for (uint32_t rob_idx = 0; rob_idx < cfg_.rob_depth; ++rob_idx) {
          if (!mshr_waiter_test_next(mshr_idx, rob_idx)) {
            continue;
          }
          if (!rob_valid_next_[rob_idx]) {
            continue;
          }
          RobState st = static_cast<RobState>(rob_state_next_[rob_idx]);
          if (st != RobState::WAIT_MSHR && st != RobState::WAIT_MSHR_ALLOC) {
            continue;
          }
          rob_state_next_[rob_idx] = static_cast<uint8_t>(RobState::READY);
          for (uint32_t w = 0; w < word_num_; ++w) {
            rob_line_data_next_[static_cast<size_t>(rob_idx) * word_num_ + w] =
                io.in.mem_resp_data[w];
          }
        }

        // Free MSHR entry
        mshr_state_next_[mshr_idx] = static_cast<uint8_t>(MshrState::FREE);
        mshr_txid_valid_next_[mshr_idx] = 0;
        mshr_is_prefetch_next_[mshr_idx] = 0;
        mshr_line_addr_next_[mshr_idx] = 0;
        for (uint32_t w = 0; w < waiters_words_; ++w) {
          mshr_waiters_next_[static_cast<size_t>(mshr_idx) * waiters_words_ + w] =
              0;
        }

        free_txid(txid);
      }
    }
  }

  // -------------------------------------------------------------------------
  // Memory request scheduling (one per cycle, demand first)
  // Hold under backpressure (ready-first interconnect compliance).
  // -------------------------------------------------------------------------
  if (memreq_latched_valid_r_) {
    io.out.mem_req_valid = true;
    io.out.mem_req_addr = memreq_latched_addr_r_;
    io.out.mem_req_id = memreq_latched_id_r_;
    if (io.in.mem_req_ready) {
      uint32_t mi = memreq_latched_mshr_r_;
      if (mi < cfg_.mshr_num &&
          static_cast<MshrState>(mshr_state_next_[mi]) == MshrState::SENDING) {
        mshr_state_next_[mi] = static_cast<uint8_t>(MshrState::WAIT_RESP);
      }
      memreq_latched_valid_next_ = false;
    }
  } else {
    auto pick_mshr = [&](bool want_prefetch) -> int {
      for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
        if (static_cast<MshrState>(mshr_state_next_[i]) != MshrState::ALLOC) {
          continue;
        }
        bool is_pf = (mshr_is_prefetch_next_[i] != 0);
        if (is_pf != want_prefetch) {
          continue;
        }
        return static_cast<int>(i);
      }
      return -1;
    };

    int mi = pick_mshr(false);
    if (mi < 0) {
      mi = pick_mshr(true);
    }
    if (mi >= 0) {
      int txid = alloc_txid();
      if (txid >= 0) {
        uint32_t mshr_idx = static_cast<uint32_t>(mi);
        uint32_t addr = mshr_line_addr_next_[mshr_idx];
        io.out.mem_req_valid = true;
        io.out.mem_req_addr = addr;
        io.out.mem_req_id = static_cast<uint8_t>(txid & 0xF);

        // Bind txid -> mshr (unless later canceled by refetch)
        txid_mshr_next_[txid] = static_cast<uint8_t>(mshr_idx);
        txid_mshr_valid_next_[txid] = true;
        mshr_txid_next_[mshr_idx] = static_cast<uint8_t>(txid & 0xF);
        mshr_txid_valid_next_[mshr_idx] = 1;

        if (io.in.mem_req_ready) {
          mshr_state_next_[mshr_idx] = static_cast<uint8_t>(MshrState::WAIT_RESP);
        } else {
          memreq_latched_valid_next_ = true;
          memreq_latched_addr_next_ = addr;
          memreq_latched_id_next_ = static_cast<uint8_t>(txid & 0xF);
          memreq_latched_mshr_next_ = static_cast<uint8_t>(mshr_idx);
          mshr_state_next_[mshr_idx] = static_cast<uint8_t>(MshrState::SENDING);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // MSHR utilization tracking (peak occupancy)
  // -------------------------------------------------------------------------
  uint32_t mshr_used = 0;
  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    if (static_cast<MshrState>(mshr_state_next_[i]) != MshrState::FREE) {
      mshr_used++;
    }
  }
  if (mshr_used > mshr_peak_next_) {
    mshr_peak_next_ = mshr_used;
  }
}

void ICacheV2::seq() {
  // Apply stage1-set latch
  if (set_load_fire_) {
    for (uint32_t way = 0; way < cfg_.ways; ++way) {
      set_tag_r_[way] = set_tag_w_[way];
      set_valid_r_[way] = set_valid_w_[way];
      for (uint32_t w = 0; w < word_num_; ++w) {
        set_data_r_[static_cast<size_t>(way) * word_num_ + w] =
            set_data_w_[static_cast<size_t>(way) * word_num_ + w];
      }
    }
  }

  // Apply cache fill (single line)
  if (cache_fill_fire_) {
    cache_write_line(cache_fill_set_, cache_fill_way_, cache_fill_words_.data());
    cache_set_tag_valid(cache_fill_set_, cache_fill_way_, cache_fill_tag_, true);
  }

  epoch_r_ = epoch_next_;
  rr_ptr_r_ = rr_ptr_next_;
  plru_bits_r_ = plru_bits_next_;
  rand_seed_r_ = rand_seed_next_;

  rob_head_r_ = rob_head_next_;
  rob_tail_r_ = rob_tail_next_;
  rob_count_r_ = rob_count_next_;
  rob_valid_r_ = rob_valid_next_;
  rob_pc_r_ = rob_pc_next_;
  rob_state_r_ = rob_state_next_;
  rob_line_addr_r_ = rob_line_addr_next_;
  rob_mshr_idx_r_ = rob_mshr_idx_next_;
  rob_line_data_r_ = rob_line_data_next_;

  lookup_valid_r_ = lookup_valid_next_;
  lookup_pc_r_ = lookup_pc_next_;
  lookup_index_r_ = lookup_index_next_;
  lookup_rob_idx_r_ = lookup_rob_idx_next_;

  sram_pending_r_ = sram_pending_next_;
  sram_delay_r_ = sram_delay_next_;
  sram_index_r_ = sram_index_next_;
  sram_pc_r_ = sram_pc_next_;
  sram_rob_idx_r_ = sram_rob_idx_next_;
  sram_seed_r_ = sram_seed_next_;

  mshr_state_r_ = mshr_state_next_;
  mshr_line_addr_r_ = mshr_line_addr_next_;
  mshr_is_prefetch_r_ = mshr_is_prefetch_next_;
  mshr_txid_r_ = mshr_txid_next_;
  mshr_txid_valid_r_ = mshr_txid_valid_next_;
  mshr_waiters_r_ = mshr_waiters_next_;
  mshr_peak_r_ = mshr_peak_next_;

  for (int i = 0; i < 16; ++i) {
    txid_inflight_r_[i] = txid_inflight_next_[i];
    txid_canceled_r_[i] = txid_canceled_next_[i];
    txid_mshr_r_[i] = txid_mshr_next_[i];
    txid_mshr_valid_r_[i] = txid_mshr_valid_next_[i];
  }
  txid_rr_r_ = txid_rr_next_;

  memreq_latched_valid_r_ = memreq_latched_valid_next_;
  memreq_latched_addr_r_ = memreq_latched_addr_next_;
  memreq_latched_id_r_ = memreq_latched_id_next_;
  memreq_latched_mshr_r_ = memreq_latched_mshr_next_;
}

void ICacheV2::log_debug() const {
  auto rob_state_name = [](RobState st) -> const char * {
    switch (st) {
    case RobState::EMPTY:
      return "EMPTY";
    case RobState::LOOKUP:
      return "LOOKUP";
    case RobState::WAIT_TLB:
      return "WAIT_TLB";
    case RobState::WAIT_MSHR_ALLOC:
      return "WAIT_MSHR_ALLOC";
    case RobState::WAIT_MSHR:
      return "WAIT_MSHR";
    case RobState::READY:
      return "READY";
    case RobState::READY_FAULT:
      return "READY_FAULT";
    case RobState::KILLED:
      return "KILLED";
    default:
      return "UNKNOWN";
    }
  };

  auto mshr_state_name = [](MshrState st) -> const char * {
    switch (st) {
    case MshrState::FREE:
      return "FREE";
    case MshrState::ALLOC:
      return "ALLOC";
    case MshrState::SENDING:
      return "SENDING";
    case MshrState::WAIT_RESP:
      return "WAIT_RESP";
    default:
      return "UNKNOWN";
    }
  };

  std::cout << "=== icache_v2 debug ===" << std::endl;
  std::cout << "  t=" << std::dec << sim_time << " epoch=" << epoch_r_
            << " lookup_v=" << (lookup_valid_r_ ? 1 : 0)
            << " lookup_pc=0x" << std::hex << lookup_pc_r_ << std::dec
            << " rob_head=" << rob_head_r_ << " rob_tail=" << rob_tail_r_
            << " rob_count=" << rob_count_r_ << std::endl;

  if (rob_count_r_ > 0 && rob_head_r_ < cfg_.rob_depth && rob_valid_r_[rob_head_r_]) {
    RobState st = static_cast<RobState>(rob_state_r_[rob_head_r_]);
    std::cout << "  rob_head: idx=" << rob_head_r_ << " pc=0x" << std::hex
              << rob_pc_r_[rob_head_r_] << std::dec
              << " state=" << rob_state_name(st)
              << " line=0x" << std::hex << rob_line_addr_r_[rob_head_r_] << std::dec
              << " mshr=" << rob_mshr_idx_r_[rob_head_r_] << std::endl;
  } else {
    std::cout << "  rob_head: (empty)" << std::endl;
  }

  uint32_t dump_n = rob_count_r_;
  if (dump_n > 8) {
    dump_n = 8;
  }
  for (uint32_t n = 0; n < dump_n; ++n) {
    uint32_t idx = (rob_head_r_ + n) % cfg_.rob_depth;
    if (!rob_valid_r_[idx]) {
      continue;
    }
    RobState st = static_cast<RobState>(rob_state_r_[idx]);
    std::cout << "  rob[" << idx << "]: pc=0x" << std::hex << rob_pc_r_[idx]
              << std::dec << " state=" << rob_state_name(st)
              << " line=0x" << std::hex << rob_line_addr_r_[idx] << std::dec
              << " mshr=" << rob_mshr_idx_r_[idx] << std::endl;
  }

  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    MshrState st = static_cast<MshrState>(mshr_state_r_[i]);
    if (st == MshrState::FREE) {
      continue;
    }
    std::cout << "  mshr[" << i << "]: state=" << mshr_state_name(st)
              << " line=0x" << std::hex << mshr_line_addr_r_[i] << std::dec
              << " pf=" << static_cast<int>(mshr_is_prefetch_r_[i])
              << " txid_v=" << static_cast<int>(mshr_txid_valid_r_[i])
              << " txid=" << static_cast<int>(mshr_txid_r_[i] & 0xF) << std::endl;
  }

  uint32_t inflight_cnt = 0;
  for (int id = 0; id < 16; ++id) {
    if (txid_inflight_r_[id]) {
      inflight_cnt++;
    }
  }
  std::cout << "  txid_inflight=" << inflight_cnt
            << " memreq_latched=" << (memreq_latched_valid_r_ ? 1 : 0)
            << std::endl;
  if (memreq_latched_valid_r_) {
    std::cout << "    latched: addr=0x" << std::hex << memreq_latched_addr_r_
              << std::dec << " id=" << static_cast<int>(memreq_latched_id_r_ & 0xF)
              << " mshr=" << static_cast<int>(memreq_latched_mshr_r_) << std::endl;
  }
}

void ICacheV2::perf_print() const {
  double ratio = 0.0;
  if (cfg_.mshr_num != 0) {
    ratio = static_cast<double>(mshr_peak_r_) / static_cast<double>(cfg_.mshr_num);
  }
  std::cout << "[icache_v2] mshr_peak: " << mshr_peak_r_ << "/" << cfg_.mshr_num
            << " (" << ratio << ")" << std::endl;
}

} // namespace icache_module_v2_n
