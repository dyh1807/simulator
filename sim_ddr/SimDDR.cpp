/**
 * @file SimDDR.cpp
 * @brief SimDDR Implementation - DDR Memory Simulator with AXI4 Interface
 *
 * Implementation with outstanding transaction support.
 * Uses queues to track multiple in-flight read/write transactions.
 */

#include "SimDDR.h"
#include <cstdio>
#include <cstring>

// Use the global p_memory from the simulator
extern uint32_t *p_memory;
extern long long sim_time;

namespace sim_ddr {

// ============================================================================
// Initialization
// ============================================================================
void SimDDR::init() {
  // Clear write channel state
  w_active = false;
  w_current = {};

  // Clear queues
  while (!w_resp_queue.empty())
    w_resp_queue.pop();
  while (!r_queue.empty())
    r_queue.pop();

  // Initialize IO outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata = 0;
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;
}

// ============================================================================
// Combinational Logic - Main Entry
// ============================================================================
void SimDDR::comb() {
  comb_write_channel();
  comb_read_channel();
}

// ============================================================================
// Combinational Logic - Write Channel (Outstanding Support)
// ============================================================================
void SimDDR::comb_write_channel() {
  // Default outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;

  // --- AW Channel: Accept new write address if no active transaction ---
  // (Can be enhanced to accept while processing W data, but keep simple for
  // now)
  if (!w_active) {
    io.aw.awready = true;
  }

  // --- W Channel: Accept write data if active transaction ---
  if (w_active && !w_current.data_done) {
    io.w.wready = true;
  }

  // --- B Channel: Send response if latency complete ---
  if (!w_resp_queue.empty()) {
    WriteRespPending &front =
        const_cast<WriteRespPending &>(w_resp_queue.front());
    if (front.latency_cnt >= SIM_DDR_LATENCY) {
      io.b.bvalid = true;
      io.b.bid = front.id;
      io.b.bresp = AXI_RESP_OKAY;
    }
  }
}

// ============================================================================
// Combinational Logic - Read Channel (Outstanding Support)
// ============================================================================
void SimDDR::comb_read_channel() {
  // Default outputs
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata = 0;
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;

  // --- AR Channel: Accept new read address if queue not full ---
  if (r_queue.size() < SIM_DDR_MAX_OUTSTANDING) {
    io.ar.arready = true;
  }

  // --- R Channel: Send data from front transaction if in data phase ---
  if (!r_queue.empty()) {
    ReadTransaction &front = const_cast<ReadTransaction &>(r_queue.front());
    if (front.in_data_phase) {
      uint32_t current_addr = front.addr + (front.beat_cnt << front.size);
      io.r.rvalid = true;
      io.r.rid = front.id;
      io.r.rdata = do_memory_read(current_addr);
      io.r.rresp = AXI_RESP_OKAY;
      io.r.rlast = (front.beat_cnt == front.len);
    }
  }
}

// ============================================================================
// Sequential Logic
// ============================================================================
void SimDDR::seq() {
  // ========== Write Channel Sequential Logic ==========

  // AW handshake: Start new write transaction
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

  // W handshake: Process write data
  if (io.w.wvalid && io.w.wready && w_active) {
    uint32_t current_addr =
        w_current.addr + (w_current.beat_cnt << w_current.size);
    do_memory_write(current_addr, io.w.wdata, io.w.wstrb);
    w_current.beat_cnt++;

    if (io.w.wlast) {
      // All data received, move to response queue
      w_current.data_done = true;
      WriteRespPending resp;
      resp.id = w_current.id;
      resp.latency_cnt = 0;
      w_resp_queue.push(resp);
      w_active = false;
    }
  }

  // B handshake: Complete response
  if (io.b.bvalid && io.b.bready) {
    w_resp_queue.pop();
  }

  // Increment latency counters for pending responses
  size_t resp_count = w_resp_queue.size();
  std::queue<WriteRespPending> temp_queue;
  while (!w_resp_queue.empty()) {
    WriteRespPending resp = w_resp_queue.front();
    w_resp_queue.pop();
    resp.latency_cnt++;
    temp_queue.push(resp);
  }
  w_resp_queue = temp_queue;

  // ========== Read Channel Sequential Logic ==========

  // AR handshake: Start new read transaction
  if (io.ar.arvalid && io.ar.arready) {
    ReadTransaction txn;
    txn.addr = io.ar.araddr;
    txn.id = io.ar.arid;
    txn.len = io.ar.arlen;
    txn.size = io.ar.arsize;
    txn.burst = io.ar.arburst;
    txn.beat_cnt = 0;
    txn.latency_cnt = 0;
    txn.in_data_phase = false;
    r_queue.push(txn);
  }

  // R handshake: Advance data beat
  if (io.r.rvalid && io.r.rready && !r_queue.empty()) {
    ReadTransaction &front = const_cast<ReadTransaction &>(r_queue.front());
    if (front.in_data_phase) {
      front.beat_cnt++;
      if (io.r.rlast) {
        // Transaction complete
        r_queue.pop();
      }
    }
  }

  // Update read queue: increment latency, transition to data phase
  size_t read_count = r_queue.size();
  std::queue<ReadTransaction> temp_r_queue;
  while (!r_queue.empty()) {
    ReadTransaction txn = r_queue.front();
    r_queue.pop();

    if (!txn.in_data_phase) {
      txn.latency_cnt++;
      if (txn.latency_cnt >= SIM_DDR_LATENCY) {
        txn.in_data_phase = true;
      }
    }
    temp_r_queue.push(txn);
  }
  r_queue = temp_r_queue;
}

// ============================================================================
// Helper Functions
// ============================================================================

void SimDDR::do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb) {
  // Convert byte address to word address
  uint32_t word_addr = addr >> 2;

  if (p_memory == nullptr) {
    return;
  }

  // Apply write strobe (byte enables)
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

  if (DCACHE_LOG) {
    printf("[SimDDR] Write: addr=0x%08x data=0x%08x wstrb=0x%x -> "
           "mem[0x%08x]=0x%08x\n",
           addr, data, wstrb, word_addr, p_memory[word_addr]);
  }
}

uint32_t SimDDR::do_memory_read(uint32_t addr) {
  // Convert byte address to word address
  uint32_t word_addr = addr >> 2;

  if (p_memory == nullptr) {
    return 0xDEADBEEF; // Error pattern
  }

  uint32_t data = p_memory[word_addr];

  if (DCACHE_LOG) {
    printf("[SimDDR] Read: addr=0x%08x -> mem[0x%08x]=0x%08x\n", addr,
           word_addr, data);
  }

  return data;
}

// ============================================================================
// Debug
// ============================================================================
void SimDDR::print_state() {
  printf("[SimDDR] Write: active=%d resp_pending=%zu\n", w_active,
         w_resp_queue.size());
  printf("[SimDDR] Read: queue_size=%zu\n", r_queue.size());
}

} // namespace sim_ddr
