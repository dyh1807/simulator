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

  void init();
  void restore_pc(uint32_t pc);
  void cycle();
  void front_cycle();
  void back2front_comb();
  uint32_t get_reg(uint8_t arch_idx) { return back.get_reg(arch_idx); }
};
