/*
 * Non-blocking I-Cache v2 (MSHR + Prefetch + Configurable Replacement)
 *
 * Key points:
 * - Preserve two lookup models: register-style and SRAM-style (latency) via
 *   the existing ICACHE_USE_SRAM_MODEL macro family (same as icache_module.h).
 * - Add MSHR (configurable) and a simple next-line prefetcher (configurable).
 * - Add configurable replacement, including generic N-way Tree-PLRU (ways must
 *   be power-of-two when PLRU is selected).
 * - Provide memory transaction ID (mem_req_id/mem_resp_id).
 *
 * Notes:
 * - The external memory data width is fixed to one cacheline
 *   (ICACHE_LINE_SIZE bytes, default 32B).
 * - The AXI interconnect upstream ID is 4-bit; this module keeps a 0..15
 *   txid pool for in-flight transactions.
 */
#ifndef ICACHE_MODULE_V2_H
#define ICACHE_MODULE_V2_H

#include <cstdint>
#include <vector>

#include <frontend.h>

// -----------------------------------------------------------------------------
// ICacheV2 default configuration knobs (compile-time)
// -----------------------------------------------------------------------------
// These macros allow benchmarking different ICacheV2Config settings without
// patching C++ sources (e.g., from Python experiment scripts via -D...).
//
// Repl policy encoding (ICACHE_V2_REPL_POLICY):
//   0 = RR, 1 = RANDOM, 2 = PLRU
#ifndef ICACHE_V2_PAGE_OFFSET_BITS
#define ICACHE_V2_PAGE_OFFSET_BITS 12
#endif
#ifndef ICACHE_V2_WAYS
#define ICACHE_V2_WAYS 8
#endif
#ifndef ICACHE_V2_MSHR_NUM
#define ICACHE_V2_MSHR_NUM 4
#endif
#ifndef ICACHE_V2_ROB_DEPTH
#define ICACHE_V2_ROB_DEPTH 16
#endif
#ifndef ICACHE_V2_PREFETCH_ENABLE
#define ICACHE_V2_PREFETCH_ENABLE 1
#endif
#ifndef ICACHE_V2_PREFETCH_DISTANCE
#define ICACHE_V2_PREFETCH_DISTANCE 1
#endif
#ifndef ICACHE_V2_PREFETCH_RESERVE
#define ICACHE_V2_PREFETCH_RESERVE 1
#endif
#ifndef ICACHE_V2_REPL_POLICY
#define ICACHE_V2_REPL_POLICY 2
#endif

namespace icache_module_v2_n {

enum class ReplPolicy : uint8_t {
  RR = 0,
  RANDOM = 1,
  PLRU = 2,
};

struct ICacheV2Config {
  uint32_t line_bytes = ICACHE_LINE_SIZE; // must match external interface
  uint32_t page_offset_bits = ICACHE_V2_PAGE_OFFSET_BITS; // 4KB pages (Sv32)

  uint32_t ways = ICACHE_V2_WAYS;
  uint32_t mshr_num = ICACHE_V2_MSHR_NUM;
  uint32_t rob_depth = ICACHE_V2_ROB_DEPTH;

  bool prefetch_enable = (ICACHE_V2_PREFETCH_ENABLE != 0);
  uint32_t prefetch_distance = ICACHE_V2_PREFETCH_DISTANCE; // next-line
  uint32_t prefetch_reserve =
      ICACHE_V2_PREFETCH_RESERVE; // reserve MSHR slots for demand

  ReplPolicy repl = static_cast<ReplPolicy>(ICACHE_V2_REPL_POLICY);
};

struct ICacheV2_in_t {
  // Input from IFU
  uint32_t pc = 0;
  bool ifu_req_valid = false;
  bool ifu_resp_ready = true;
  bool refetch = false;

  // Input from MMU
  uint32_t ppn = 0;
  bool ppn_valid = false;
  bool page_fault = false;

  // Input from memory
  bool mem_req_ready = false;
  bool mem_resp_valid = false;
  uint8_t mem_resp_id = 0;
  uint32_t mem_resp_data[ICACHE_LINE_SIZE / 4] = {0};
};

struct ICacheV2_out_t {
  // Output to IFU
  bool miss = false;
  bool ifu_resp_valid = false;
  bool ifu_req_ready = false;
  uint32_t ifu_resp_pc = 0;
  uint32_t rd_data[ICACHE_LINE_SIZE / 4] = {0};
  bool ifu_page_fault = false;

