#pragma once
#include <cstddef>
#include <cstdint>
#include "icache_module_v2.h"

namespace icache_module_v2_n {
namespace icache_v2_pi_po {

static constexpr size_t ICacheV2_out_t_BITS = (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (32 * (ICACHE_LINE_SIZE / 4)) + (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (1 * 1) + (32 * 1) + (8 * 1) + (1 * 1);
inline void pack_ICacheV2_out_t(const icache_module_v2_n::ICacheV2_out_t &v, bool *bits, size_t &idx) {
bits[idx++] = v.miss;
bits[idx++] = v.ifu_resp_valid;
bits[idx++] = v.ifu_req_ready;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_pc) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rd_data[i0]) >> b) & 1u) != 0; }
}
bits[idx++] = v.ifu_page_fault;
bits[idx++] = v.ppn_ready;
bits[idx++] = v.mmu_req_valid;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mmu_req_vtag) >> b) & 1u) != 0; }
bits[idx++] = v.mem_req_valid;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_addr) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_req_id) >> b) & 1u) != 0; }
bits[idx++] = v.mem_resp_ready;
}

inline void unpack_ICacheV2_out_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_out_t &v) {
v.miss = bits[idx++];
v.ifu_resp_valid = bits[idx++];
v.ifu_req_ready = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_pc = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rd_data[i0] = static_cast<uint32_t>(tmp); }
}
v.ifu_page_fault = bits[idx++];
v.ppn_ready = bits[idx++];
v.mmu_req_valid = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mmu_req_vtag = static_cast<uint32_t>(tmp); }
v.mem_req_valid = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_addr = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_req_id = static_cast<uint8_t>(tmp); }
v.mem_resp_ready = bits[idx++];
}

static constexpr size_t ICacheV2_reg_write_t_BITS = (32 * 1) + (32 * (ICACHE_V2_SET_NUM)) + (8 * (ICACHE_V2_SET_NUM) * (ICACHE_V2_PLRU_BITS_PER_SET)) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (8 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (8 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH) * (ICACHE_V2_WORD_NUM)) + (1 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * (ICACHE_V2_WAYS) * (ICACHE_V2_WORD_NUM)) + (32 * (ICACHE_V2_WAYS)) + (1 * (ICACHE_V2_WAYS)) + (1 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (8 * (ICACHE_V2_MSHR_NUM)) + (32 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (64 * (ICACHE_V2_MSHR_NUM) * (ICACHE_V2_WAITER_WORDS)) + (1 * (16)) + (1 * (16)) + (8 * (16)) + (1 * (16)) + (8 * 1) + (1 * 1) + (32 * 1) + (8 * 1) + (8 * 1);
inline void pack_ICacheV2_reg_write_t(const icache_module_v2_n::ICacheV2_reg_write_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.epoch_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rr_ptr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.plru_bits_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rand_seed_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_head_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_tail_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_count_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_pc_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_mshr_idx_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
bits[idx++] = v.lookup_valid_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_rob_idx_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_tag_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
bits[idx++] = v.set_valid_r[i0];
}
bits[idx++] = v.sram_pending_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_delay_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_rob_idx_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_seed_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_is_prefetch_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
for (size_t b = 0; b < 64; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_waiters_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_inflight_r[i0];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_canceled_r[i0];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_mshr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_mshr_valid_r[i0];
}
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_rr_r) >> b) & 1u) != 0; }
bits[idx++] = v.memreq_latched_valid_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_addr_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_id_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_mshr_r) >> b) & 1u) != 0; }
}

