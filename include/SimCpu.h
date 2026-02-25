#pragma once
#include "BackTop.h"
#include "FrontTop.h"
#include "MemSubsystem.h"
#include "front_IO.h"

class SimCpu {
  // 性能计数器
public:
  SimCpu() : back(&this->ctx), mem_subsystem(&this->ctx) {};
  BackTop back;
  FrontTop front;
  MemSubsystem mem_subsystem;
  SimContext ctx;
  // Oracle 模式下的一拍保留寄存，避免“后端当拍阻塞”导致前端指令丢失。
  bool oracle_pending_valid = false;
  front_top_out oracle_pending_out = {};

  void init();
  void restore_pc(uint32_t pc);
  void cycle();
  void front_cycle();
  void back2front_comb();
  uint32_t get_reg(uint8_t arch_idx) { return back.get_reg(arch_idx); }
  // 由 SimContext 在提交路径调用的本地辅助逻辑。
  void commit_sync(InstInfo *inst);
  void difftest_prepare(InstEntry *inst_entry, bool *skip);
};
