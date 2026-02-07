#pragma once
#include <cstddef>
#include <cstdint>
#include "icache_module.h"

namespace icache_module_n {
namespace icache_v1_pi_po {

static constexpr size_t ICache_in_t_BITS = (32 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (20 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (1 * 1) + (4 * 1) + (32 * (ICACHE_LINE_SIZE / 4));
inline void pack_ICache_in_t(const icache_module_n::ICache_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pc) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_req_valid) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ifu_resp_ready) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.refetch) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.flush) >> b) & 1u) != 0; }
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

inline void unpack_ICache_in_t(const bool *bits, size_t &idx, icache_module_n::ICache_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pc = static_cast<wire32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_req_valid = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ifu_resp_ready = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.refetch = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.flush = static_cast<wire1_t>(tmp); }
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

static constexpr size_t ICache_regs_t_BITS = (1 * 1) + (32 * (ICACHE_V1_WAYS) * (ICACHE_V1_WORD_NUM)) + (20 * (ICACHE_V1_WAYS)) + (1 * (ICACHE_V1_WAYS)) + (32 * 1) + (7 * 1) + (2 * 1) + (1 * 1) + (32 * (ICACHE_LINE_SIZE / 4)) + (8 * 1) + (20 * 1) + (1 * 1) + (7 * 1) + (32 * 1);
inline void pack_ICache_regs_t(const icache_module_n::ICache_regs_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_valid_r) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V1_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_cache_set_data_r[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_cache_set_tag_r[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_cache_set_valid_r[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_pc_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 7; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.pipe_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 2; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.state) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_axi_state) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.mem_resp_data_r[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.replace_idx) >> b) & 1u) != 0; }
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.ppn_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_pending_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 7; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_index_r) >> b) & 1u) != 0; }
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_pc_r) >> b) & 1u) != 0; }
}

inline void unpack_ICache_regs_t(const bool *bits, size_t &idx, icache_module_n::ICache_regs_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_valid_r = static_cast<reg1_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V1_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_cache_set_data_r[i0][i1] = static_cast<reg32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_cache_set_tag_r[i0] = static_cast<reg20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_cache_set_valid_r[i0] = static_cast<reg1_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_pc_r = static_cast<reg32_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 7; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.pipe_index_r = static_cast<reg7_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 2; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.state = static_cast<reg2_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_axi_state = static_cast<reg1_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.mem_resp_data_r[i0] = static_cast<reg32_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.replace_idx = static_cast<reg8_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.ppn_r = static_cast<reg20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_pending_r = static_cast<reg1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 7; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_index_r = static_cast<reg7_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_pc_r = static_cast<reg32_t>(tmp); }
}

static constexpr size_t ICache_lookup_in_t_BITS = (1 * 1) + (32 * (ICACHE_V1_WAYS) * (ICACHE_V1_WORD_NUM)) + (20 * (ICACHE_V1_WAYS)) + (1 * (ICACHE_V1_WAYS));
inline void pack_ICache_lookup_in_t(const icache_module_n::ICache_lookup_in_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_resp_valid) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V1_WORD_NUM); ++i1) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_data[i0][i1]) >> b) & 1u) != 0; }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_tag[i0]) >> b) & 1u) != 0; }
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.lookup_set_valid[i0]) >> b) & 1u) != 0; }
}
}

inline void unpack_ICache_lookup_in_t(const bool *bits, size_t &idx, icache_module_n::ICache_lookup_in_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_resp_valid = static_cast<wire1_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
for (size_t i1 = 0; i1 < (ICACHE_V1_WORD_NUM); ++i1) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_data[i0][i1] = static_cast<wire32_t>(tmp); }
}
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_tag[i0] = static_cast<wire20_t>(tmp); }
}
for (size_t i0 = 0; i0 < (ICACHE_V1_WAYS); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.lookup_set_valid[i0] = static_cast<wire1_t>(tmp); }
}
}