inline void unpack_ICacheV2_reg_write_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_reg_write_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.epoch_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rr_ptr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.plru_bits_r[i0][i1] = static_cast<uint8_t>(tmp); }
}
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rand_seed_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_head_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_tail_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_count_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_valid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_pc_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_state_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_addr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_mshr_idx_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_data_r[i0][i1] = static_cast<uint32_t>(tmp); }
}
}
v.lookup_valid_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_pc_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_index_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_rob_idx_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_data_r[i0][i1] = static_cast<uint32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_tag_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
v.set_valid_r[i0] = bits[idx++];
}
v.sram_pending_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_delay_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_index_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_pc_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_rob_idx_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_seed_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_state_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_line_addr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_is_prefetch_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_valid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 64; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_waiters_r[i0][i1] = static_cast<uint64_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_inflight_r[i0] = bits[idx++];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_canceled_r[i0] = bits[idx++];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_mshr_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_mshr_valid_r[i0] = bits[idx++];
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_rr_r = static_cast<uint8_t>(tmp); }
v.memreq_latched_valid_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_addr_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_id_r = static_cast<uint8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_mshr_r = static_cast<uint8_t>(tmp); }
}

static constexpr size_t ICacheV2_lookup_in_t_BITS = (1 * 1) + (32 * (ICACHE_V2_WAYS) * (ICACHE_V2_WORD_NUM)) + (32 * (ICACHE_V2_WAYS)) + (1 * (ICACHE_V2_WAYS));
inline void pack_ICacheV2_lookup_in_t(const icache_module_v2_n::ICacheV2_lookup_in_t &v, bool *bits, size_t &idx) {
bits[idx++] = v.lookup_resp_valid;
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_data[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_tag[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
bits[idx++] = v.lookup_set_valid[i0];
}
}

inline void unpack_ICacheV2_lookup_in_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_lookup_in_t &v) {
v.lookup_resp_valid = bits[idx++];
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_data[i0][i1] = static_cast<uint32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_tag[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
v.lookup_set_valid[i0] = bits[idx++];
}
}

static constexpr size_t ICacheV2_regs_t_BITS = (32 * 1) + (32 * (ICACHE_V2_SET_NUM)) + (8 * (ICACHE_V2_SET_NUM) * (ICACHE_V2_PLRU_BITS_PER_SET)) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (8 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (8 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH)) + (32 * (ICACHE_V2_ROB_DEPTH) * (ICACHE_V2_WORD_NUM)) + (1 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * (ICACHE_V2_WAYS) * (ICACHE_V2_WORD_NUM)) + (32 * (ICACHE_V2_WAYS)) + (1 * (ICACHE_V2_WAYS)) + (1 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (32 * 1) + (8 * (ICACHE_V2_MSHR_NUM)) + (32 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (8 * (ICACHE_V2_MSHR_NUM)) + (64 * (ICACHE_V2_MSHR_NUM) * (ICACHE_V2_WAITER_WORDS)) + (1 * (16)) + (1 * (16)) + (8 * (16)) + (1 * (16)) + (8 * 1) + (1 * 1) + (32 * 1) + (8 * 1) + (8 * 1);
inline void pack_ICacheV2_regs_t(const icache_module_v2_n::ICacheV2_regs_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.epoch_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rr_ptr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.plru_bits_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rand_seed_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_head_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_tail_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_count_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_pc_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_mshr_idx_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.rob_line_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
bits[idx++] = v.lookup_valid_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_rob_idx_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set_tag_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
bits[idx++] = v.set_valid_r[i0];
}
bits[idx++] = v.sram_pending_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_delay_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_rob_idx_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.sram_seed_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_state_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_line_addr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_is_prefetch_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_txid_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
for (size_t b = 0; b < 64; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mshr_waiters_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_inflight_r[i0];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_canceled_r[i0];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_mshr_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
bits[idx++] = v.txid_mshr_valid_r[i0];
}
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.txid_rr_r) >> b) & 1u) != 0; }
bits[idx++] = v.memreq_latched_valid_r;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_addr_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_id_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.memreq_latched_mshr_r) >> b) & 1u) != 0; }
}