  // Output to MMU
  bool ppn_ready = false;
  bool mmu_req_valid = false;
  uint32_t mmu_req_vtag = 0;

  // Output to memory
  bool mem_req_valid = false;
  uint32_t mem_req_addr = 0;
  uint8_t mem_req_id = 0;
  bool mem_resp_ready = false;
};

struct ICacheV2_IO_t {
  ICacheV2_in_t in;
  ICacheV2_out_t out;
};

class ICacheV2 {
public:
  explicit ICacheV2(ICacheV2Config cfg = {});

  void reset();
  void invalidate_all();
  void comb();
  void seq();
  void log_debug() const;
  void perf_print() const;

  ICacheV2_IO_t io;

private:
  // --- Helpers ---
  static bool is_power_of_two(uint32_t v);
  static uint32_t xorshift32(uint32_t x);

  uint32_t mask_lsb(uint32_t bits) const;
  uint32_t get_index_from_pc(uint32_t pc) const;
  uint32_t get_ppn_from_paddr(uint32_t paddr) const;
  uint32_t get_line_addr(uint32_t ppn, uint32_t index) const;

  uint32_t cache_tag_at(uint32_t set, uint32_t way) const;
  bool cache_valid_at(uint32_t set, uint32_t way) const;
  uint32_t cache_data_at(uint32_t set, uint32_t way, uint32_t word) const;
  void cache_write_line(uint32_t set, uint32_t way,
                        const uint32_t *line_words);
  void cache_set_tag_valid(uint32_t set, uint32_t way, uint32_t tag, bool valid);

  // Replacement
  uint32_t choose_victim_way(uint32_t set);
  void repl_on_access(uint32_t set, uint32_t way);

  // PLRU (generic N-way tree)
  uint32_t plru_choose(uint32_t set) const;
  void plru_update(uint32_t set, uint32_t way);

  // MSHR helpers
  int find_mshr_by_line(uint32_t line_addr) const;
  int find_free_mshr() const;
  uint32_t free_mshr_count() const;
  bool mshr_waiter_test(uint32_t mshr_idx, uint32_t rob_idx) const;
  void mshr_waiter_set(uint32_t mshr_idx, uint32_t rob_idx);

  // TXID helpers (0..15)
  int alloc_txid();
  void free_txid(uint8_t txid);

  // --- Config / Derived parameters ---
  ICacheV2Config cfg_;
  uint32_t offset_bits_ = 0;
  uint32_t index_bits_ = 0;
  uint32_t set_num_ = 0;
  uint32_t word_num_ = 0;
  uint32_t waiters_words_ = 0;

  // --- Cache arrays ---
  std::vector<uint32_t> cache_data_;  // [set][way][word]
  std::vector<uint32_t> cache_tag_;   // [set][way]
  std::vector<uint8_t> cache_valid_;  // [set][way]

  // --- Replacement state ---
  std::vector<uint32_t> rr_ptr_r_;
  std::vector<uint32_t> rr_ptr_next_;
  std::vector<uint8_t> plru_bits_r_;   // [set][ways-1]
  std::vector<uint8_t> plru_bits_next_;
  uint32_t rand_seed_r_ = 1;
  uint32_t rand_seed_next_ = 1;

  // --- Epoch / Refetch ---
  uint32_t epoch_r_ = 0;
  uint32_t epoch_next_ = 0;

  // --- ROB (in-order commit) ---
  enum class RobState : uint8_t {
    EMPTY = 0,
    LOOKUP = 1,
    WAIT_TLB = 2,
    WAIT_MSHR_ALLOC = 3,
    WAIT_MSHR = 4,
    READY = 5,
    READY_FAULT = 6,
    KILLED = 7,
  };

  uint32_t rob_head_r_ = 0;
  uint32_t rob_head_next_ = 0;
  uint32_t rob_tail_r_ = 0;
  uint32_t rob_tail_next_ = 0;
  uint32_t rob_count_r_ = 0;
  uint32_t rob_count_next_ = 0;

  std::vector<uint8_t> rob_valid_r_;
  std::vector<uint8_t> rob_valid_next_;
  std::vector<uint32_t> rob_pc_r_;
  std::vector<uint32_t> rob_pc_next_;
  std::vector<uint8_t> rob_state_r_;
  std::vector<uint8_t> rob_state_next_;
  std::vector<uint32_t> rob_line_addr_r_;
  std::vector<uint32_t> rob_line_addr_next_;
  std::vector<uint32_t> rob_mshr_idx_r_;
  std::vector<uint32_t> rob_mshr_idx_next_;
  std::vector<uint32_t> rob_line_data_r_;   // [rob][word]
  std::vector<uint32_t> rob_line_data_next_;

