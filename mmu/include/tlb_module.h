#pragma once

#include <config.h>
#include <cstdint>
#include <mmu_io.h>

#ifndef MMU_TLB_ENTRY_NUM
#if ITLB_SIZE > DTLB_SIZE
#define MMU_TLB_ENTRY_NUM ITLB_SIZE
#else
#define MMU_TLB_ENTRY_NUM DTLB_SIZE
#endif
#endif

#ifndef MMU_TLB_LOOKUP_LATENCY
#define MMU_TLB_LOOKUP_LATENCY 0
#endif

#ifndef MMU_TLB_LOOKUP_FROM_INPUT
#define MMU_TLB_LOOKUP_FROM_INPUT 0
#endif

namespace tlb_module_n {

static constexpr uint32_t kTlbEntryNum = MMU_TLB_ENTRY_NUM;
static constexpr uint32_t kLsuPorts = MAX_LSU_REQ_NUM;
static constexpr uint32_t kPlruTreeSize = kTlbEntryNum - 1;

struct TLB_in_t {
  wire1_t is_itlb = true;
  wire9_t satp_asid = 0;

  wire1_t ifu_req_valid = false;
  wire20_t ifu_req_vtag = 0;
  wire2_t ifu_req_op_type = static_cast<wire2_t>(mmu_n::OP_FETCH);

  wire1_t lsu_req_valid[kLsuPorts] = {false};
  wire20_t lsu_req_vtag[kLsuPorts] = {0};
  wire2_t lsu_req_op_type[kLsuPorts] = {0};

  wire1_t flush_valid = false;
  wire20_t flush_vpn = 0;
  wire9_t flush_asid = 0;

  wire1_t ptw_write_valid = false;
  wire1_t ptw_write_dest = 0;
  wire10_t ptw_write_vpn1 = 0;
  wire10_t ptw_write_vpn0 = 0;
  wire12_t ptw_write_ppn1 = 0;
  wire10_t ptw_write_ppn0 = 0;
  wire9_t ptw_write_asid = 0;
  wire1_t ptw_write_megapage = false;
  wire1_t ptw_write_dirty = false;
  wire1_t ptw_write_accessed = false;
  wire1_t ptw_write_global = false;
  wire1_t ptw_write_user = false;
  wire1_t ptw_write_execute = false;
  wire1_t ptw_write_write = false;
  wire1_t ptw_write_read = false;
  wire1_t ptw_write_valid_bit = false;
  wire1_t ptw_write_pte_valid = false;
};

struct TLB_regs_t {
  reg1_t entry_pte_valid[kTlbEntryNum] = {false};
  reg10_t entry_vpn1[kTlbEntryNum] = {0};
  reg10_t entry_vpn0[kTlbEntryNum] = {0};
  reg12_t entry_ppn1[kTlbEntryNum] = {0};
  reg10_t entry_ppn0[kTlbEntryNum] = {0};
  reg9_t entry_asid[kTlbEntryNum] = {0};
  reg1_t entry_megapage[kTlbEntryNum] = {false};
  reg1_t entry_dirty[kTlbEntryNum] = {false};
  reg1_t entry_accessed[kTlbEntryNum] = {false};
  reg1_t entry_global[kTlbEntryNum] = {false};
  reg1_t entry_user[kTlbEntryNum] = {false};
  reg1_t entry_execute[kTlbEntryNum] = {false};
  reg1_t entry_write[kTlbEntryNum] = {false};
  reg1_t entry_read[kTlbEntryNum] = {false};
  reg1_t entry_valid[kTlbEntryNum] = {false};

  reg1_t plru_tree[kPlruTreeSize] = {false};
  reg6_t replace_idx_r = 0;

  reg1_t ifu_pending_r = false;
  reg8_t ifu_delay_r = 0;
  reg20_t ifu_vtag_r = 0;
  reg2_t ifu_op_type_r = 0;

  reg1_t lsu_pending_r[kLsuPorts] = {false};
  reg8_t lsu_delay_r[kLsuPorts] = {0};
  reg20_t lsu_vtag_r[kLsuPorts] = {0};
  reg2_t lsu_op_type_r[kLsuPorts] = {0};
};

struct TLB_lookup_in_t {
  wire1_t ifu_lookup_resp_valid = false;
  wire1_t ifu_lookup_hit = false;
  wire6_t ifu_lookup_hit_index = 0;
  wire10_t ifu_lookup_vpn1 = 0;
  wire10_t ifu_lookup_vpn0 = 0;
  wire12_t ifu_lookup_ppn1 = 0;
  wire10_t ifu_lookup_ppn0 = 0;
  wire9_t ifu_lookup_asid = 0;
  wire1_t ifu_lookup_megapage = false;
  wire1_t ifu_lookup_dirty = false;
  wire1_t ifu_lookup_accessed = false;
  wire1_t ifu_lookup_global = false;
  wire1_t ifu_lookup_user = false;
  wire1_t ifu_lookup_execute = false;
  wire1_t ifu_lookup_write = false;
  wire1_t ifu_lookup_read = false;
  wire1_t ifu_lookup_valid = false;
  wire1_t ifu_lookup_pte_valid = false;