inline void unpack_ICacheV2_regs_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_regs_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.epoch_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rr_ptr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_SET_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_PLRU_BITS_PER_SET); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.plru_bits_r[i0][i1] = static_cast<uint8_t>(tmp); }
}
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rand_seed_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_head_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_tail_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_count_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_valid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_pc_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_state_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_addr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_mshr_idx_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_ROB_DEPTH); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.rob_line_data_r[i0][i1] = static_cast<uint32_t>(tmp); }
}
}
v.lookup_valid_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_pc_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_index_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_rob_idx_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_data_r[i0][i1] = static_cast<uint32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set_tag_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_WAYS); ++i0) {
v.set_valid_r[i0] = bits[idx++];
}
v.sram_pending_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_delay_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_index_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_pc_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_rob_idx_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.sram_seed_r = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_state_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_line_addr_r[i0] = static_cast<uint32_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_is_prefetch_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_txid_valid_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V2_MSHR_NUM); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V2_WAITER_WORDS); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 64; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mshr_waiters_r[i0][i1] = static_cast<uint64_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_inflight_r[i0] = bits[idx++];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_canceled_r[i0] = bits[idx++];
}
for (size_t i0 = 0; i0 < (16); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_mshr_r[i0] = static_cast<uint8_t>(tmp); }
}
for (size_t i0 = 0; i0 < (16); ++i0) {
v.txid_mshr_valid_r[i0] = bits[idx++];
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.txid_rr_r = static_cast<uint8_t>(tmp); }
v.memreq_latched_valid_r = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_addr_r = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_id_r = static_cast<uint8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.memreq_latched_mshr_r = static_cast<uint8_t>(tmp); }
}

static constexpr size_t ICacheV2_table_write_t_BITS = (1 * 1) + (32 * 1) + (32 * 1) + (32 * (ICACHE_V2_WORD_NUM)) + (32 * 1) + (1 * 1);
inline void pack_ICacheV2_table_write_t(const icache_module_v2_n::ICacheV2_table_write_t &v, bool *bits, size_t &idx) {
bits[idx++] = v.we;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.set) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.way) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V2_WORD_NUM); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.data[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tag) >> b) & 1u) != 0; }
bits[idx++] = v.valid;
}

inline void unpack_ICacheV2_table_write_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_table_write_t &v) {
v.we = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.set = static_cast<uint32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.way = static_cast<uint32_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V2_WORD_NUM); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.data[i0] = static_cast<uint32_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tag = static_cast<uint32_t>(tmp); }
v.valid = bits[idx++];
}

static constexpr size_t ICacheV2_in_t_BITS = (32 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (8 * 1) + (32 * (ICACHE_LINE_SIZE / 4));
inline void pack_ICacheV2_in_t(const icache_module_v2_n::ICacheV2_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pc) >> b) & 1u) != 0; }
bits[idx++] = v.ifu_req_valid;
bits[idx++] = v.ifu_resp_ready;
bits[idx++] = v.refetch;
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn) >> b) & 1u) != 0; }
bits[idx++] = v.ppn_valid;
bits[idx++] = v.page_fault;
bits[idx++] = v.mem_req_ready;
bits[idx++] = v.mem_resp_valid;
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_id) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_data[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_ICacheV2_in_t(const bool *bits, size_t &idx, icache_module_v2_n::ICacheV2_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pc = static_cast<uint32_t>(tmp); }
v.ifu_req_valid = bits[idx++];
v.ifu_resp_ready = bits[idx++];
v.refetch = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn = static_cast<uint32_t>(tmp); }
v.ppn_valid = bits[idx++];
v.page_fault = bits[idx++];
v.mem_req_ready = bits[idx++];
v.mem_resp_valid = bits[idx++];
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_id = static_cast<uint8_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_data[i0] = static_cast<uint32_t>(tmp); }
}
}

static constexpr size_t PI_WIDTH = ICacheV2_in_t_BITS + ICacheV2_regs_t_BITS + ICacheV2_lookup_in_t_BITS;
static constexpr size_t PO_WIDTH = ICacheV2_out_t_BITS + ICacheV2_reg_write_t_BITS + ICacheV2_table_write_t_BITS;

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
  pack_ICacheV2_reg_write_t(io.reg_write, po, idx);
  pack_ICacheV2_table_write_t(io.table_write, po, idx);
}

inline void unpack_po(const bool *po, icache_module_v2_n::ICacheV2_IO_t &io) {
  size_t idx = 0;
  unpack_ICacheV2_out_t(po, idx, io.out);
  unpack_ICacheV2_reg_write_t(po, idx, io.reg_write);
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
