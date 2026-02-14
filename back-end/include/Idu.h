#pragma once
#include "IO.h"
#include "config.h"
#include "FTQ.h"
class SimContext;

class IduIn {
public:
  FrontDecIO *front2dec;
  RenDecIO *ren2dec;
  RobBroadcastIO *rob_bcast;
  RobCommitIO *commit;
  ExuIdIO *exu2id; // [New] From Exu
};

class IduOut {
public:
  DecFrontIO *dec2front;
  DecRenIO *dec2ren;
  DecBroadcastIO *dec_bcast;
  FTQLookupIO *ftq_lookup;
};

class Idu {
public:
  Idu(SimContext *ctx, int max_br = 1) {
    this->ctx = ctx;
    this->max_br_per_cycle = max_br;
    out.ftq_lookup = &ftq_lookup;
  }
  SimContext *ctx;
  int max_br_per_cycle;
  IduIn in;
  IduOut out;
  void decode(InstInfo &uop, uint32_t inst);

  void init();
  void comb_decode();      // 译码并分配tag
  void comb_branch();      // 分支处理
  void comb_ftq_begin();
  void comb_fire();        // 与前端握手
  void comb_flush();       // flush处理
  void comb_ftq_commit_reclaim();
  void comb_release_tag(); // 释放分支tag
  void seq();              // 时钟跳变，状态更新

  IduIO get_hardware_io(); // 获取硬件级别 IO (Hardware Reference)

  // 状态
  reg<BR_TAG_WIDTH> tag_list[MAX_BR_NUM];
  reg<BR_TAG_WIDTH> enq_ptr;
  reg<BR_TAG_WIDTH> now_tag;
  reg<1> tag_vec[MAX_BR_NUM];

  // 下一周期状态
  wire<BR_TAG_WIDTH> tag_list_1[MAX_BR_NUM];
  wire<BR_TAG_WIDTH> enq_ptr_1;
  wire<BR_TAG_WIDTH> now_tag_1;
  wire<1> tag_vec_1[MAX_BR_NUM];

  // [New] Latch for Branch Result from Exu
  ExuIdIO br_latch;
  ExuIdIO br_latch_1;

  struct IbufEntry {
    bool valid = false;
    uint32_t inst = 0;
    uint32_t pc = 0;
    bool page_fault_inst = false;
    bool predict_dir = false;
    bool alt_pred = false;
    uint8_t altpcpn = 0;
    uint8_t pcpn = 0;
    uint32_t tage_idx[4] = {0, 0, 0, 0};
    uint32_t ftq_idx = 0;
    uint32_t ftq_offset = 0;
    bool ftq_is_last = false;
  };

  IbufEntry ibuf[IDU_INST_BUFFER_SIZE];
  int ibuf_head = 0;
  int ibuf_tail = 0;
  int ibuf_count = 0;

private:
  FTQ ftq;
  FTQAllocReqIO ftq_alloc_req;
  FTQAllocRespIO ftq_alloc_resp;
  FTQStatusIO ftq_status;
  FTQLookupIO ftq_lookup;
};
