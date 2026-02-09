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
inline uint32_t clamp_latency(uint32_t value) {
  return (value < 1u) ? 1u : value;
}

inline bool lookup_latency_enabled_v2() {
  return (ICACHE_USE_SRAM_MODEL != 0) &&
         ((ICACHE_SRAM_RANDOM_DELAY != 0) || (ICACHE_SRAM_FIXED_LATENCY > 0));
}
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

  // Generalized-IO bitvector structs are compile-time sized by ICACHE_V2_*.
  // Keep runtime config within those compile-time bounds.
  if (cfg_.ways > ICACHE_V2_WAYS) {
    std::cerr << "[icache_v2] cfg.ways exceeds ICACHE_V2_WAYS" << std::endl;
    std::exit(1);
  }
  if (cfg_.mshr_num > ICACHE_V2_MSHR_NUM) {
    std::cerr << "[icache_v2] cfg.mshr_num exceeds ICACHE_V2_MSHR_NUM"
              << std::endl;
    std::exit(1);
  }
  if (cfg_.rob_depth > ICACHE_V2_ROB_DEPTH) {
    std::cerr << "[icache_v2] cfg.rob_depth exceeds ICACHE_V2_ROB_DEPTH"
              << std::endl;
    std::exit(1);
  }
  if (set_num_ > ICACHE_V2_SET_NUM) {
    std::cerr << "[icache_v2] cfg.page_offset_bits/set_num exceeds "
                 "ICACHE_V2_SET_NUM bound"
              << std::endl;
    std::exit(1);
  }
  if (word_num_ > ICACHE_V2_WORD_NUM) {
    std::cerr << "[icache_v2] cfg.line_bytes/word_num exceeds ICACHE_V2_WORD_NUM"
              << std::endl;
    std::exit(1);
  }
  if (waiters_words_ > ICACHE_V2_WAITER_WORDS) {
    std::cerr << "[icache_v2] cfg.rob_depth waiters_words exceeds "
                 "ICACHE_V2_WAITER_WORDS"
              << std::endl;
    std::exit(1);
  }

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

void ICacheV2::lookup_read_set(uint32_t set) {
  bool lookup_from_input =
      (ICACHE_V2_LOOKUP_FROM_INPUT != 0) || lookup_latency_enabled_v2();
  if (lookup_from_input) {
    bool resp_valid = io.lookup_in.lookup_resp_valid;
    for (uint32_t way = 0; way < cfg_.ways; ++way) {
      set_tag_w_[way] = io.lookup_in.lookup_set_tag[way];
      bool valid = io.lookup_in.lookup_set_valid[way] && resp_valid;
      set_valid_w_[way] = valid ? 1 : 0;
      for (uint32_t w = 0; w < word_num_; ++w) {
        set_data_w_[static_cast<size_t>(way) * word_num_ + w] =
            io.lookup_in.lookup_set_data[way][w];
      }
    }
    return;
  }

  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    set_tag_w_[way] = cache_tag_at(set, way);
    set_valid_w_[way] = cache_valid_at(set, way) ? 1 : 0;
    for (uint32_t w = 0; w < word_num_; ++w) {
      set_data_w_[static_cast<size_t>(way) * word_num_ + w] =
          cache_data_at(set, way, w);
    }
  }
}

void ICacheV2::export_lookup_set_for_pc(
    uint32_t pc, uint32_t out_data[ICACHE_V2_WAYS][ICACHE_LINE_SIZE / 4],
    uint32_t out_tag[ICACHE_V2_WAYS], bool out_valid[ICACHE_V2_WAYS]) const {
  for (uint32_t way = 0; way < ICACHE_V2_WAYS; ++way) {
    out_tag[way] = 0;
    out_valid[way] = false;
    for (uint32_t w = 0; w < (ICACHE_LINE_SIZE / 4); ++w) {
      out_data[way][w] = 0;
    }
  }

  uint32_t pc_index = get_index_from_pc(pc);
  uint32_t rd_index = pc_index;

  uint32_t ways = cfg_.ways;
  if (ways > ICACHE_V2_WAYS) {
    ways = ICACHE_V2_WAYS;
  }
  uint32_t words = word_num_;
  if (words > (ICACHE_LINE_SIZE / 4)) {
    words = (ICACHE_LINE_SIZE / 4);
  }

  for (uint32_t way = 0; way < ways; ++way) {
    out_tag[way] = cache_tag_at(rd_index, way);
    out_valid[way] = cache_valid_at(rd_index, way);
    for (uint32_t w = 0; w < words; ++w) {
      out_data[way][w] = cache_data_at(rd_index, way, w);
    }
  }
}

