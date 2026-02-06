#pragma once
#include <cstddef>
#include <cstdint>
#include "icache_module_v2.h"

namespace icache_module_v2_n {
namespace icache_v2_pi_po {

static constexpr size_t ICacheV2_out_t_BITS = (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (32 * (ICACHE_LINE_SIZE / 4)) + (1 * 1) + (1 * 1) + (1 * 1) + (20 * 1) + (1 * 1) + (32 * 1) + (4 * 1) + (1 * 1);
inline void pack_ICacheV2_out_t(const icache_module_v2_n::ICacheV2_out_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.miss) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_pc) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rd_data[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_page_fault) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mmu_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mmu_req_vtag) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_addr) >> b) & 1u) != 0; }
for (size_t b = 0; b < 4; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_id) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_ready) >> b) & 1u) != 0; }
}

inline void unpack_ICacheV2_out_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_out_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.miss = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_pc = static_cast<wire32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rd_data[i0] = static_cast<wire32_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_page_fault = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mmu_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mmu_req_vtag = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_addr = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 4; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_id = static_cast<wire4_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_ready = static_cast<wire1_t>(tmp); }
}

static constexpr size_t ICacheV2_regs_t_BITS = (32 * 1) + (8 * (ICACHE_V2_SET_NUM)) + (1 * (ICACHE_V2_SET_NUM) * (ICACHE_V2_PLRU_BITS_PER_SET)) + (32 * 1) + (8 * 1) + (8 * 1) + (8 * 1) + (1 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (3 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (8 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH) * (ICACHE_V2_WORD_NUM)) + (1 * 1) + (32 * 1) + (12 * 1) + (8 * 1) + (32 * (ICACHE_V2_WAYS) * (ICACHE_V2_WORD_NUM)) + (20 * (ICACHE_V2_WAYS)) + (1 * (ICACHE_V2_WAYS)) + (1 * 1) + (8 * 1) + (12 * 1) + (32 * 1) + (8 * 1) + (32 * 1) + (2 * (ICACHE_V2_MSHR_NUM)) + (32 * (ICACHE_V2_MSHR_NUM)) + (1 * (ICACHE_V2_MSHR_NUM)) + (4 * (ICACHE_V2_MSHR_NUM)) + (1 * (ICACHE_V2_MSHR_NUM)) + (64 * (ICACHE_V2_MSHR_NUM) * (ICACHE_V2_WAITER_WORDS)) + (1 * (16)) + (1 * (16)) + (8 * (16)) + (1 * (16)) + (4 * 1) + (1 * 1) + (32 * 1) + (4 * 1) + (8 * 1);
inline void pack_ICacheV2_regs_t(const icache_module_v2_n::ICacheV2_regs_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.epoch_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rr_ptr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.plru_bits_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rand_seed_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_head_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_tail_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_count_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_pc_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 3; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_mshr_idx_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_valid_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_rob_idx_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_tag_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_pending_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_delay_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_rob_idx_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_seed_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_is_prefetch_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 4; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
for (size_t b = 0; b < 64; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_waiters_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_inflight_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_canceled_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_mshr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_mshr_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 4; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_rr_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_valid_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_addr_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 4; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_id_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_mshr_r) >> b) & 1u) != 0; }
}

inline void unpack_ICacheV2_regs_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_regs_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.epoch_r = static_cast<reg32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rr_ptr_r[i0] = static_cast<reg8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.plru_bits_r[i0][i1] = static_cast<reg1_t>(tmp); }
}
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rand_seed_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_head_r = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_tail_r = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_count_r = static_cast<reg8_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_valid_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_pc_r[i0] = static_cast<reg32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 3; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_state_r[i0] = static_cast<reg3_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_addr_r[i0] = static_cast<reg32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_mshr_idx_r[i0] = static_cast<reg8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_data_r[i0][i1] = static_cast<reg32_t>(tmp); }
}
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_valid_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_pc_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_index_r = static_cast<reg12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_rob_idx_r = static_cast<reg8_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_data_r[i0][i1] = static_cast<reg32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_tag_r[i0] = static_cast<reg20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_valid_r[i0] = static_cast<reg1_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_pending_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_delay_r = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_index_r = static_cast<reg12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_pc_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_rob_idx_r = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_seed_r = static_cast<reg32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_state_r[i0] = static_cast<reg2_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_line_addr_r[i0] = static_cast<reg32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_is_prefetch_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 4; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_r[i0] = static_cast<reg4_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_valid_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 64; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_waiters_r[i0][i1] = static_cast<reg64_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_inflight_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_canceled_r[i0] = static_cast<reg1_t>(tmp); }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_mshr_r[i0] = static_cast<reg8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_mshr_valid_r[i0] = static_cast<reg1_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 4; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_rr_r = static_cast<reg4_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_valid_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_addr_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 4; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_id_r = static_cast<reg4_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_mshr_r = static_cast<reg8_t>(tmp); }
}

