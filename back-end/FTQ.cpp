#include "FTQ.h"

FTQ::FTQ() { init(); }

void FTQ::init() {
  in.rob_commit = nullptr;
  in.alloc_req = nullptr;
  in.flush_req = false;
  in.recover_req = false;
  in.recover_tail = 0;
  out.alloc_resp = nullptr;
  out.status = nullptr;
  out.lookup = nullptr;
  head = 0;
  tail = 0;
  count = 0;
  head_1 = 0;
  tail_1 = 0;
  count_1 = 0;
  for (int i = 0; i < FTQ_SIZE; i++) {
    entries[i] = FTQEntry();
    entries_1[i] = FTQEntry();
  }
}

void FTQ::comb_begin() {
  in.flush_req = false;
  in.recover_req = false;
  head_1 = head;
  tail_1 = tail;
  count_1 = count;
  for (int i = 0; i < FTQ_SIZE; i++) {
    entries_1[i] = entries[i];
    if (out.lookup != nullptr) {
      out.lookup->entries[i] = entries[i];
    }
  }
}

int FTQ::comb_alloc(const FTQEntry &entry) {
  if (count_1 >= FTQ_SIZE) {
    return -1;
  }
  int idx = tail_1;
  entries_1[idx] = entry;
  entries_1[idx].valid = true;
  tail_1 = (tail_1 + 1) % FTQ_SIZE;
  count_1++;
  return idx;
}

void FTQ::comb_alloc_req() {
  if (out.alloc_resp != nullptr) {
    out.alloc_resp->success = false;
    out.alloc_resp->idx = -1;
  }
  if (in.alloc_req == nullptr || out.alloc_resp == nullptr) {
    return;
  }
  if (!in.alloc_req->valid) {
    return;
  }
  int idx = comb_alloc(in.alloc_req->entry);
  out.alloc_resp->success = (idx >= 0);
  out.alloc_resp->idx = idx;
}

void FTQ::comb_status() {
  if (out.status == nullptr) {
    return;
  }
  out.status->full = is_full();
  out.status->empty = is_empty();
}

FTQEntry &FTQ::get(int idx) { return entries[idx]; }

const FTQEntry &FTQ::get(int idx) const { return entries[idx]; }

void FTQ::comb_pop(int pop_cnt) {
  if (pop_cnt <= 0) {
    return;
  }
  for (int i = 0; i < pop_cnt; i++) {
    if (count_1 <= 0) {
      break;
    }
    // Keep entry payload intact after reclaim.
    // Some consumers may still read by ftq_idx in the same/next cycle window.
    head_1 = (head_1 + 1) % FTQ_SIZE;
    count_1--;
  }
}

void FTQ::comb_commit_reclaim() {
  if (in.flush_req || in.recover_req) {
    return;
  }
  if (in.rob_commit == nullptr) {
    return;
  }
  int pop_cnt = 0;
  for (int i = 0; i < COMMIT_WIDTH; i++) {
    if (in.rob_commit->commit_entry[i].valid &&
        in.rob_commit->commit_entry[i].uop.ftq_is_last) {
      pop_cnt++;
    }
  }
  comb_pop(pop_cnt);
}

void FTQ::comb_ctrl() {
  if (in.flush_req) {
    comb_flush();
  } else if (in.recover_req) {
    comb_recover(in.recover_tail);
  }
}

void FTQ::comb_recover(int new_tail) {
  int normalized_tail = ((new_tail % FTQ_SIZE) + FTQ_SIZE) % FTQ_SIZE;
  tail_1 = normalized_tail;
  // Roll back pointers/count only; do not clear payload for dropped entries.
  // Entries are overwritten on next allocation.
  if (tail_1 >= head_1) {
    count_1 = tail_1 - head_1;
  } else {
    count_1 = FTQ_SIZE - (head_1 - tail_1);
  }
}

void FTQ::comb_flush() {
  head_1 = 0;
  tail_1 = 0;
  count_1 = 0;
  for (int i = 0; i < FTQ_SIZE; i++) {
    entries_1[i] = FTQEntry();
  }
}

void FTQ::seq() {
  head = head_1;
  tail = tail_1;
  count = count_1;
  for (int i = 0; i < FTQ_SIZE; i++) {
    entries[i] = entries_1[i];
    if (out.lookup != nullptr) {
      out.lookup->entries[i] = entries_1[i];
    }
  }
}

bool FTQ::is_full() const { return count >= FTQ_SIZE; }

bool FTQ::is_empty() const { return count == 0; }
