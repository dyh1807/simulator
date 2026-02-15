/**
 * @file SimDDR_AXI3.cpp
 * @brief SimDDR AXI3 (256-bit) implementation
 */

#include "SimDDR_AXI3.h"
#include <cstdio>

extern uint32_t *p_memory;
extern long long sim_time;

namespace sim_ddr_axi3 {

void SimDDR_AXI3::init() {
  w_active = false;
  w_current = {};
  while (!w_resp_queue.empty()) {
    w_resp_queue.pop();
  }

  r_active = false;
  r_current = {};

  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;

  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata.clear();
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;
}

void SimDDR_AXI3::comb_outputs() {
  comb_read_channel();
  comb_write_channel();
}

void SimDDR_AXI3::comb_inputs() {
  // No-op: this module computes outputs based on current io.* inputs
}

void SimDDR_AXI3::comb_write_channel() {
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;

  // Only allow one outstanding write (including response pending).
  if (!w_active && w_resp_queue.empty()) {
    io.aw.awready = true;
  }

  if (w_active && !w_current.data_done) {
    io.w.wready = true;
  }

  if (!w_resp_queue.empty()) {
    WriteRespPending &front =
        const_cast<WriteRespPending &>(w_resp_queue.front());
    if (front.latency_cnt >= SIM_DDR_AXI3_LATENCY) {
      io.b.bvalid = true;
      io.b.bid = front.id;
      io.b.bresp = AXI_RESP_OKAY;
    }
  }
}

void SimDDR_AXI3::comb_read_channel() {
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata.clear();
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;

  // Only allow one outstanding read (no interleaving).
  if (!r_active) {
    io.ar.arready = true;
  }

  if (r_active && r_current.in_data_phase && !r_current.complete) {
    uint32_t beat_addr = r_current.addr + (r_current.beat_cnt << r_current.size);
    for (int i = 0; i < AXI_DATA_WORDS; i++) {
      io.r.rdata[i] = do_memory_read(beat_addr + (i * 4));
    }
    io.r.rvalid = true;
    io.r.rid = r_current.id;
    io.r.rresp = AXI_RESP_OKAY;
    io.r.rlast = (r_current.beat_cnt == r_current.len);
  }
}

void SimDDR_AXI3::seq() {
  // ========== Write ==========
  if (io.aw.awvalid && io.aw.awready) {
    w_active = true;
    w_current.addr = io.aw.awaddr;
    w_current.id = io.aw.awid;
    w_current.len = io.aw.awlen;
    w_current.size = io.aw.awsize;
    w_current.burst = io.aw.awburst;
    w_current.beat_cnt = 0;
    w_current.data_done = false;
  }

  if (io.w.wvalid && io.w.wready && w_active) {
    uint32_t beat_addr = w_current.addr + (w_current.beat_cnt << w_current.size);
    uint32_t wstrb = io.w.wstrb;
    for (int i = 0; i < AXI_DATA_WORDS; i++) {
      uint8_t nibble = (wstrb >> (i * 4)) & 0xF;
      if (nibble) {
        do_memory_write(beat_addr + (i * 4), io.w.wdata[i], nibble);
      }
    }
    w_current.beat_cnt++;

    if (io.w.wlast) {
      w_current.data_done = true;
      WriteRespPending resp;
      resp.id = w_current.id;
      resp.latency_cnt = 0;
      w_resp_queue.push(resp);
      w_active = false;
    }
  }

  if (io.b.bvalid && io.b.bready) {
    w_resp_queue.pop();
  }

  // Increment latency counters for pending write responses
  std::queue<WriteRespPending> tmp;
  while (!w_resp_queue.empty()) {
    WriteRespPending resp = w_resp_queue.front();
    w_resp_queue.pop();
    resp.latency_cnt++;
    tmp.push(resp);
  }
  w_resp_queue = tmp;

  // ========== Read ==========
  if (io.ar.arvalid && io.ar.arready) {
    r_active = true;
    r_current.addr = io.ar.araddr;
    r_current.id = io.ar.arid;
    r_current.len = io.ar.arlen;
    r_current.size = io.ar.arsize;
    r_current.burst = io.ar.arburst;
    r_current.beat_cnt = 0;
    r_current.latency_cnt = 0;
    r_current.in_data_phase = false;
    r_current.complete = false;
  }

  if (io.r.rvalid && io.r.rready && r_active) {
    if (io.r.rlast) {
      r_current.complete = true;
      r_active = false;
    } else {
      r_current.beat_cnt++;
    }
  }

  if (r_active && !r_current.in_data_phase) {
    r_current.latency_cnt++;
    if (r_current.latency_cnt >= SIM_DDR_AXI3_LATENCY) {
      r_current.in_data_phase = true;
    }
  }
}

void SimDDR_AXI3::do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb) {
  uint32_t word_addr = addr >> 2;
  if (p_memory == nullptr) {
    return;
  }

  uint32_t old_data = p_memory[word_addr];
  uint32_t mask = 0;
  if (wstrb & 0x1)
    mask |= 0x000000FF;
  if (wstrb & 0x2)
    mask |= 0x0000FF00;
  if (wstrb & 0x4)
    mask |= 0x00FF0000;
  if (wstrb & 0x8)
    mask |= 0xFF000000;

  p_memory[word_addr] = (data & mask) | (old_data & ~mask);
}

uint32_t SimDDR_AXI3::do_memory_read(uint32_t addr) {
  uint32_t word_addr = addr >> 2;
  if (p_memory == nullptr) {
    return 0xDEADBEEF;
  }
  return p_memory[word_addr];
}

void SimDDR_AXI3::print_state() {
  printf("[SimDDR_AXI3] t=%lld w_active=%d w_resp=%zu r_active=%d\n", sim_time,
         w_active, w_resp_queue.size(), r_active);
}

} // namespace sim_ddr_axi3