  // --- Lookup pipeline reg (stage1->stage2) ---
  bool lookup_valid_r_ = false;
  bool lookup_valid_next_ = false;
  uint32_t lookup_pc_r_ = 0;
  uint32_t lookup_pc_next_ = 0;
  uint32_t lookup_index_r_ = 0;
  uint32_t lookup_index_next_ = 0;
  uint32_t lookup_rob_idx_r_ = 0;
  uint32_t lookup_rob_idx_next_ = 0;

  // Latched set data for stage2: [ways][word]/tag/valid.
  std::vector<uint32_t> set_data_w_;
  std::vector<uint32_t> set_data_r_;
  std::vector<uint32_t> set_tag_w_;
  std::vector<uint32_t> set_tag_r_;
  std::vector<uint8_t> set_valid_w_;
  std::vector<uint8_t> set_valid_r_;
  bool set_load_fire_ = false; // comb-only: load set_*_w_ -> set_*_r_ in seq

  // SRAM lookup delay model (single outstanding lookup)
  bool sram_pending_r_ = false;
  bool sram_pending_next_ = false;
  uint32_t sram_delay_r_ = 0;
  uint32_t sram_delay_next_ = 0;
  uint32_t sram_index_r_ = 0;
  uint32_t sram_index_next_ = 0;
  uint32_t sram_pc_r_ = 0;
  uint32_t sram_pc_next_ = 0;
  uint32_t sram_rob_idx_r_ = 0;
  uint32_t sram_rob_idx_next_ = 0;
  uint32_t sram_seed_r_ = 1;
  uint32_t sram_seed_next_ = 1;
  bool sram_load_fire_ = false; // comb-only

  // --- MSHR ---
  enum class MshrState : uint8_t {
    FREE = 0,
    ALLOC = 1,     // allocated, waiting to be scheduled
    SENDING = 2,   // mem_req_valid asserted, waiting ready
    WAIT_RESP = 3, // request accepted, waiting response
  };

  std::vector<uint8_t> mshr_state_r_;
  std::vector<uint8_t> mshr_state_next_;
  std::vector<uint32_t> mshr_line_addr_r_;
  std::vector<uint32_t> mshr_line_addr_next_;
  std::vector<uint8_t> mshr_is_prefetch_r_;
  std::vector<uint8_t> mshr_is_prefetch_next_;
  std::vector<uint8_t> mshr_txid_r_;
  std::vector<uint8_t> mshr_txid_next_;
  std::vector<uint8_t> mshr_txid_valid_r_;
  std::vector<uint8_t> mshr_txid_valid_next_;

  // Waiters bitset: [mshr][waiters_words_] of uint64_t
  std::vector<uint64_t> mshr_waiters_r_;
  std::vector<uint64_t> mshr_waiters_next_;

  // --- TXID mapping ---
  bool txid_inflight_r_[16] = {false};
  bool txid_inflight_next_[16] = {false};
  bool txid_canceled_r_[16] = {false};
  bool txid_canceled_next_[16] = {false};
  uint8_t txid_mshr_r_[16] = {0};
  uint8_t txid_mshr_next_[16] = {0};
  bool txid_mshr_valid_r_[16] = {false};
  bool txid_mshr_valid_next_[16] = {false};
  uint8_t txid_rr_r_ = 0;
  uint8_t txid_rr_next_ = 0;

  // --- Memory request latch (hold under backpressure) ---
  bool memreq_latched_valid_r_ = false;
  bool memreq_latched_valid_next_ = false;
  uint32_t memreq_latched_addr_r_ = 0;
  uint32_t memreq_latched_addr_next_ = 0;
  uint8_t memreq_latched_id_r_ = 0;
  uint8_t memreq_latched_id_next_ = 0;
  uint8_t memreq_latched_mshr_r_ = 0;
  uint8_t memreq_latched_mshr_next_ = 0;

  // Cache fill writeback (comb->seq)
  bool cache_fill_fire_ = false; // comb-only
  uint32_t cache_fill_set_ = 0;
  uint32_t cache_fill_way_ = 0;
  uint32_t cache_fill_tag_ = 0; // ppn
  std::vector<uint32_t> cache_fill_words_;

  // MSHR utilization (peak occupancy since reset)
  uint32_t mshr_peak_r_ = 0;
  uint32_t mshr_peak_next_ = 0;
};

} // namespace icache_module_v2_n

#endif
