#pragma once
/**
 * @file AXI_Interleaving.h
 * @brief AXI-Interleaving Layer
 *
 * Converts simplified master interfaces (single-beat, wide data) to
 * AXI4 protocol (multi-beat bursts) for connection to SimDDR.
 */

#include "AXI_Interleaving_IO.h"
#include "SimDDR_IO.h"
#include <config.h>
#include <queue>
#include <vector>

namespace axi_interleaving {

// ============================================================================
// Pending Transaction Tracking
// ============================================================================

struct ReadPendingTxn {
  uint8_t master_id;
  uint8_t orig_id;
  uint8_t total_beats;
  uint8_t beats_done;
  WideData256_t data;
};

struct WritePendingTxn {
  uint8_t master_id;
  uint8_t orig_id;
  uint32_t addr;
  WideData256_t wdata;
  uint32_t wstrb;
  uint8_t total_beats;
  uint8_t beats_sent;
  bool aw_done;
  bool w_done;
};

// ============================================================================
// AXI_Interleaving Class
// ============================================================================

class AXI_Interleaving {
public:
  void init();
  void comb();
  void seq();

  // Upstream IO (Masters)
  ReadMasterPort_t read_ports[NUM_READ_MASTERS];
  WriteMasterPort_t write_port;

  // Downstream IO (to SimDDR)
  sim_ddr::SimDDR_IO_t axi_io;

private:
  uint8_t r_arb_rr_idx;
  bool r_arb_active;
  int r_current_master;

  std::vector<ReadPendingTxn> r_pending;

  bool w_active;
  WritePendingTxn w_current;
  std::queue<uint8_t> w_resp_pending;

  void comb_read_arbiter();
  void comb_read_response();
  void comb_write_request();
  void comb_write_response();

  uint8_t calc_burst_len(uint8_t total_size);
};

} // namespace axi_interleaving