static constexpr size_t ICacheV2_table_write_t_BITS = (1 * 1) + (12 * 1) + (8 * 1) + (32 * (ICACHE_V2_WORD_NUM)) + (20 * 1) + (1 * 1);
inline void pack_ICacheV2_table_write_t(const icache_module_v2_n::ICacheV2_table_write_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.we) >> b) & 1u) != 0; }
for (size_t b = 0; b < 12; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.way) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WORD_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.data[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tag) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.valid) >> b) & 1u) != 0; }
}

inline void unpack_ICacheV2_table_write_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_table_write_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.we = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 12; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set = static_cast<wire12_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.way = static_cast<wire8_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WORD_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.data[i0] = static_cast<wire32_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tag = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.valid = static_cast<wire1_t>(tmp); }
}

static constexpr size_t ICacheV2_in_t_BITS = (32 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (20 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (4 * 1) + (32 * (ICACHE_LINE_SIZE / 4));
inline void pack_ICacheV2_in_t(const icache_module_v2_n::ICacheV2_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pc) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.refetch) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.page_fault) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 4; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_id) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_data[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_ICacheV2_in_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pc = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.refetch = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.page_fault = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 4; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_id = static_cast<wire4_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_data[i0] = static_cast<wire32_t>(tmp); }
}
}

static constexpr size_t ICacheV2_lookup_in_t_BITS = (1 * 1) + (32 * (ICACHE_V2_WAYS) * (ICACHE_V2_WORD_NUM)) + (20 * (ICACHE_V2_WAYS)) + (1 * (ICACHE_V2_WAYS));
inline void pack_ICacheV2_lookup_in_t(const icache_module_v2_n::ICacheV2_lookup_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_resp_valid) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_data[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_tag[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_valid[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_ICacheV2_lookup_in_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_lookup_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_resp_valid = static_cast<wire1_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_data[i0][i1] = static_cast<wire32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_tag[i0] = static_cast<wire20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_valid[i0] = static_cast<wire1_t>(tmp); }
}
}

static constexpr size_t PI_WIDTH = ICacheV2_in_t_BITS + ICacheV2_regs_t_BITS + ICacheV2_lookup_in_t_BITS;
static constexpr size_t PO_WIDTH = ICacheV2_out_t_BITS + ICacheV2_regs_t_BITS + ICacheV2_table_write_t_BITS;

inline void pack_pi(const icache_module_v2_n::ICacheV2_IO_t &io, bool *pi) {
  size_t idx = 0;
  pack_ICacheV2_in_t(io.in, pi, idx);
  pack_ICacheV2_regs_t(io.regs, pi, idx);
  pack_ICacheV2_lookup_in_t(io.lookup_in, pi, idx);
}

inline void unpack_pi(const bool *pi, icache_module_v2_n::ICacheV2_IO_t &io) {
  size_t idx = 0;
  unpack_ICacheV2_in_t(pi, idx, io.in);
  unpack_ICacheV2_regs_t(pi, idx, io.regs);
  unpack_ICacheV2_lookup_in_t(pi, idx, io.lookup_in);
}

inline void pack_po(const icache_module_v2_n::ICacheV2_IO_t &io, bool *po) {
  size_t idx = 0;
  pack_ICacheV2_out_t(io.out, po, idx);
  pack_ICacheV2_regs_t(io.reg_write, po, idx);
  pack_ICacheV2_table_write_t(io.table_write, po, idx);
}

inline void unpack_po(const bool *po, icache_module_v2_n::ICacheV2_IO_t &io) {
  size_t idx = 0;
  unpack_ICacheV2_out_t(po, idx, io.out);
  unpack_ICacheV2_regs_t(po, idx, io.reg_write);
  unpack_ICacheV2_table_write_t(po, idx, io.table_write);
}

inline void pi_to_inputs(const bool *pi, icache_module_v2_n::ICacheV2_IO_t &io) {
  unpack_pi(pi, io);
}

inline void outputs_to_po(const icache_module_v2_n::ICacheV2_IO_t &io, bool *po) {
  pack_po(io, po);
}

template <typename ModuleT>
inline void eval_comb(ModuleT &module, const bool *pi, bool *po) {
  unpack_pi(pi, module.io);
  module.comb();
  pack_po(module.io, po);
}

} // namespace icache_v2_pi_po
} // namespace icache_module_v2_n
