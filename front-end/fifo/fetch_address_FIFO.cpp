#include "front_fifo.h"

#include <cstdio>
#include <queue>

// 简单行为级 FIFO，结构与 PTAB 模块接口风格保持一致
static std::queue<uint32_t> fetch_addr_fifo;

void fetch_address_FIFO_top(struct fetch_address_FIFO_in *in,
                            struct fetch_address_FIFO_out *out) {
  // reset 优先级最高
  if (in->reset) {
    while (!fetch_addr_fifo.empty()) {
      fetch_addr_fifo.pop();
    }
    out->full = false;
    out->empty = true;
    out->read_valid = false;
    out->fetch_address = 0;
    return;
  }

  // refetch（flush）清空，但允许本拍再写一条（与 PTAB 行为保持一致）
  if (in->refetch) {
    while (!fetch_addr_fifo.empty()) {
      fetch_addr_fifo.pop();
    }
    DEBUG_LOG_SMALL_4("fetch_address_FIFO refetch\n");
  }

  // 写入（BPU 侧）
  if (in->write_enable) {
    if (fetch_addr_fifo.size() >= FETCH_ADDR_FIFO_SIZE) {
      std::printf("[fetch_address_FIFO] ERROR: fifo overflow\n");
      std::exit(1);
    }
    
    DEBUG_LOG_SMALL_4("fetch_address_FIFO in: fetch_address: %x\n", in->fetch_address);
    fetch_addr_fifo.push(in->fetch_address);
  }

  // 默认本拍没有读出
  out->read_valid = false;
  out->fetch_address = 0;

  // 读出（I-cache 侧）
  if (in->read_enable && !fetch_addr_fifo.empty()) {
    out->fetch_address = fetch_addr_fifo.front();
    fetch_addr_fifo.pop();
    out->read_valid = true;
    DEBUG_LOG_SMALL_4("fetch_address_FIFO out: fetch_address: %x\n", out->fetch_address);
  }

  // for 2-Ahead
  out->full = (fetch_addr_fifo.size() >= (FETCH_ADDR_FIFO_SIZE - 1)); // soft full logic
  // out->full = (fetch_addr_fifo.size() == FETCH_ADDR_FIFO_SIZE); 
  out->empty = fetch_addr_fifo.empty();
}