static constexpr size_t ICache_out_t_BITS = (1 * 1) + (1 * 1) + (1 * 1) + (32 * 1) + (32 * (ICACHE_LINE_SIZE / 4)) + (1 * 1) + (1 * 1) + (1 * 1) + (20 * 1) + (1 * 1) + (32 * 1) + (4 * 1) + (1 * 1);
inline void pack_ICache_out_t(const icache_module_n::ICache_out_t &v, bool *bits, size_t &idx) {
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

inline void unpack_ICache_out_t(const bool *bits, size_t &idx, icache_module_n::ICache_out_t &v) {
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

static constexpr size_t ICache_table_write_t_BITS = (1 * 1) + (7 * 1) + (8 * 1) + (32 * (ICACHE_LINE_SIZE / 4)) + (20 * 1) + (1 * 1);
inline void pack_ICache_table_write_t(const icache_module_n::ICache_table_write_t &v, bool *bits, size_t &idx) {
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.we) >> b) & 1u) != 0; }
for (size_t b = 0; b < 7; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.index) >> b) & 1u) != 0; }
for (size_t b = 0; b < 8; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.way) >> b) & 1u) != 0; }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
for (size_t b = 0; b < 32; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.data[i0]) >> b) & 1u) != 0; }
}
for (size_t b = 0; b < 20; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.tag) >> b) & 1u) != 0; }
for (size_t b = 0; b < 1; ++b) { bits[idx++] = ((static_cast<uint64_t>(v.valid) >> b) & 1u) != 0; }
}

inline void unpack_ICache_table_write_t(const bool *bits, size_t &idx, icache_module_n::ICache_table_write_t &v) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.we = static_cast<wire1_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 7; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.index = static_cast<wire7_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 8; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.way = static_cast<wire8_t>(tmp); }
for (size_t i0 = 0; i0 < (ICACHE_LINE_SIZE / 4); ++i0) {
{ uint64_t tmp = 0; for (size_t b = 0; b < 32; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.data[i0] = static_cast<wire32_t>(tmp); }
}
{ uint64_t tmp = 0; for (size_t b = 0; b < 20; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.tag = static_cast<wire20_t>(tmp); }
{ uint64_t tmp = 0; for (size_t b = 0; b < 1; ++b) { if (bits[idx++]) tmp |= (uint64_t(1) << b); } v.valid = static_cast<wire1_t>(tmp); }
}

static constexpr size_t PI_WIDTH = ICache_in_t_BITS + ICache_regs_t_BITS + ICache_lookup_in_t_BITS;
static constexpr size_t PO_WIDTH = ICache_out_t_BITS + ICache_regs_t_BITS + ICache_table_write_t_BITS;

inline void pack_pi(const icache_module_n::ICache_IO_t &io, bool *pi) {
  size_t idx = 0;
  pack_ICache_in_t(io.in, pi, idx);
  pack_ICache_regs_t(io.regs, pi, idx);
  pack_ICache_lookup_in_t(io.lookup_in, pi, idx);
}

inline void unpack_pi(const bool *pi, icache_module_n::ICache_IO_t &io) {
  size_t idx = 0;
  unpack_ICache_in_t(pi, idx, io.in);
  unpack_ICache_regs_t(pi, idx, io.regs);
  unpack_ICache_lookup_in_t(pi, idx, io.lookup_in);
}

inline void pack_po(const icache_module_n::ICache_IO_t &io, bool *po) {
  size_t idx = 0;
  pack_ICache_out_t(io.out, po, idx);
  pack_ICache_regs_t(io.reg_write, po, idx);
  pack_ICache_table_write_t(io.table_write, po, idx);
}

inline void unpack_po(const bool *po, icache_module_n::ICache_IO_t &io) {
  size_t idx = 0;
  unpack_ICache_out_t(po, idx, io.out);
  unpack_ICache_regs_t(po, idx, io.reg_write);
  unpack_ICache_table_write_t(po, idx, io.table_write);
}

inline void pi_to_inputs(const bool *pi, icache_module_n::ICache_IO_t &io) {
  unpack_pi(pi, io);
}

inline void outputs_to_po(const icache_module_n::ICache_IO_t &io, bool *po) {
  pack_po(io, po);
}

template <typename ModuleT>
inline void eval_comb(ModuleT &module, const bool *pi, bool *po) {
  unpack_pi(pi, module.io);
  module.comb();
  pack_po(module.io, po);
}

} // namespace icache_v1_pi_po
} // namespace icache_module_n
