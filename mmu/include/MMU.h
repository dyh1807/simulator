#pragma once
/*
 * MMU: Implements virtual to physical address translation
 * - Supports Sv32 paging mode
 * - Consists of generalized-IO TLB/PTW modules
 * - Interfaces with IFU and LSU for address translation requests
 */

#include "ptw_module.h"
#include "tlb_module.h"
#include "TLBEntry.h"
#include "mmu_io.h"
#include <array>
#include <cstdint>

class MMU {
public:
  struct TLBPerfCounter {
    uint64_t access_count = 0;
    uint64_t hit_count = 0;

    uint64_t get_access_count() const { return access_count; }
    uint64_t get_hit_count() const { return hit_count; }
  };

  MMU();

  void reset();
  void comb_frontend();
  void comb_backend();
  void comb_arbiter();
  void comb_ptw();
  void seq();

  // I/O Ports
  MMU_IO_t io;

  /*
   * TLB perf counters are exposed for existing perf-stat callsites.
   */
  TLBPerfCounter i_tlb;
  TLBPerfCounter d_tlb;

private:
  // Generalized-IO MMU submodules.
  tlb_module_n::TLBModule itlb_mod;
  tlb_module_n::TLBModule dtlb_mod;
  ptw_module_n::PTWModule ptw_mod;

  // Externalized TLB table state (register-file style backend).
  std::array<TLBEntry, ITLB_SIZE> itlb_table;
  std::array<TLBEntry, DTLB_SIZE> dtlb_table;

  // Lookup latency state lives in top-level state, not inside tlb_module.
  reg16_t itlb_ifu_lookup_delay_r;
  reg16_t dtlb_lsu_lookup_delay_r[MAX_LSU_REQ_NUM];
  reg6_t itlb_refill_idx_r;
  reg6_t dtlb_refill_idx_r;

  // wire candidates for arbiter
  TLB_to_PTW tlb2ptw_frontend;
  TLB_to_PTW tlb2ptw_backend[MAX_LSU_REQ_NUM];
  TLB_to_PTW tlb2ptw;

  // Memory simulation state (no SimDDR backend case).
  struct {
    uint32_t count;
    bool busy;
    uint32_t data;
    uint32_t addr;
  } mem_sim;

  void comb_memory();

  /*
   * MMU Top-Level Registers
   */
  mmu_resp_master_t resp_ifu_r_1;
  mmu_resp_master_t resp_lsu_r_1[MAX_LSU_REQ_NUM];

  /* Helper function to set mmu response fields */
  static inline void comb_set_resp(mmu_resp_master_t &resp, bool valid,
                                   bool miss, bool excp, uint32_t ptag) {
    resp.valid = valid;
    resp.excp = excp;
    resp.miss = miss;
    resp.ptag = ptag;
  }
};
