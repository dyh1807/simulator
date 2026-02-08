#pragma once
#include <cstddef>
#include <cstdint>
#include "tlb_module.h"

namespace tlb_module_n {
namespace tlb_pi_po {

static constexpr size_t TLB_in_t_BITS = (1 * 1) + (9 * 1) + (1 * 1) + (20 * 1) + (2 * 1) + (1 * (kLsuPorts)) + (20 * (kLsuPorts)) + (2 * (kLsuPorts)) + (1 * 1) + (20 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1);
inline void pack_TLB_in_t(const tlb_module_n::TLB_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.is_itlb) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.satp_asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_vtag) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_op_type) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_req_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_req_vtag[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_req_op_type[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.flush_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.flush_vpn) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.flush_asid) >> b) & 1u) != 0; }
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
}

inline void unpack_TLB_in_t(const bool *bits, size_t &idx, tlb_module_n::TLB_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.is_itlb = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.satp_asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_vtag = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_op_type = static_cast<wire2_t>(tmp); }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_req_valid[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_req_vtag[i0] = static_cast<wire20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_req_op_type[i0] = static_cast<wire2_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.flush_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.flush_vpn = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.flush_asid = static_cast<wire9_t>(tmp); }
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
}

static constexpr size_t TLB_regs_t_BITS = (1 * (kTlbEntryNum)) + (10 * (kTlbEntryNum)) + (10 * (kTlbEntryNum)) + (12 * (kTlbEntryNum)) + (10 * (kTlbEntryNum)) + (9 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kTlbEntryNum)) + (1 * (kPlruTreeSize)) + (6 * 1) + (1 * 1) + (8 * 1) + (20 * 1) + (2 * 1) + (1 * (kLsuPorts)) + (8 * (kLsuPorts)) + (20 * (kLsuPorts)) + (2 * (kLsuPorts));
inline void pack_TLB_regs_t(const tlb_module_n::TLB_regs_t &v, bool *bits, size_t &idx) {
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_pte_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_vpn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_vpn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_ppn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_ppn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_asid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_megapage[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_dirty[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_accessed[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_global[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_user[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_execute[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_write[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_read[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.entry_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kPlruTreeSize); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.plru_tree[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.replace_idx_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_pending_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_delay_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_vtag_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_op_type_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_pending_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_delay_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_vtag_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_op_type_r[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_TLB_regs_t(const bool *bits, size_t &idx, tlb_module_n::TLB_regs_t &v) {
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_pte_valid[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_vpn1[i0] = static_cast<reg10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_vpn0[i0] = static_cast<reg10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_ppn1[i0] = static_cast<reg12_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_ppn0[i0] = static_cast<reg10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_asid[i0] = static_cast<reg9_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_megapage[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_dirty[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_accessed[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_global[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_user[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_execute[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_write[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_read[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kTlbEntryNum); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.entry_valid[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kPlruTreeSize); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.plru_tree[i0] = static_cast<reg1_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.replace_idx_r = static_cast<reg6_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_pending_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_delay_r = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_vtag_r = static_cast<reg20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_op_type_r = static_cast<reg2_t>(tmp); }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_pending_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_delay_r[i0] = static_cast<reg8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_vtag_r[i0] = static_cast<reg20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_op_type_r[i0] = static_cast<reg2_t>(tmp); }
}
}

static constexpr size_t TLB_lookup_in_t_BITS = (1 * 1) + (1 * 1) + (6 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (6 * (kLsuPorts)) + (10 * (kLsuPorts)) + (10 * (kLsuPorts)) + (12 * (kLsuPorts)) + (10 * (kLsuPorts)) + (9 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts));
inline void pack_TLB_lookup_in_t(const tlb_module_n::TLB_lookup_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_hit) >> b) & 1u) != 0; }
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_hit_index) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_vpn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_vpn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_ppn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_ppn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_megapage) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_dirty) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_accessed) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_global) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_user) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_execute) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_write) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_read) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_lookup_pte_valid) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_resp_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_hit[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_hit_index[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_vpn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_vpn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_ppn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_ppn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_asid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_megapage[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_dirty[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_accessed[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_global[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_user[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_execute[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_write[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_read[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_lookup_pte_valid[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_TLB_lookup_in_t(const bool *bits, size_t &idx, tlb_module_n::TLB_lookup_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_hit = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_hit_index = static_cast<wire6_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_vpn1 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_vpn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_ppn1 = static_cast<wire12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_ppn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_megapage = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_dirty = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_accessed = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_global = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_user = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_execute = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_write = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_read = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_lookup_pte_valid = static_cast<wire1_t>(tmp); }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_resp_valid[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_hit[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_hit_index[i0] = static_cast<wire6_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_vpn1[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_vpn0[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_ppn1[i0] = static_cast<wire12_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_ppn0[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_asid[i0] = static_cast<wire9_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_megapage[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_dirty[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_accessed[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_global[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_user[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_execute[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_write[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_read[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_valid[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_lookup_pte_valid[i0] = static_cast<wire1_t>(tmp); }
}
}

static constexpr size_t TLB_out_t_BITS = (1 * 1) + (1 * (kLsuPorts)) + (1 * 1) + (1 * 1) + (6 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (6 * (kLsuPorts)) + (10 * (kLsuPorts)) + (10 * (kLsuPorts)) + (12 * (kLsuPorts)) + (10 * (kLsuPorts)) + (9 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts)) + (1 * (kLsuPorts));
inline void pack_TLB_out_t(const tlb_module_n::TLB_out_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_ready) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_req_ready[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_hit) >> b) & 1u) != 0; }
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_hit_index) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_vpn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_vpn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_ppn1) >> b) & 1u) != 0; }
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_ppn0) >> b) & 1u) != 0; }
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_asid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_megapage) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_dirty) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_accessed) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_global) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_user) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_execute) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_write) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_read) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_valid_bit) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_pte_valid) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_valid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_hit[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_hit_index[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_vpn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_vpn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_ppn1[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 10; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_ppn0[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 9; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_asid[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_megapage[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_dirty[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_accessed[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_global[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_user[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_execute[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_write[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_read[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_valid_bit[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lsu_resp_pte_valid[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_TLB_out_t(const bool *bits, size_t &idx, tlb_module_n::TLB_out_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_ready = static_cast<wire1_t>(tmp); }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_req_ready[i0] = static_cast<wire1_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_hit = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_hit_index = static_cast<wire6_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_vpn1 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_vpn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_ppn1 = static_cast<wire12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_ppn0 = static_cast<wire10_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_asid = static_cast<wire9_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_megapage = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_dirty = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_accessed = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_global = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_user = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_execute = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_write = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_read = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_valid_bit = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_pte_valid = static_cast<wire1_t>(tmp); }
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_valid[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_hit[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_hit_index[i0] = static_cast<wire6_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_vpn1[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_vpn0[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_ppn1[i0] = static_cast<wire12_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 10; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_ppn0[i0] = static_cast<wire10_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 9; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_asid[i0] = static_cast<wire9_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_megapage[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_dirty[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_accessed[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_global[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_user[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_execute[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_write[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_read[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_valid_bit[i0] = static_cast<wire1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (kLsuPorts); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lsu_resp_pte_valid[i0] = static_cast<wire1_t>(tmp); }
}
}

static constexpr size_t TLB_table_write_t_BITS = (1 * 1) + (6 * 1) + (10 * 1) + (10 * 1) + (12 * 1) + (10 * 1) + (9 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1);
inline void pack_TLB_table_write_t(const tlb_module_n::TLB_table_write_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.we) >> b) & 1u) != 0; }
for (size_t b = 0; b < 6; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.index) >> b) & 1u) != 0; }
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

inline void unpack_TLB_table_write_t(const bool *bits, size_t &idx, tlb_module_n::TLB_table_write_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.we = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 6; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.index = static_cast<wire6_t>(tmp); }
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

static constexpr size_t PI_WIDTH = TLB_in_t_BITS + TLB_regs_t_BITS + TLB_lookup_in_t_BITS;
static constexpr size_t PO_WIDTH = TLB_out_t_BITS + TLB_regs_t_BITS + TLB_table_write_t_BITS;

inline void pack_pi(const tlb_module_n::TLB_IO_t &io, bool *pi) {
  size_t idx = 0;
  pack_TLB_in_t(io.in, pi, idx);
  pack_TLB_regs_t(io.regs, pi, idx);
  pack_TLB_lookup_in_t(io.lookup_in, pi, idx);
}

inline void unpack_pi(const bool *pi, tlb_module_n::TLB_IO_t &io) {
  size_t idx = 0;
  unpack_TLB_in_t(pi, idx, io.in);
  unpack_TLB_regs_t(pi, idx, io.regs);
  unpack_TLB_lookup_in_t(pi, idx, io.lookup_in);
}

inline void pack_po(const tlb_module_n::TLB_IO_t &io, bool *po) {
  size_t idx = 0;
  pack_TLB_out_t(io.out, po, idx);
  pack_TLB_regs_t(io.reg_write, po, idx);
  pack_TLB_table_write_t(io.table_write, po, idx);
}

inline void unpack_po(const bool *po, tlb_module_n::TLB_IO_t &io) {
  size_t idx = 0;
  unpack_TLB_out_t(po, idx, io.out);
  unpack_TLB_regs_t(po, idx, io.reg_write);
  unpack_TLB_table_write_t(po, idx, io.table_write);
}

inline void pi_to_inputs(const bool *pi, tlb_module_n::TLB_IO_t &io) {
  unpack_pi(pi, io);
}

inline void outputs_to_po(const tlb_module_n::TLB_IO_t &io, bool *po) {
  pack_po(io, po);
}

template <typename ModuleT>
inline void eval_comb(ModuleT &module, const bool *pi, bool *po) {
  unpack_pi(pi, module.io);
  module.comb();
  pack_po(module.io, po);
}

} // namespace tlb_pi_po
} // namespace tlb_module_n