bool ICacheV2::lookup(uint32_t pc_index, bool ifu_fire, uint32_t new_rob_idx) {
  bool lookup_load_fire = false;
  uint32_t load_pc = io.in.pc;
  uint32_t load_index = pc_index;
  uint32_t load_rob_idx = new_rob_idx;

  if (!lookup_latency_enabled_v2()) {
    if (ifu_fire) {
      lookup_load_fire = true;
    }
  } else {
    if (ifu_fire && !sram_pending_r_) {
      sram_pending_next_ = true;
      sram_pc_next_ = io.in.pc;
      sram_index_next_ = pc_index;
      sram_rob_idx_next_ = new_rob_idx;
    }

    if (sram_pending_r_ && io.lookup_in.lookup_resp_valid) {
      lookup_load_fire = true;
      load_pc = sram_pc_r_;
      load_index = sram_index_r_;
      load_rob_idx = sram_rob_idx_r_;
      sram_pending_next_ = false;
    }
  }

  if (lookup_load_fire) {
    lookup_read_set(load_index);
    set_load_fire_ = true;
    lookup_pc_next_ = load_pc;
    lookup_index_next_ = load_index;
    lookup_rob_idx_next_ = load_rob_idx;
  }

  return lookup_load_fire;
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
#if !ICACHE_V2_LOOKUP_FROM_INPUT
  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    if (!cache_valid_at(set, way)) {
      return way;
    }
  }
#endif

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

void ICacheV2::load_regs_from_io() {
  const auto &r = io.regs;

  epoch_r_ = r.epoch_r;
  for (uint32_t set = 0; set < set_num_; ++set) {
    rr_ptr_r_[set] = r.rr_ptr_r[set];
  }
  if (!plru_bits_r_.empty()) {
    for (uint32_t set = 0; set < set_num_; ++set) {
      for (uint32_t bit = 0; bit < (cfg_.ways - 1); ++bit) {
        plru_bits_r_[static_cast<size_t>(set) * (cfg_.ways - 1) + bit] =
            r.plru_bits_r[set][bit];
      }
    }
  }
  rand_seed_r_ = r.rand_seed_r;

  rob_head_r_ = (cfg_.rob_depth == 0) ? 0 : (r.rob_head_r % cfg_.rob_depth);
  rob_tail_r_ = (cfg_.rob_depth == 0) ? 0 : (r.rob_tail_r % cfg_.rob_depth);
  rob_count_r_ = (r.rob_count_r > cfg_.rob_depth) ? cfg_.rob_depth : r.rob_count_r;
  for (uint32_t i = 0; i < cfg_.rob_depth; ++i) {
    rob_valid_r_[i] = r.rob_valid_r[i] ? 1 : 0;
    rob_pc_r_[i] = r.rob_pc_r[i];
    rob_state_r_[i] = r.rob_state_r[i];
    rob_line_addr_r_[i] = r.rob_line_addr_r[i];
    rob_mshr_idx_r_[i] = r.rob_mshr_idx_r[i];
    for (uint32_t w = 0; w < word_num_; ++w) {
      rob_line_data_r_[static_cast<size_t>(i) * word_num_ + w] =
          r.rob_line_data_r[i][w];
    }
  }

  lookup_valid_r_ = r.lookup_valid_r;
  lookup_pc_r_ = r.lookup_pc_r;
  lookup_index_r_ = (set_num_ == 0) ? 0 : (r.lookup_index_r % set_num_);
  lookup_rob_idx_r_ =
      (cfg_.rob_depth == 0) ? 0 : (r.lookup_rob_idx_r % cfg_.rob_depth);
  sram_pending_r_ = r.sram_pending_r;
  sram_pc_r_ = r.sram_pc_r;
  sram_index_r_ = (set_num_ == 0) ? 0 : (r.sram_index_r % set_num_);
  sram_rob_idx_r_ =
      (cfg_.rob_depth == 0) ? 0 : (r.sram_rob_idx_r % cfg_.rob_depth);
  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    set_tag_r_[way] = r.set_tag_r[way];
    set_valid_r_[way] = r.set_valid_r[way] ? 1 : 0;
    for (uint32_t w = 0; w < word_num_; ++w) {
      set_data_r_[static_cast<size_t>(way) * word_num_ + w] =
          r.set_data_r[way][w];
    }
  }

  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    mshr_state_r_[i] = r.mshr_state_r[i];
    mshr_line_addr_r_[i] = r.mshr_line_addr_r[i];
    mshr_is_prefetch_r_[i] = r.mshr_is_prefetch_r[i];
    mshr_txid_r_[i] = r.mshr_txid_r[i] & 0xF;
    mshr_txid_valid_r_[i] = r.mshr_txid_valid_r[i] ? 1 : 0;
    for (uint32_t w = 0; w < waiters_words_; ++w) {
      mshr_waiters_r_[static_cast<size_t>(i) * waiters_words_ + w] =
          r.mshr_waiters_r[i][w];
    }
  }

  for (int id = 0; id < 16; ++id) {
    txid_inflight_r_[id] = r.txid_inflight_r[id];
    txid_canceled_r_[id] = r.txid_canceled_r[id];
    txid_mshr_r_[id] =
        static_cast<uint8_t>((cfg_.mshr_num == 0) ? 0 : (r.txid_mshr_r[id] % cfg_.mshr_num));
    txid_mshr_valid_r_[id] = r.txid_mshr_valid_r[id];
  }
  txid_rr_r_ = static_cast<uint8_t>(r.txid_rr_r & 0xF);

  memreq_latched_valid_r_ = r.memreq_latched_valid_r;
  memreq_latched_addr_r_ = r.memreq_latched_addr_r;
  memreq_latched_id_r_ = static_cast<uint8_t>(r.memreq_latched_id_r & 0xF);
  memreq_latched_mshr_r_ = static_cast<uint8_t>(
      (cfg_.mshr_num == 0) ? 0 : (r.memreq_latched_mshr_r % cfg_.mshr_num));
}

