#include "front_fifo.h"
#include <iostream>
#include <queue>
#include "frontend.h"

struct PTAB_entry {
  bool predict_dir[FETCH_WIDTH];
  uint32_t predict_next_fetch_address;
  uint32_t predict_base_pc[FETCH_WIDTH];
  // for TAGE update
  bool alt_pred[FETCH_WIDTH];
  uint8_t altpcpn[FETCH_WIDTH];
  uint8_t pcpn[FETCH_WIDTH];
  uint32_t tage_idx[FETCH_WIDTH][TN_MAX]; 
  uint32_t tage_tag[FETCH_WIDTH][TN_MAX]; 
  // 如果有，当前PTAB的下一次出队为dummy entry
  bool need_mini_flush; 
  bool dummy_entry;
};

bool dummy_entry = false;

static std::queue<PTAB_entry> ptab;

// 相对于看一眼reg
bool ptab_peek_mini_flush() {
  DEBUG_LOG_SMALL_4("ptab_peeking for pc=%x,need_mini_flush=%d\n", ptab.front().predict_base_pc[0], ptab.front().need_mini_flush);
  bool ret = ptab.front().need_mini_flush;
  ptab.front().need_mini_flush = false; // only once
  return ret;
}

void PTAB_top(struct PTAB_in *in, struct PTAB_out *out) {
  out->read_valid = false;
  if (in->reset) {
    while (!ptab.empty()) {
      ptab.pop();
    }
    out->full = false;
    out->empty = true;
    dummy_entry = false;
    return;
  }

  if (in->refetch) {
    while (!ptab.empty()) {
      ptab.pop();
    }
    DEBUG_LOG_SMALL_4("PTAB refetch\n");
    // we allow one write to PTAB in refetch cycle...
    dummy_entry = false;
  }
  // when there is new prediction, add it to PTAB
  if (in->write_enable) {
    if (ptab.size() >= PTAB_SIZE) {
      printf("[PTAB_TOP] ERROR!!: ptab.size() >= PTAB_SIZE\n");
      exit(1); // should not reach here
    }
    PTAB_entry entry;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      entry.predict_dir[i] = in->predict_dir[i];
      entry.predict_base_pc[i] = in->predict_base_pc[i];
      DEBUG_LOG_SMALL_4("PTAB in: predict_base_pc[%d]: %x\n", i, in->predict_base_pc[i]);
      entry.alt_pred[i] = in->alt_pred[i];
      entry.altpcpn[i] = in->altpcpn[i];
      entry.pcpn[i] = in->pcpn[i];
      for(int j = 0; j < TN_MAX; j++) { 
        entry.tage_idx[i][j] = in->tage_idx[i][j];
        entry.tage_tag[i][j] = in->tage_tag[i][j];
      }
    }
    entry.predict_next_fetch_address = in->predict_next_fetch_address;
    entry.need_mini_flush = in->need_mini_flush;
    entry.dummy_entry = false;
    DEBUG_LOG_SMALL_4("PTAB in: need_mini_flush=%d\n", in->need_mini_flush);
    ptab.push(entry);
    if (in->need_mini_flush) {
      // we need to push a dummy entry to PTAB
      PTAB_entry dummy_entry;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        dummy_entry.predict_dir[i] = false;
        dummy_entry.predict_base_pc[i] = 0;
        dummy_entry.alt_pred[i] = false;
        dummy_entry.altpcpn[i] = 0;
        dummy_entry.pcpn[i] = 0;
      }
      dummy_entry.predict_next_fetch_address = 0;
      dummy_entry.need_mini_flush = false;
      dummy_entry.dummy_entry = true;
      if(ptab.size() >= PTAB_SIZE) {
        printf("[PTAB_TOP] dummy entry push ERROR!!: ptab.size() >= PTAB_SIZE\n");
        exit(1); // should not reach here
      }
      ptab.push(dummy_entry);
    }
  }

  // output prediction
  if (in->read_enable && !ptab.empty()) {
    // if(dummy_entry) {
    //   out->dummy_entry = true;
    //   dummy_entry = false;
    //   DEBUG_LOG_SMALL_4("PTAB out: dummy_entry\n");
    // }
    // else {
      out->dummy_entry = ptab.front().dummy_entry;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        out->predict_dir[i] = ptab.front().predict_dir[i];
        out->predict_base_pc[i] = ptab.front().predict_base_pc[i];
        DEBUG_LOG_SMALL_4("PTAB out: predict_base_pc[%d]: %x\n", i, ptab.front().predict_base_pc[i]);
        out->alt_pred[i] = ptab.front().alt_pred[i];
        out->altpcpn[i] = ptab.front().altpcpn[i];
        out->pcpn[i] = ptab.front().pcpn[i];
        for(int j = 0; j < TN_MAX; j++) { 
          out->tage_idx[i][j] = ptab.front().tage_idx[i][j];
          out->tage_tag[i][j] = ptab.front().tage_tag[i][j];
        }
      }
      out->predict_next_fetch_address = ptab.front().predict_next_fetch_address;
      out->read_valid = true;
      // out->need_mini_flush = ptab.front().need_mini_flush;
      if(ptab.front().need_mini_flush) {
        dummy_entry = true;
      }
      ptab.pop();
    // }
  }
  // out->full = ptab.size() == PTAB_SIZE;
  out->full = ptab.size() >= (PTAB_SIZE - 1);
  out->empty = ptab.empty();
}
