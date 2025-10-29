#pragma once
#include "IO.h"
#include "config.h"

class REN_IO {
public:
  Dec_Ren *dec2ren;
  Ren_Dec *ren2dec;

  Iss_Awake *iss_awake;

  Dec_Broadcast *dec_bcast;
  Prf_Awake *prf_awake;

  Ren_Dis *ren2dis;
  Dis_Ren *dis2ren;

  Rob_Broadcast *rob_bcast;
  Rob_Commit *rob_commit;
};

class Rename {
public:
  REN_IO io;

  Rename();
  void comb_select();
  void comb_rename(); // 重命名
  void comb_fire();
  void comb_wake();
  void comb_alloc(); // 分配寄存器
  void comb_branch();
  void comb_commit();
  void comb_pipeline();
  void comb_flush();
  void seq();

  reg7_t arch_RAT[ARF_NUM + 1];

  // register
  Inst_entry inst_r[FETCH_WIDTH];
  reg7_t spec_RAT[ARF_NUM + 1];
  reg7_t RAT_checkpoint[MAX_BR_NUM][ARF_NUM + 1];
  reg1_t free_vec[PRF_NUM];
  reg1_t alloc_checkpoint[MAX_BR_NUM][PRF_NUM];
  reg1_t busy_table[PRF_NUM];
  reg1_t spec_alloc[PRF_NUM]; // 处于speculative状态分配的寄存器

  Inst_entry inst_r_1[FETCH_WIDTH];
  wire7_t spec_RAT_1[ARF_NUM + 1];
  wire7_t RAT_checkpoint_1[MAX_BR_NUM][ARF_NUM + 1];
  reg1_t free_vec_1[PRF_NUM];
  reg1_t alloc_checkpoint_1[MAX_BR_NUM][PRF_NUM];
  reg1_t busy_table_1[PRF_NUM];
  reg1_t spec_alloc_1[PRF_NUM]; // 处于speculative状态分配的寄存器
};