void ICacheV2::sync_regs_to_io() {
  auto &r = io.regs;
  r = {};

  r.epoch_r = epoch_r_;
  for (uint32_t set = 0; set < set_num_; ++set) {
    r.rr_ptr_r[set] = rr_ptr_r_[set];
  }
  if (!plru_bits_r_.empty()) {
    for (uint32_t set = 0; set < set_num_; ++set) {
      for (uint32_t bit = 0; bit < (cfg_.ways - 1); ++bit) {
        r.plru_bits_r[set][bit] =
            plru_bits_r_[static_cast<size_t>(set) * (cfg_.ways - 1) + bit];
      }
    }
  }
  r.rand_seed_r = rand_seed_r_;

  r.rob_head_r = rob_head_r_;
  r.rob_tail_r = rob_tail_r_;
  r.rob_count_r = rob_count_r_;
  for (uint32_t i = 0; i < cfg_.rob_depth; ++i) {
    r.rob_valid_r[i] = rob_valid_r_[i] != 0;
    r.rob_pc_r[i] = rob_pc_r_[i];
    r.rob_state_r[i] = rob_state_r_[i];
    r.rob_line_addr_r[i] = rob_line_addr_r_[i];
    r.rob_mshr_idx_r[i] = rob_mshr_idx_r_[i];
    for (uint32_t w = 0; w < word_num_; ++w) {
      r.rob_line_data_r[i][w] =
          rob_line_data_r_[static_cast<size_t>(i) * word_num_ + w];
    }
  }

  r.lookup_valid_r = lookup_valid_r_;
  r.lookup_pc_r = lookup_pc_r_;
  r.lookup_index_r = lookup_index_r_;
  r.lookup_rob_idx_r = lookup_rob_idx_r_;
  r.sram_pending_r = sram_pending_r_;
  r.sram_pc_r = sram_pc_r_;
  r.sram_index_r = sram_index_r_;
  r.sram_rob_idx_r = sram_rob_idx_r_;
  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    r.set_tag_r[way] = set_tag_r_[way];
    r.set_valid_r[way] = set_valid_r_[way] != 0;
    for (uint32_t w = 0; w < word_num_; ++w) {
      r.set_data_r[way][w] = set_data_r_[static_cast<size_t>(way) * word_num_ + w];
    }
  }

  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    r.mshr_state_r[i] = mshr_state_r_[i];
    r.mshr_line_addr_r[i] = mshr_line_addr_r_[i];
    r.mshr_is_prefetch_r[i] = mshr_is_prefetch_r_[i];
    r.mshr_txid_r[i] = mshr_txid_r_[i];
    r.mshr_txid_valid_r[i] = mshr_txid_valid_r_[i];
    for (uint32_t w = 0; w < waiters_words_; ++w) {
      r.mshr_waiters_r[i][w] =
          mshr_waiters_r_[static_cast<size_t>(i) * waiters_words_ + w];
    }
  }

  for (int id = 0; id < 16; ++id) {
    r.txid_inflight_r[id] = txid_inflight_r_[id];
    r.txid_canceled_r[id] = txid_canceled_r_[id];
    r.txid_mshr_r[id] = txid_mshr_r_[id];
    r.txid_mshr_valid_r[id] = txid_mshr_valid_r_[id];
  }
  r.txid_rr_r = txid_rr_r_;

  r.memreq_latched_valid_r = memreq_latched_valid_r_;
  r.memreq_latched_addr_r = memreq_latched_addr_r_;
  r.memreq_latched_id_r = memreq_latched_id_r_;
  r.memreq_latched_mshr_r = memreq_latched_mshr_r_;
}

