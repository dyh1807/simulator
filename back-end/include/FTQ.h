#pragma once
#include "IO.h"
#include "config.h"
#include <cstdint>

struct FTQEntry {
  uint32_t start_pc;
  uint32_t next_pc; // Predicted Target of the block
  bool pred_taken_mask[FETCH_WIDTH];
  uint32_t tage_idx[FETCH_WIDTH][4]; // Moved from InstUop
  uint32_t tage_tag[FETCH_WIDTH][4];
  bool mid_pred[FETCH_WIDTH]; // For future use (mid-block prediction)
  bool alt_pred[FETCH_WIDTH];
  uint8_t altpcpn[FETCH_WIDTH];
  uint8_t pcpn[FETCH_WIDTH];
  bool valid;

  // Debug/Trace info
  uint64_t allocation_time;

  FTQEntry() {
    valid = false;
    start_pc = 0;
    next_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      pred_taken_mask[i] = false;
      mid_pred[i] = false;
      alt_pred[i] = false;
      altpcpn[i] = 0;
      pcpn[i] = 0;
      for (int j = 0; j < 4; j++) {
        tage_idx[i][j] = 0;
        tage_tag[i][j] = 0;
      }
    }
  }
};

class FTQLookupIO {
public:
  FTQEntry entries[FTQ_SIZE];
};
