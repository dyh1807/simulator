#include "front_fifo.h"
#include "types.h"
#include <iostream>
#include <queue>
#include <array>
#include <cstdint>

struct FRONT2BACK_FIFO_entry {
  std::array<uint32_t, FETCH_WIDTH> fetch_group;
  std::array<bool, FETCH_WIDTH> page_fault_inst;
  std::array<bool, FETCH_WIDTH> inst_valid;
  std::array<bool, FETCH_WIDTH> predict_dir_corrected;
  uint32_t predict_next_fetch_address_corrected;
  std::array<uint32_t, FETCH_WIDTH> predict_base_pc;
  std::array<bool, FETCH_WIDTH> alt_pred;
  std::array<uint8_t, FETCH_WIDTH> altpcpn;
  std::array<uint8_t, FETCH_WIDTH> pcpn;
  uint32_t tage_idx[FETCH_WIDTH][TN_MAX]; 
  uint32_t tage_tag[FETCH_WIDTH][TN_MAX]; 
};

static std::queue<FRONT2BACK_FIFO_entry> front2back_fifo;
static SimContext *front2back_ctx = nullptr;

void front2back_fifo_set_context(SimContext *ctx) { front2back_ctx = ctx; }

void front2back_FIFO_top(struct front2back_FIFO_in *in, struct front2back_FIFO_out *out) {
  if (in->reset) {
    while (!front2back_fifo.empty()) {
      front2back_fifo.pop();
    }
    out->full = false;
    out->empty = true;
    return;
  }
  if (in->refetch) {
    while (!front2back_fifo.empty()) {
      front2back_fifo.pop();
    }
    // we allow write to happen during refetch...
  }
  if (in->write_enable) {
    if(front2back_fifo.size() >= FRONT2BACK_FIFO_SIZE) {
      printf("[FRONT2BACK_FIFO_TOP] ERROR!!: front2back_fifo.size() >= FRONT2BACK_FIFO_SIZE\n");
      exit(1); // should not reach here
    }
    FRONT2BACK_FIFO_entry entry;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      entry.fetch_group[i] = in->fetch_group[i];
      entry.page_fault_inst[i] = in->page_fault_inst[i];
      entry.inst_valid[i] = in->inst_valid[i];
      entry.predict_dir_corrected[i] = in->predict_dir_corrected[i];
      entry.predict_base_pc[i] = in->predict_base_pc[i];
      entry.alt_pred[i] = in->alt_pred[i];
      entry.altpcpn[i] = in->altpcpn[i];
      entry.pcpn[i] = in->pcpn[i];
      for(int j = 0; j < TN_MAX; j++) { 
        entry.tage_idx[i][j] = in->tage_idx[i][j];
        entry.tage_tag[i][j] = in->tage_tag[i][j];
      }
    }
    entry.predict_next_fetch_address_corrected = in->predict_next_fetch_address_corrected;
    front2back_fifo.push(entry);
  }

  if (in->read_enable) {
    if (front2back_ctx != nullptr) {
      front2back_ctx->perf.front2back_read_enable_cycle_total++;
    }
    if (front2back_fifo.empty()) {
      if (front2back_ctx != nullptr) {
        front2back_ctx->perf.front2back_read_empty_cycle_total++;
      }
    }
  }

  if (in->read_enable && !front2back_fifo.empty()) {
    if (front2back_ctx != nullptr) {
      front2back_ctx->perf.front2back_read_cycle_total++;
    }
    uint64_t valid_inst_num = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = front2back_fifo.front().fetch_group[i];
      out->page_fault_inst[i] = front2back_fifo.front().page_fault_inst[i];
      out->inst_valid[i] = front2back_fifo.front().inst_valid[i];
      valid_inst_num += out->inst_valid[i] ? 1 : 0;
      out->predict_dir_corrected[i] = front2back_fifo.front().predict_dir_corrected[i];
      out->predict_base_pc[i] = front2back_fifo.front().predict_base_pc[i];
      out->alt_pred[i] = front2back_fifo.front().alt_pred[i];
      out->altpcpn[i] = front2back_fifo.front().altpcpn[i];
      out->pcpn[i] = front2back_fifo.front().pcpn[i];
      for(int j = 0; j < TN_MAX; j++) { 
        out->tage_idx[i][j] = front2back_fifo.front().tage_idx[i][j];
        out->tage_tag[i][j] = front2back_fifo.front().tage_tag[i][j];
      }
    }
    out->predict_next_fetch_address_corrected = front2back_fifo.front().predict_next_fetch_address_corrected;
    front2back_fifo.pop();
    if (front2back_ctx != nullptr) {
      front2back_ctx->perf.front2back_fetched_inst_total += valid_inst_num;
    }
    out->front2back_FIFO_valid = true; // only valid when read is enabled and FIFO is not empty
  } else {
    out->front2back_FIFO_valid = false;
  }

  out->full = front2back_fifo.size() == FRONT2BACK_FIFO_SIZE;
  out->empty = front2back_fifo.empty();
}
