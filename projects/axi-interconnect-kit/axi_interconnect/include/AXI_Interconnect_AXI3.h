#pragma once
/**
 * @file AXI_Interconnect_AXI3.h
 * @brief AXI-Interconnect (CPU-side ports) -> constrained AXI3 (256-bit) bridge
 *
 * Keeps the same upstream (CPU-side) simplified ports as AXI_Interconnect:
 * - 4 read masters
 * - 2 write masters
 *
 * Downstream targets a constrained AXI3 controller:
 * - 256-bit data beats only (SIZE=5)
 * - INCR burst only
 * - No read data interleaving (bridge issues one read at a time)
 * - No write interleaving (bridge issues one write at a time)
 */

#include "AXI_Interconnect_IO.h"
#include "SimDDR_AXI3_IO.h"
#include "axi_interconnect_compat.h"
#include <cstdint>

namespace axi_interconnect {

class AXI_Interconnect_AXI3 {
public:
  AXI_Interconnect_AXI3() : write_port(write_ports[MASTER_DCACHE_W]) {}

  void init();

  // Two-phase combinational logic
  void comb_outputs(); // Phase 1: resp.valid/data + req.ready (registered)
  void comb_inputs();  // Phase 2: process req, drive AXI3 AR/AW/W
  void comb() {
    comb_outputs();
    comb_inputs();
  }

  void seq();
  void debug_print();

  // Upstream ports (same as existing interconnect)
  ReadMasterPort_t read_ports[NUM_READ_MASTERS];
  WriteMasterPort_t write_ports[NUM_WRITE_MASTERS];
  // Backward-compatible alias to the primary write master.
  WriteMasterPort_t &write_port;

  // Downstream AXI3 IO
  sim_ddr_axi3::SimDDR_AXI3_IO_t axi_io;

private:
  // Registered req.ready pulses
  bool req_ready_r[NUM_READ_MASTERS];
  bool w_req_ready_r[NUM_WRITE_MASTERS];
  uint8_t w_arb_rr_idx;
  int w_current_master;

  // Round-robin for reads
  uint8_t r_arb_rr_idx;

  // Read response register (one at a time)
  bool r_resp_valid;
  uint8_t r_resp_master;
  WideData256_t r_resp_data;
  uint8_t r_resp_id;

  // Active read transaction tracking (no interleaving)
  bool r_active;
  uint32_t r_axi_id;
  uint8_t r_total_beats;
  uint8_t r_beats_done;
  sim_ddr_axi3::Data256_t r_beats[2]; // enough for at most 2 beats

  // AR latch for AXI compliance
  struct ARLatch_t {
    bool valid;
    uint32_t addr;
    uint8_t len;
    uint8_t size;
    uint8_t burst;
    uint32_t id;
  } ar_latched;

  // Write response registers
  bool w_resp_valid[NUM_WRITE_MASTERS];
  uint8_t w_resp_id[NUM_WRITE_MASTERS];
  uint8_t w_resp_resp[NUM_WRITE_MASTERS];

  // Active write transaction tracking (no interleaving)
  bool w_active;
  uint32_t w_axi_id;
  uint8_t w_total_beats;
  uint8_t w_beats_sent;
  sim_ddr_axi3::Data256_t w_beats_data[2];
  uint32_t w_beats_strb[2];
  bool w_aw_done;
  bool w_w_done;

  // AW latch for AXI compliance
  struct AWLatch_t {
    bool valid;
    uint32_t addr;
    uint8_t len;
    uint8_t size;
    uint8_t burst;
    uint32_t id;
  } aw_latched;

  void comb_read_arbiter();
  void comb_read_response();
  void comb_write_request();
  void comb_write_response();

  // Helpers
  static uint32_t make_axi_id(uint8_t master_id, uint8_t orig_id,
                             uint8_t offset_bytes, uint8_t total_size);
  static void decode_axi_id(uint32_t axi_id, uint8_t &master_id,
                            uint8_t &orig_id, uint8_t &offset_bytes,
                            uint8_t &total_size);
  static uint8_t calc_total_beats(uint8_t offset_bytes, uint8_t total_size);

  static uint32_t load_le32(const uint8_t *p);
  static void store_le32(uint8_t *p, uint32_t v);
};

} // namespace axi_interconnect