void ICacheV2::publish_reg_write_from_next() {
  auto &rw = io.reg_write;
  rw = {};

  rw.epoch_r = epoch_next_;
  for (uint32_t set = 0; set < set_num_; ++set) {
    rw.rr_ptr_r[set] = rr_ptr_next_[set];
  }
  if (!plru_bits_next_.empty()) {
    for (uint32_t set = 0; set < set_num_; ++set) {
      for (uint32_t bit = 0; bit < (cfg_.ways - 1); ++bit) {
        rw.plru_bits_r[set][bit] =
            plru_bits_next_[static_cast<size_t>(set) * (cfg_.ways - 1) + bit];
      }
    }
  }
  rw.rand_seed_r = rand_seed_next_;

  rw.rob_head_r = rob_head_next_;
  rw.rob_tail_r = rob_tail_next_;
  rw.rob_count_r = rob_count_next_;
  for (uint32_t i = 0; i < cfg_.rob_depth; ++i) {
    rw.rob_valid_r[i] = rob_valid_next_[i] != 0;
    rw.rob_pc_r[i] = rob_pc_next_[i];
    rw.rob_state_r[i] = rob_state_next_[i];
    rw.rob_line_addr_r[i] = rob_line_addr_next_[i];
    rw.rob_mshr_idx_r[i] = rob_mshr_idx_next_[i];
    for (uint32_t w = 0; w < word_num_; ++w) {
      rw.rob_line_data_r[i][w] =
          rob_line_data_next_[static_cast<size_t>(i) * word_num_ + w];
    }
  }

  rw.lookup_valid_r = lookup_valid_next_;
  rw.lookup_pc_r = lookup_pc_next_;
  rw.lookup_index_r = lookup_index_next_;
  rw.lookup_rob_idx_r = lookup_rob_idx_next_;
  rw.sram_pending_r = sram_pending_next_;
  rw.sram_pc_r = sram_pc_next_;
  rw.sram_index_r = sram_index_next_;
  rw.sram_rob_idx_r = sram_rob_idx_next_;
  for (uint32_t way = 0; way < cfg_.ways; ++way) {
    rw.set_tag_r[way] = set_load_fire_ ? set_tag_w_[way] : set_tag_r_[way];
    rw.set_valid_r[way] =
        (set_load_fire_ ? set_valid_w_[way] : set_valid_r_[way]) != 0;
    for (uint32_t w = 0; w < word_num_; ++w) {
      rw.set_data_r[way][w] =
          set_load_fire_
              ? set_data_w_[static_cast<size_t>(way) * word_num_ + w]
              : set_data_r_[static_cast<size_t>(way) * word_num_ + w];
    }
  }

  for (uint32_t i = 0; i < cfg_.mshr_num; ++i) {
    rw.mshr_state_r[i] = mshr_state_next_[i];
    rw.mshr_line_addr_r[i] = mshr_line_addr_next_[i];
    rw.mshr_is_prefetch_r[i] = mshr_is_prefetch_next_[i];
    rw.mshr_txid_r[i] = mshr_txid_next_[i];
    rw.mshr_txid_valid_r[i] = mshr_txid_valid_next_[i];
    for (uint32_t w = 0; w < waiters_words_; ++w) {
      rw.mshr_waiters_r[i][w] =
          mshr_waiters_next_[static_cast<size_t>(i) * waiters_words_ + w];
    }
  }

  for (int id = 0; id < 16; ++id) {
    rw.txid_inflight_r[id] = txid_inflight_next_[id];
    rw.txid_canceled_r[id] = txid_canceled_next_[id];
    rw.txid_mshr_r[id] = txid_mshr_next_[id];
    rw.txid_mshr_valid_r[id] = txid_mshr_valid_next_[id];
  }
  rw.txid_rr_r = txid_rr_next_;

  rw.memreq_latched_valid_r = memreq_latched_valid_next_;
  rw.memreq_latched_addr_r = memreq_latched_addr_next_;
  rw.memreq_latched_id_r = memreq_latched_id_next_;
  rw.memreq_latched_mshr_r = memreq_latched_mshr_next_;
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
#if ICACHE_V2_GENERALIZED_IO_MODE
  sync_regs_to_io();
#endif
}

