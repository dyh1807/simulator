/**
 * @file SimDDR.cpp
 * @brief SimDDR Implementation - DDR Memory Simulator with AXI4 Interface
 *
 * Implementation follows the simulator's comb/seq pattern:
 * - comb(): Combinational logic, calculates next state values
 * - seq(): Sequential logic, updates registers on clock edge
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
  // Write channel initialization
  w_state = WriteState::IDLE;
  w_state_next = WriteState::IDLE;
  w_addr = 0;
  w_len = 0;
  w_size = 0;
  w_burst = 0;
  w_beat_cnt = 0;
  w_latency_cnt = 0;

  w_addr_next = 0;
  w_len_next = 0;
  w_size_next = 0;
  w_burst_next = 0;
  w_beat_cnt_next = 0;
  w_latency_cnt_next = 0;

  // Read channel initialization
  r_state = ReadState::IDLE;
  r_state_next = ReadState::IDLE;
  r_addr = 0;
  r_len = 0;
  r_size = 0;
  r_burst = 0;
  r_beat_cnt = 0;
  r_latency_cnt = 0;

  r_addr_next = 0;
  r_len_next = 0;
  r_size_next = 0;
  r_burst_next = 0;
  r_beat_cnt_next = 0;
  r_latency_cnt_next = 0;

  // Initialize IO outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bresp = AXI_RESP_OKAY;
  io.ar.arready = false;
  io.r.rvalid = false;
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
// Combinational Logic - Write Channel
// ============================================================================
void SimDDR::comb_write_channel() {
  // Default: maintain current values
  w_state_next = w_state;
  w_addr_next = w_addr;
  w_len_next = w_len;
  w_size_next = w_size;
  w_burst_next = w_burst;
  w_beat_cnt_next = w_beat_cnt;
  w_latency_cnt_next = w_latency_cnt;

  // Default outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bresp = AXI_RESP_OKAY;

  switch (w_state) {
  case WriteState::IDLE:
    // Ready to accept new address
    io.aw.awready = true;

    if (io.aw.awvalid && io.aw.awready) {
      // Latch address and control signals
      w_addr_next = io.aw.awaddr;
      w_len_next = io.aw.awlen;
      w_size_next = io.aw.awsize;
      w_burst_next = io.aw.awburst;
      w_beat_cnt_next = 0;
      w_state_next = WriteState::DATA;
    }
    break;

  case WriteState::DATA:
    // Ready to accept write data
    io.w.wready = true;

    if (io.w.wvalid && io.w.wready) {
      // Increment beat counter (write happens in seq())
      w_beat_cnt_next = w_beat_cnt + 1;

      if (io.w.wlast) {
        // Last beat received, go to response phase
        w_latency_cnt_next = 0;
        w_state_next = WriteState::RESP;
      }
    }
    break;

  case WriteState::RESP:
    // Send write response after latency
    if (w_latency_cnt >= SIM_DDR_LATENCY) {
      io.b.bvalid = true;
      io.b.bresp = AXI_RESP_OKAY;

      if (io.b.bready) {
        // Response accepted, return to idle
        w_state_next = WriteState::IDLE;
      }
    } else {
      w_latency_cnt_next = w_latency_cnt + 1;
    }
    break;

  default:
    w_state_next = WriteState::IDLE;
    break;
  }
}

// ============================================================================
// Combinational Logic - Read Channel
// ============================================================================
void SimDDR::comb_read_channel() {
  // Default: maintain current values
  r_state_next = r_state;
  r_addr_next = r_addr;
  r_len_next = r_len;
  r_size_next = r_size;
  r_burst_next = r_burst;
  r_beat_cnt_next = r_beat_cnt;
  r_latency_cnt_next = r_latency_cnt;

  // Default outputs
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rdata = 0;
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;

  switch (r_state) {
  case ReadState::IDLE:
    // Ready to accept new address
    io.ar.arready = true;

    if (io.ar.arvalid && io.ar.arready) {
      // Latch address and control signals
      r_addr_next = io.ar.araddr;
      r_len_next = io.ar.arlen;
      r_size_next = io.ar.arsize;
      r_burst_next = io.ar.arburst;
      r_beat_cnt_next = 0;
      r_latency_cnt_next = 0;
      r_state_next = ReadState::LATENCY;
    }
    break;

  case ReadState::LATENCY:
    // Wait for memory latency
    if (r_latency_cnt >= SIM_DDR_LATENCY) {
      r_state_next = ReadState::DATA;
    } else {
      r_latency_cnt_next = r_latency_cnt + 1;
    }
    break;

  case ReadState::DATA:
    // Send read data
    {
      uint32_t current_addr = r_addr + (r_beat_cnt << r_size);
      io.r.rvalid = true;
      io.r.rdata = do_memory_read(current_addr);
      io.r.rresp = AXI_RESP_OKAY;
      io.r.rlast = (r_beat_cnt == r_len);

      if (io.r.rready) {
        r_beat_cnt_next = r_beat_cnt + 1;

        if (io.r.rlast) {
          // Last beat sent, return to idle
          r_state_next = ReadState::IDLE;
        }
      }
    }
    break;

  default:
    r_state_next = ReadState::IDLE;
    break;
  }
}

// ============================================================================
// Sequential Logic
// ============================================================================
void SimDDR::seq() {
  // Perform memory write if W handshake occurred this cycle
  // This happens BEFORE state update, using current cycle's signals
  if (w_state == WriteState::DATA && io.w.wvalid && io.w.wready) {
    uint32_t current_addr = w_addr + (w_beat_cnt << w_size);
    do_memory_write(current_addr, io.w.wdata, io.w.wstrb);
  }

  // Write channel state update
  w_state = w_state_next;
  w_addr = w_addr_next;
  w_len = w_len_next;
  w_size = w_size_next;
  w_burst = w_burst_next;
  w_beat_cnt = w_beat_cnt_next;
  w_latency_cnt = w_latency_cnt_next;

  // Read channel state update
  r_state = r_state_next;
  r_addr = r_addr_next;
  r_len = r_len_next;
  r_size = r_size_next;
  r_burst = r_burst_next;
  r_beat_cnt = r_beat_cnt_next;
  r_latency_cnt = r_latency_cnt_next;
}

// ============================================================================
// Helper Functions
// ============================================================================

uint32_t SimDDR::calc_next_addr(uint32_t addr, uint8_t size, uint8_t burst) {
  uint32_t bytes_per_beat = 1U << size;
  switch (burst) {
  case AXI_BURST_FIXED:
    return addr; // Address stays the same
  case AXI_BURST_INCR:
    return addr + bytes_per_beat;
  case AXI_BURST_WRAP:
    // Simplified: treat as INCR
    return addr + bytes_per_beat;
  default:
    return addr + bytes_per_beat;
  }
}

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
  const char *w_state_str[] = {"IDLE", "ADDR", "DATA", "RESP"};
  const char *r_state_str[] = {"IDLE", "ADDR", "LATENCY", "DATA"};

  printf("[SimDDR] Write: state=%s addr=0x%08x len=%d beat=%d latency=%d\n",
         w_state_str[static_cast<int>(w_state)], w_addr, w_len, w_beat_cnt,
         w_latency_cnt);
  printf("[SimDDR] Read:  state=%s addr=0x%08x len=%d beat=%d latency=%d\n",
         r_state_str[static_cast<int>(r_state)], r_addr, r_len, r_beat_cnt,
         r_latency_cnt);
}

} // namespace sim_ddr
