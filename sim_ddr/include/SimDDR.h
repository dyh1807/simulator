#pragma once
/**
 * @file SimDDR.h
 * @brief SimDDR - DDR Memory Simulator with AXI4 Interface
 *
 * This module simulates DDR memory with standard AXI4 interface.
 * It follows the simulator's comb/seq pattern for cycle-accurate behavior.
 *
 * Features:
 * - 5 AXI4 channels (AW, W, B, AR, R)
 * - Configurable memory latency
 * - INCR burst mode support
 * - Uses external p_memory for storage (shared with main simulator)
 */

#include "SimDDR_IO.h"
#include <config.h>

namespace sim_ddr {

// ============================================================================
// SimDDR Configuration
// ============================================================================
constexpr uint32_t SIM_DDR_LATENCY = 2;     // Memory latency in cycles
constexpr uint32_t SIM_DDR_MAX_BURST = 256; // Max burst length (AXI4 limit)

// ============================================================================
// State Machine States
// ============================================================================

// Write Channel States
enum class WriteState {
  IDLE, // Waiting for AW valid
  ADDR, // Address latched, waiting for W data
  DATA, // Receiving W data beats
  RESP, // Waiting latency, then sending B response
};

// Read Channel States
enum class ReadState {
  IDLE,    // Waiting for AR valid
  ADDR,    // Address latched, waiting for latency
  LATENCY, // Waiting for memory latency
  DATA,    // Sending R data beats
};

// ============================================================================
// SimDDR Class
// ============================================================================
class SimDDR {
public:
  // ========== Simulator Interface ==========
  void init();
  void comb();
  void seq();

  // ========== IO Ports ==========
  SimDDR_IO_t io;

  // ========== Debug ==========
  void print_state();

private:
  // ========== Write Channel State ==========
  WriteState w_state;
  WriteState w_state_next;

  reg32_t w_addr;       // Latched write address
  reg8_t w_len;         // Latched burst length - 1
  reg3_t w_size;        // Latched burst size
  reg2_t w_burst;       // Latched burst type
  reg8_t w_beat_cnt;    // Current beat counter
  reg8_t w_latency_cnt; // Latency counter

  // Write channel next values
  reg32_t w_addr_next;
  reg8_t w_len_next;
  reg3_t w_size_next;
  reg2_t w_burst_next;
  reg8_t w_beat_cnt_next;
  reg8_t w_latency_cnt_next;

  // ========== Read Channel State ==========
  ReadState r_state;
  ReadState r_state_next;

  reg32_t r_addr;       // Latched read address
  reg8_t r_len;         // Latched burst length - 1
  reg3_t r_size;        // Latched burst size
  reg2_t r_burst;       // Latched burst type
  reg8_t r_beat_cnt;    // Current beat counter
  reg8_t r_latency_cnt; // Latency counter

  // Read channel next values
  reg32_t r_addr_next;
  reg8_t r_len_next;
  reg3_t r_size_next;
  reg2_t r_burst_next;
  reg8_t r_beat_cnt_next;
  reg8_t r_latency_cnt_next;

  // ========== Combinational Logic Functions ==========
  void comb_write_channel();
  void comb_read_channel();

  // ========== Helper Functions ==========
  uint32_t calc_next_addr(uint32_t addr, uint8_t size, uint8_t burst);
  void do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb);
  uint32_t do_memory_read(uint32_t addr);
};

} // namespace sim_ddr
