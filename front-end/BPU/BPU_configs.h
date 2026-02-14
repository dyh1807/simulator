#ifndef BPU_CONFIGS_H
#define BPU_CONFIGS_H
#include "config.h"

// SRAM delay configs
// #define SRAM_DELAY_ENABLE  // if not defined, no SRAM delay, back to Register
#define SRAM_DELAY_MIN 0 // n+1
#define SRAM_DELAY_MAX 0 // n+1

// BPU configs
#define SPECULATIVE_ON
#define BPU_BANK_NUM FETCH_WIDTH

#define BPU_TYPE_ENTRY_NUM 4096
#define BPU_TYPE_IDX_MASK (BPU_TYPE_ENTRY_NUM - 1)
#define Q_DEPTH 5000 // UQ depth

// TAGE configs
#define BASE_ENTRY_NUM 2048
#define GHR_LENGTH 256
#define TN_MAX 4
#define TN_ENTRY_NUM 4096
#define FH_N_MAX 3
#define TAGE_BASE_IDX_WIDTH 11 // log2(2048)
#define TAGE_TAG_WIDTH 8       // 8-bit tag
#define TAGE_IDX_WIDTH 12      // log2(4096)
#define TAGE_TAG_MASK ((1 << TAGE_TAG_WIDTH) - 1)
#define TAGE_IDX_MASK ((1 << TAGE_IDX_WIDTH) - 1)
#define TAGE_BASE_IDX_MASK ((1 << TAGE_BASE_IDX_WIDTH) - 1)

// BTB configs
#define BTB_ENTRY_NUM 512
#define BTB_TAG_LEN 8
#define BTB_WAY_NUM 4
#define BTB_TYPE_ENTRY_NUM 4096
#define BHT_ENTRY_NUM 2048
#define TC_ENTRY_NUM 2048

#define BTB_IDX_LEN 9 // log2(512)
#define BTB_IDX_MASK (BTB_ENTRY_NUM - 1)
#define BTB_TAG_MASK ((1 << BTB_TAG_LEN) - 1)
#define BTB_TYPE_IDX_MASK (BTB_TYPE_ENTRY_NUM - 1)
#define BHT_IDX_MASK (BHT_ENTRY_NUM - 1)
#define TC_ENTRY_MASK (TC_ENTRY_NUM - 1)

// Branch Types
#define BR_DIRECT 0 // only cond now
#define BR_CALL 1
#define BR_RET 2 // can merge...
#define BR_IDIRECT 3
#define BR_NONCTL 4
#define BR_JAL 5

// 2-Ahead Predictor configs
#define TWO_AHEAD_TABLE_SIZE 4096

#endif
