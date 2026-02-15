#pragma once
/**
 * @file SimDDR_AXI3.h
 * @brief SimDDR - DDR Memory Simulator with constrained AXI3 (256-bit) IO
 *
 * Intended to model an AXI3 memory controller with constraints:
 * - 256-bit data beats only (no narrow bursts)
 * - INCR bursts only (no WRAP)
 * - No read-data interleaving (single outstanding read)
 * - No write interleaving (single outstanding write)
 *
 * Uses global p_memory (uint32_t*) as backing storage.
 */

#include "SimDDR_AXI3_IO.h"
#include "axi_interconnect_compat.h"
#include <cstdint>
#include <queue>

namespace sim_ddr_axi3 {

// ============================================================================
// Configuration
// ============================================================================
constexpr uint32_t SIM_DDR_AXI3_LATENCY = ICACHE_MISS_LATENCY;
constexpr uint8_t SIM_DDR_AXI3_MAX_BURST = 16; // AXI3 supports up to 16 beats

// ============================================================================
// Transactions
// ============================================================================
struct WriteTransaction {
  uint32_t addr;
  uint32_t id;
  uint8_t len;   // beats-1
  uint8_t size;  // log2(bytes)
  uint8_t burst; // INCR only
  uint8_t beat_cnt;
  bool data_done;
};

struct WriteRespPending {
  uint32_t id;
  uint32_t latency_cnt;
};

struct ReadTransaction {
  uint32_t addr;
  uint32_t id;
  uint8_t len;
  uint8_t size;
  uint8_t burst;
  uint8_t beat_cnt;
  uint32_t latency_cnt;
  bool in_data_phase;
  bool complete;
};

class SimDDR_AXI3 {
public:
  void init();

  // Two-phase combinational logic (for symmetry with MemorySubsystem)
  void comb_outputs(); // Phase 1: ready/valid/data outputs
  void comb_inputs();  // Phase 2: (no-op; inputs sampled in comb_outputs)
  void comb() {
    comb_outputs();
    comb_inputs();
  }

  void seq();

  // IO ports
  SimDDR_AXI3_IO_t io;

  // Debug
  void print_state();

private:
  // Write path
  bool w_active;
  WriteTransaction w_current;
  std::queue<WriteRespPending> w_resp_queue;

  // Read path (single outstanding, no interleaving)
  bool r_active;
  ReadTransaction r_current;

  void comb_write_channel();
  void comb_read_channel();

  void do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb);
  uint32_t do_memory_read(uint32_t addr);
};

} // namespace sim_ddr_axi3

