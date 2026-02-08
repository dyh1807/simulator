#pragma once

#include <config.h>
#include <cstdint>
#include <mmu_io.h>

namespace ptw_module_n {

struct PTW_in_t {
  wire1_t tlb_miss = false;
  wire10_t tlb_vpn1 = 0;
  wire10_t tlb_vpn0 = 0;
  wire2_t tlb_op_type = 0;

  wire22_t satp_ppn = 0;
  wire9_t satp_asid = 0;
  wire32_t mstatus = 0;
  wire32_t sstatus = 0;
  wire2_t privilege = static_cast<wire2_t>(mmu_n::M_MODE);

  wire1_t dcache_req_ready = false;
  wire1_t dcache_resp_valid = false;
  wire1_t dcache_resp_miss = false;
  wire32_t dcache_resp_data = 0;

  wire1_t mem_req_ready = false;
  wire1_t mem_resp_valid = false;
  wire32_t mem_resp_data = 0;
};

struct PTW_regs_t {
  reg3_t state_r = 0;
  reg1_t dcache_state_r = 0;

  reg10_t vpn1_r = 0;
  reg10_t vpn0_r = 0;
  reg2_t op_type_r = 0;

  reg32_t pte1_raw_r = 0;
  reg32_t pte2_raw_r = 0;
};

struct PTW_lookup_in_t {
  wire1_t reserved = false;
};

struct PTW_out_t {
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

  wire1_t dcache_req_valid = false;
  wire32_t dcache_req_paddr = 0;
  wire1_t dcache_resp_ready = false;

  wire1_t mem_req_valid = false;
  wire32_t mem_req_paddr = 0;
  wire1_t mem_resp_ready = false;
};

using PTW_reg_write_t = PTW_regs_t;

struct PTW_table_write_t {
  wire1_t we = false;
  wire1_t dest = 0;
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

struct PTW_IO_t {
  PTW_in_t in;
  PTW_regs_t regs;
  PTW_lookup_in_t lookup_in;
  PTW_out_t out;
  PTW_reg_write_t reg_write;
  PTW_table_write_t table_write;
};

class PTWModule {
public:
  PTW_IO_t io;

  void reset();
  void comb();
  void seq();
};

} // namespace ptw_module_n

