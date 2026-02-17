#pragma once
#include <cstddef>
#include <cstdint>
#include "ptw_module.h"

namespace ptw_module_n {
namespace ptw_pi_po {

static constexpr size_t PTW_in_t_BITS = (1 * 1) + (10 * 1) + (10 * 1) + (2 * 1) + (22 * 1) + (9 * 1) + (32 * 1) + (32 * 1) + (2 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (1 * 1) + (1 * 1) + (32 * 1);
inline void pack_PTW_in_t(const ptw_module_n::PTW_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tlb_miss) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tlb_vpn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tlb_vpn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tlb_op_type) >> b) & 1u) != 0; }
for (size_t b = 0; b < 22; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.satp_ppn) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.satp_asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mstatus) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sstatus) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.privilege) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_req_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_resp_miss) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_resp_data) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_data) >> b) & 1u) != 0; }
}

inline void unpack_PTW_in_t(const bool *bits, size_t &idx, ptw_module_n::PTW_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tlb_miss = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tlb_vpn1 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tlb_vpn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tlb_op_type = static_cast<wire2_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 22; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.satp_ppn = static_cast<wire22_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.satp_asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mstatus = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sstatus = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.privilege = static_cast<wire2_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_req_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_resp_miss = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_resp_data = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_data = static_cast<wire32_t>(tmp); }
}

static constexpr size_t PTW_regs_t_BITS = (3 * 1) + (1 * 1) + (10 * 1) + (10 * 1) + (2 * 1) + (32 * 1) + (32 * 1);
inline void pack_PTW_regs_t(const ptw_module_n::PTW_regs_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 3; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.state_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_state_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.vpn1_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.vpn0_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.op_type_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pte1_raw_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pte2_raw_r) >> b) & 1u) != 0; }
}

inline void unpack_PTW_regs_t(const bool *bits, size_t &idx, ptw_module_n::PTW_regs_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 3; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.state_r = static_cast<reg3_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_state_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.vpn1_r = static_cast<reg10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.vpn0_r = static_cast<reg10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.op_type_r = static_cast<reg2_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pte1_raw_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pte2_raw_r = static_cast<reg32_t>(tmp); }
}

static constexpr size_t PTW_lookup_in_t_BITS = (1 * 1);
inline void pack_PTW_lookup_in_t(const ptw_module_n::PTW_lookup_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.reserved) >> b) & 1u) != 0; }
}

inline void unpack_PTW_lookup_in_t(const bool *bits, size_t &idx, ptw_module_n::PTW_lookup_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.reserved = static_cast<wire1_t>(tmp); }
}

static constexpr size_t PTW_out_t_BITS = (1 * 1) + (1 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (1 * 1);
inline void pack_PTW_out_t(const ptw_module_n::PTW_out_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_dest) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_vpn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_vpn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_ppn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_ppn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_megapage) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_dirty) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_accessed) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_global) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_user) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_execute) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_write) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_read) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_valid_bit) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ptw_write_pte_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_req_paddr) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dcache_resp_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_paddr) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_ready) >> b) & 1u) != 0; }
}

inline void unpack_PTW_out_t(const bool *bits, size_t &idx, ptw_module_n::PTW_out_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_dest = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_vpn1 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_vpn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_ppn1 = static_cast<wire12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_ppn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_megapage = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_dirty = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_accessed = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_global = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_user = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_execute = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_write = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_read = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_valid_bit = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ptw_write_pte_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_req_paddr = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dcache_resp_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_paddr = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_ready = static_cast<wire1_t>(tmp); }
}

static constexpr size_t PTW_table_write_t_BITS = (1 * 1) + (1 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1);
inline void pack_PTW_table_write_t(const ptw_module_n::PTW_table_write_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.we) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dest) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.vpn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.vpn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.megapage) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.dirty) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.accessed) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.global) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.user) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.execute) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.write) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.read) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.valid_bit) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pte_valid) >> b) & 1u) != 0; }
}

inline void unpack_PTW_table_write_t(const bool *bits, size_t &idx, ptw_module_n::PTW_table_write_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.we = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dest = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.vpn1 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.vpn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn1 = static_cast<wire12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.megapage = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.dirty = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.accessed = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.global = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.user = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.execute = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.write = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.read = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.valid_bit = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pte_valid = static_cast<wire1_t>(tmp); }
}

static constexpr size_t PI_WIDTH = PTW_in_t_BITS + PTW_regs_t_BITS + PTW_lookup_in_t_BITS;
static constexpr size_t PO_WIDTH = PTW_out_t_BITS + PTW_regs_t_BITS + PTW_table_write_t_BITS;

inline void pack_pi(const ptw_module_n::PTW_IO_t &io, bool *pi) {
  size_t idx = 0;
  pack_PTW_in_t(io.in, pi, idx);
  pack_PTW_regs_t(io.regs, pi, idx);
  pack_PTW_lookup_in_t(io.lookup_in, pi, idx);
}

inline void unpack_pi(const bool *pi, ptw_module_n::PTW_IO_t &io) {
  size_t idx = 0;
  unpack_PTW_in_t(pi, idx, io.in);
  unpack_PTW_regs_t(pi, idx, io.regs);
  unpack_PTW_lookup_in_t(pi, idx, io.lookup_in);
}

inline void pack_po(const ptw_module_n::PTW_IO_t &io, bool *po) {
  size_t idx = 0;
  pack_PTW_out_t(io.out, po, idx);
  pack_PTW_regs_t(io.reg_write, po, idx);
  pack_PTW_table_write_t(io.table_write, po, idx);
}

inline void unpack_po(const bool *po, ptw_module_n::PTW_IO_t &io) {
  size_t idx = 0;
  unpack_PTW_out_t(po, idx, io.out);
  unpack_PTW_regs_t(po, idx, io.reg_write);
  unpack_PTW_table_write_t(po, idx, io.table_write);
}

inline void pi_to_inputs(const bool *pi, ptw_module_n::PTW_IO_t &io) {
  unpack_pi(pi, io);
}

inline void outputs_to_po(const ptw_module_n::PTW_IO_t &io, bool *po) {
  pack_po(io, po);
}

template <typename ModuleT>
inline void eval_comb(ModuleT &module, const bool *pi, bool *po) {
  unpack_pi(pi, module.io);
  module.comb();
  pack_po(module.io, po);
}

} // namespace ptw_pi_po
} // namespace ptw_module_n
