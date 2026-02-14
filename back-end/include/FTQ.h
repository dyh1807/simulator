#pragma once
#include "IO.h"
#include "config.h"
#include <cstdint>
#include <vector>

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

class FTQIn {
public:
  RobCommitIO *rob_commit = nullptr;
  class FTQAllocReqIO *alloc_req = nullptr;
  bool flush_req = false;
  bool recover_req = false;
  int recover_tail = 0;
};

class FTQAllocReqIO {
public:
  bool valid = false;
  FTQEntry entry;
};

class FTQAllocRespIO {
public:
  bool success = false;
  int idx = -1;
};

class FTQStatusIO {
public:
  bool full = false;
  bool empty = true;
};

class FTQLookupIO {
public:
  FTQEntry entries[FTQ_SIZE];
};

class FTQOut {
public:
  FTQAllocRespIO *alloc_resp = nullptr;
  FTQStatusIO *status = nullptr;
  FTQLookupIO *lookup = nullptr;
};

class FTQ {
public:
  FTQ();
  FTQIn in;
  FTQOut out;

  void init();
  void comb_begin();
  int comb_alloc(const FTQEntry &entry); // Returns index
  void comb_alloc_req();
  void comb_status();
  void comb_pop(int pop_cnt);
  void comb_ctrl();
  void comb_commit_reclaim();
  void comb_recover(int new_tail);
  void comb_flush();
  void seq();

  FTQEntry &get(int idx);
  const FTQEntry &get(int idx) const;

  bool is_full() const;
  bool is_empty() const;

private:
  FTQEntry entries[FTQ_SIZE];
  FTQEntry entries_1[FTQ_SIZE];
  int head;   // Oldest
  int tail;   // Newest (allocation point)
  int count;
  int head_1;
  int tail_1;
  int count_1;
};
