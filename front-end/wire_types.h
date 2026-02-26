#ifndef FRONTEND_WIRE_TYPES_H
#define FRONTEND_WIRE_TYPES_H

#include <cstdint>

#include "BPU/BPU_configs.h"

constexpr int ceil_log2_u32(uint32_t value) {
  int width = 0;
  uint32_t threshold = 1;
  while (threshold < value) {
    threshold <<= 1;
    ++width;
  }
  return width;
}

using wire1_t = bool;
static constexpr int wire1_t_BITS = 1;
using wire2_t = uint8_t;
static constexpr int wire2_t_BITS = 2;
using wire3_t = uint8_t;
static constexpr int wire3_t_BITS = 3;
using wire4_t = uint8_t;
static constexpr int wire4_t_BITS = 4;
using wire8_t = uint8_t;
static constexpr int wire8_t_BITS = 8;
using wire9_t = uint16_t;
static constexpr int wire9_t_BITS = 9;
using wire11_t = uint16_t;
static constexpr int wire11_t_BITS = 11;
using wire12_t = uint16_t;
static constexpr int wire12_t_BITS = 12;
using wire32_t = uint32_t;
static constexpr int wire32_t_BITS = 32;

using br_type_t = wire3_t;
static constexpr int br_type_t_BITS = 3;
using pcpn_t = wire3_t;
static constexpr int pcpn_t_BITS = 3;
using predecode_type_t = wire2_t;
static constexpr int predecode_type_t_BITS = 2;

using tage_tag_t = wire8_t;
static constexpr int tage_tag_t_BITS = TAGE_TAG_WIDTH;
using tage_idx_t = wire12_t;
static constexpr int tage_idx_t_BITS = TAGE_IDX_WIDTH;

using btb_tag_t = wire8_t;
static constexpr int btb_tag_t_BITS = BTB_TAG_LEN;
using btb_idx_t = wire9_t;
static constexpr int btb_idx_t_BITS = BTB_IDX_LEN;
using btb_type_idx_t = wire12_t;
static constexpr int btb_type_idx_t_BITS = ceil_log2_u32(BTB_TYPE_ENTRY_NUM);
using bht_idx_t = wire11_t;
static constexpr int bht_idx_t_BITS = ceil_log2_u32(BHT_ENTRY_NUM);
using tc_idx_t = wire11_t;
static constexpr int tc_idx_t_BITS = ceil_log2_u32(TC_ENTRY_NUM);
using bht_hist_t = wire11_t;
static constexpr int bht_hist_t_BITS = ceil_log2_u32(BHT_ENTRY_NUM);

#endif