void ICacheV2::reset() {
  epoch_r_ = 0;
  epoch_next_ = 0;

  mshr_peak_r_ = 0;
  mshr_peak_next_ = 0;
  txid_peak_r_ = 0;
  txid_peak_next_ = 0;

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
  sram_pending_r_ = false;
  sram_pending_next_ = false;
  sram_pc_r_ = 0;
  sram_pc_next_ = 0;
  sram_index_r_ = 0;
  sram_index_next_ = 0;
  sram_rob_idx_r_ = 0;
  sram_rob_idx_next_ = 0;

  std::fill(set_data_r_.begin(), set_data_r_.end(), 0);
  std::fill(set_tag_r_.begin(), set_tag_r_.end(), 0);
  std::fill(set_valid_r_.begin(), set_valid_r_.end(), 0);
  set_load_fire_ = false;
  cache_fill_fire_ = false;

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

  if (lookup_latency_enabled_v2()) {
    if (ICACHE_SRAM_RANDOM_DELAY != 0) {
      uint32_t min_lat = clamp_latency(ICACHE_SRAM_RANDOM_MIN);
      uint32_t max_lat = ICACHE_SRAM_RANDOM_MAX;
      if (max_lat < min_lat) {
        max_lat = min_lat;
      }
      std::cout << "[icache_v2] SRAM model: random latency " << min_lat << "-"
                << max_lat << " cycles" << std::endl;
    } else {
      std::cout << "[icache_v2] SRAM model: fixed latency "
                << clamp_latency(ICACHE_SRAM_FIXED_LATENCY) << " cycles"
                << std::endl;
    }
  } else {
    std::cout << "[icache_v2] SRAM model: disabled (register lookup)"
              << std::endl;
  }
  std::cout << "[icache_v2] ways=" << cfg_.ways << " sets=" << set_num_
            << " mshr=" << cfg_.mshr_num << " rob=" << cfg_.rob_depth
            << " repl=" << static_cast<int>(cfg_.repl)
            << " prefetch=" << (cfg_.prefetch_enable ? 1 : 0) << std::endl;

  io.out = {};
  io.out.mem_resp_ready = true;
  io.table_write = {};
  io.reg_write = {};
#if ICACHE_V2_GENERALIZED_IO_MODE
  sync_regs_to_io();
#endif
}