  wire1_t lsu_lookup_resp_valid[kLsuPorts] = {false};
  wire1_t lsu_lookup_hit[kLsuPorts] = {false};
  wire6_t lsu_lookup_hit_index[kLsuPorts] = {0};
  wire10_t lsu_lookup_vpn1[kLsuPorts] = {0};
  wire10_t lsu_lookup_vpn0[kLsuPorts] = {0};
  wire12_t lsu_lookup_ppn1[kLsuPorts] = {0};
  wire10_t lsu_lookup_ppn0[kLsuPorts] = {0};
  wire9_t lsu_lookup_asid[kLsuPorts] = {0};
  wire1_t lsu_lookup_megapage[kLsuPorts] = {false};
  wire1_t lsu_lookup_dirty[kLsuPorts] = {false};
  wire1_t lsu_lookup_accessed[kLsuPorts] = {false};
  wire1_t lsu_lookup_global[kLsuPorts] = {false};
  wire1_t lsu_lookup_user[kLsuPorts] = {false};
  wire1_t lsu_lookup_execute[kLsuPorts] = {false};
  wire1_t lsu_lookup_write[kLsuPorts] = {false};
  wire1_t lsu_lookup_read[kLsuPorts] = {false};
  wire1_t lsu_lookup_valid[kLsuPorts] = {false};
  wire1_t lsu_lookup_pte_valid[kLsuPorts] = {false};
};

struct TLB_out_t {
  wire1_t ifu_req_ready = true;
  wire1_t lsu_req_ready[kLsuPorts] = {true};

  wire1_t ifu_resp_valid = false;
  wire1_t ifu_resp_hit = false;
  wire6_t ifu_resp_hit_index = 0;
  wire10_t ifu_resp_vpn1 = 0;
  wire10_t ifu_resp_vpn0 = 0;
  wire12_t ifu_resp_ppn1 = 0;
  wire10_t ifu_resp_ppn0 = 0;
  wire9_t ifu_resp_asid = 0;
  wire1_t ifu_resp_megapage = false;
  wire1_t ifu_resp_dirty = false;
  wire1_t ifu_resp_accessed = false;
  wire1_t ifu_resp_global = false;
  wire1_t ifu_resp_user = false;
  wire1_t ifu_resp_execute = false;
  wire1_t ifu_resp_write = false;
  wire1_t ifu_resp_read = false;
  wire1_t ifu_resp_valid_bit = false;
  wire1_t ifu_resp_pte_valid = false;

  wire1_t lsu_resp_valid[kLsuPorts] = {false};
  wire1_t lsu_resp_hit[kLsuPorts] = {false};
  wire6_t lsu_resp_hit_index[kLsuPorts] = {0};
  wire10_t lsu_resp_vpn1[kLsuPorts] = {0};
  wire10_t lsu_resp_vpn0[kLsuPorts] = {0};
  wire12_t lsu_resp_ppn1[kLsuPorts] = {0};
  wire10_t lsu_resp_ppn0[kLsuPorts] = {0};
  wire9_t lsu_resp_asid[kLsuPorts] = {0};
  wire1_t lsu_resp_megapage[kLsuPorts] = {false};
  wire1_t lsu_resp_dirty[kLsuPorts] = {false};
  wire1_t lsu_resp_accessed[kLsuPorts] = {false};
  wire1_t lsu_resp_global[kLsuPorts] = {false};
  wire1_t lsu_resp_user[kLsuPorts] = {false};
  wire1_t lsu_resp_execute[kLsuPorts] = {false};
  wire1_t lsu_resp_write[kLsuPorts] = {false};
  wire1_t lsu_resp_read[kLsuPorts] = {false};
  wire1_t lsu_resp_valid_bit[kLsuPorts] = {false};
  wire1_t lsu_resp_pte_valid[kLsuPorts] = {false};
};

using TLB_reg_write_t = TLB_regs_t;

struct TLB_table_write_t {
  wire1_t we = false;
  wire6_t index = 0;
  wire10_t vpn1 = 0;
  wire10_t vpn0 = 0;
  wire12_t ppn1 = 0;
  wire10_t ppn0 = 0;
  wire9_t asid = 0;
  wire1_t megapage = false;
  wire1_t dirty = false;
  wire1_t accessed = false;
  wire1_t global = false;
  wire1_t user = false;
  wire1_t execute = false;
  wire1_t write = false;
  wire1_t read = false;
  wire1_t valid_bit = false;
  wire1_t pte_valid = false;
};

struct TLB_IO_t {
  TLB_in_t in;
  TLB_regs_t regs;
  TLB_lookup_in_t lookup_in;
  TLB_out_t out;
  TLB_reg_write_t reg_write;
  TLB_table_write_t table_write;
};

class TLBModule {
public:
  TLB_IO_t io;

  void reset();
  void comb();
  void seq();
};

} // namespace tlb_module_n

