#pragma once
#include "IO.h"
#include "PreIduQueue.h"
#include "config.h"
class SimContext;

class IduIn {
public:
  PreIduIssueIO *issue;
  RenDecIO *ren2dec;
  RobBroadcastIO *rob_bcast;
  RobCommitIO *commit;
  ExuIdIO *exu2id; // [New] From Exu
};

class IduOut {
public:
  DecRenIO *dec2ren;
  DecBroadcastIO *dec_bcast;
  FTQLookupIO *ftq_lookup;
};

class Idu {
public:
  Idu(SimContext *ctx, int max_br = 1) {
    this->ctx = ctx;
    this->max_br_per_cycle = max_br;
    out.ftq_lookup = nullptr;
  }
  SimContext *ctx;
  int max_br_per_cycle;
  IduIn in;
  IduOut out;
  void decode(InstInfo &uop, uint32_t inst);

  void init();
  void comb_decode(); // 译码并分配tag
  void comb_branch(); // 分支处理
  void comb_fire();  // 发射握手与分支tag推进
  void comb_flush(); // flush处理
  void comb_release_tag(); // 释放分支tag
  void seq();              // 时钟跳变，状态更新

  IduIO get_hardware_io(); // 获取硬件级别 IO (Hardware Reference)

  // 状态
  reg<BR_TAG_WIDTH> tag_list[MAX_BR_NUM];
  reg<BR_TAG_WIDTH> enq_ptr;
  reg<BR_TAG_WIDTH> now_tag;
  reg<1> tag_vec[MAX_BR_NUM];
  ExuIdIO br_latch;

  // 下一周期状态
  wire<BR_TAG_WIDTH> tag_list_1[MAX_BR_NUM];
  wire<BR_TAG_WIDTH> enq_ptr_1;
  wire<BR_TAG_WIDTH> now_tag_1;
  wire<1> tag_vec_1[MAX_BR_NUM];
  ExuIdIO br_latch_1;
};