void ICacheV2::comb() {
#if ICACHE_V2_GENERALIZED_IO_MODE
  load_regs_from_io();
#endif

  io.out = {};
  io.out.mem_resp_ready = true; // always drain
  io.table_write = {};
  io.reg_write = {};
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
  sram_pc_next_ = sram_pc_r_;
  sram_index_next_ = sram_index_r_;
  sram_rob_idx_next_ = sram_rob_idx_r_;

  mshr_state_next_ = mshr_state_r_;
  mshr_line_addr_next_ = mshr_line_addr_r_;
  mshr_is_prefetch_next_ = mshr_is_prefetch_r_;
  mshr_txid_next_ = mshr_txid_r_;
  mshr_txid_valid_next_ = mshr_txid_valid_r_;
  mshr_waiters_next_ = mshr_waiters_r_;

  mshr_peak_next_ = mshr_peak_r_;
  txid_peak_next_ = txid_peak_r_;

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

  // Refetch kill path (existing behavior).
  const bool kill_pipe = io.in.refetch;
  const bool strict_lookup_from_input = (ICACHE_V2_LOOKUP_FROM_INPUT != 0);
  if (kill_pipe) {
    epoch_next_ = epoch_r_ + 1;

    // If a memory response arrives in the kill cycle, we will drop the data.
    // Make sure to free the txid now; otherwise, the interconnect may consume
    // the response (ready=1) and the txid would leak.
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
    //   immediately (otherwise kill can leak all txids over time).
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
    sram_pc_next_ = 0;
    sram_index_next_ = 0;
    sram_rob_idx_next_ = 0;
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
#if ICACHE_V2_GENERALIZED_IO_MODE
    publish_reg_write_from_next();
#endif
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
    if (strict_lookup_from_input) {
      return false;
    }
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
  // (Actual set read is done in lookup() after ifu_fire is known.)

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
                if (cfg_.prefetch_enable && !strict_lookup_from_input) {
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
    if (!strict_lookup_from_input) {
      for (uint32_t way = 0; way < cfg_.ways; ++way) {
        if (cache_valid_at(set, way) && cache_tag_at(set, way) == ppn) {
          hit = true;
          hit_way = way;
          break;
        }
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

  io.out.ifu_req_ready =
      (rob_free > 0) && stage2_ready_for_new &&
      (!lookup_latency_enabled_v2() || !sram_pending_r_);

  bool ifu_fire = io.in.ifu_req_valid && io.out.ifu_req_ready;
  bool lookup_load_fire = false;
  uint32_t new_rob_idx = 0;

  if (ifu_fire) {
    new_rob_idx = rob_tail_next_;
    rob_valid_next_[new_rob_idx] = 1;
    rob_pc_next_[new_rob_idx] = io.in.pc;
    rob_state_next_[new_rob_idx] = static_cast<uint8_t>(RobState::LOOKUP);
    rob_line_addr_next_[new_rob_idx] = 0;
    rob_mshr_idx_next_[new_rob_idx] = 0;

    rob_tail_next_ = (rob_tail_next_ + 1) % cfg_.rob_depth;
    rob_count_next_ = rob_count_next_ + 1;
  }

  lookup_load_fire = lookup(pc_index, ifu_fire, new_rob_idx);

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

  // -------------------------------------------------------------------------
  // TXID utilization tracking (peak in-flight txids)
  // Note: This reflects actual memory-level concurrency (bounded by 16 IDs),
  // whereas MSHR occupancy can be larger and includes ALLOC/SENDING entries.
  // -------------------------------------------------------------------------
  uint32_t txid_used = 0;
  for (int id = 0; id < 16; ++id) {
    if (txid_inflight_next_[id]) {
      txid_used++;
    }
  }
  if (txid_used > txid_peak_next_) {
    txid_peak_next_ = txid_used;
  }

  if (cache_fill_fire_) {
    io.table_write.we = true;
    io.table_write.set = cache_fill_set_;
    io.table_write.way = cache_fill_way_;
    io.table_write.tag = cache_fill_tag_;
    io.table_write.valid = true;
    for (uint32_t w = 0; w < word_num_; ++w) {
      io.table_write.data[w] = cache_fill_words_[w];
    }
  }

#if ICACHE_V2_GENERALIZED_IO_MODE
  publish_reg_write_from_next();
#endif
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

  // fence.i visibility point: invalidate cache lines at seq boundary.
  if (io.in.flush) {
    std::fill(cache_valid_.begin(), cache_valid_.end(), 0);
  }

  // Apply cache fill (single line) via explicit table-write output.
  if (io.table_write.we && !io.in.flush) {
    cache_write_line(io.table_write.set, io.table_write.way, io.table_write.data);
    cache_set_tag_valid(io.table_write.set, io.table_write.way, io.table_write.tag,
                        io.table_write.valid);
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
  sram_pc_r_ = sram_pc_next_;
  sram_index_r_ = sram_index_next_;
  sram_rob_idx_r_ = sram_rob_idx_next_;

  mshr_state_r_ = mshr_state_next_;
  mshr_line_addr_r_ = mshr_line_addr_next_;
  mshr_is_prefetch_r_ = mshr_is_prefetch_next_;
  mshr_txid_r_ = mshr_txid_next_;
  mshr_txid_valid_r_ = mshr_txid_valid_next_;
  mshr_waiters_r_ = mshr_waiters_next_;
  mshr_peak_r_ = mshr_peak_next_;
  txid_peak_r_ = txid_peak_next_;

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

#if ICACHE_V2_GENERALIZED_IO_MODE
  sync_regs_to_io();
#endif
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

  double tx_ratio = static_cast<double>(txid_peak_r_) / 16.0;
  std::cout << "[icache_v2] txid_peak: " << txid_peak_r_ << "/16"
            << " (" << tx_ratio << ")" << std::endl;
}

} // namespace icache_module_v2_n
