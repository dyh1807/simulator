#pragma once
/**
 * @file SimDDR_AXI3_IO.h
 * @brief AXI3 interface signal type definitions for SimDDR (256-bit data)
 *
 * This IO targets a constrained AXI3 memory controller interface:
 * - AXI3 protocol
 * - ID width 22-bit (represented with wire32_t)
 * - Data width 256-bit (8 x 32-bit words)
 * - INCR burst only (no WRAP)
 * - No narrow bursts (AWSIZE/ARSIZE must be 32B)
 */

#include "axi_interconnect_compat.h"
#include <cstdint>

namespace sim_ddr_axi3 {

// ============================================================================
// AXI3 Configuration
// ============================================================================
constexpr uint8_t AXI_ID_WIDTH = 22; // represented with wire32_t
constexpr uint8_t AXI_DATA_BYTES = 32;
constexpr uint8_t AXI_DATA_WORDS = 8;

// ============================================================================
// AXI Burst Types
// ============================================================================
constexpr uint8_t AXI_BURST_FIXED = 0b00;
constexpr uint8_t AXI_BURST_INCR = 0b01;
constexpr uint8_t AXI_BURST_WRAP = 0b10;

// ============================================================================
// AXI Response Types
// ============================================================================
constexpr uint8_t AXI_RESP_OKAY = 0b00;
constexpr uint8_t AXI_RESP_EXOKAY = 0b01;
constexpr uint8_t AXI_RESP_SLVERR = 0b10;
constexpr uint8_t AXI_RESP_DECERR = 0b11;

// ============================================================================
// 256-bit Data Type (8 x 32-bit words)
// ============================================================================
struct Data256_t {
  uint32_t words[AXI_DATA_WORDS];

  void clear() {
    for (int i = 0; i < AXI_DATA_WORDS; i++) {
      words[i] = 0;
    }
  }

  uint32_t &operator[](int idx) { return words[idx]; }
  const uint32_t &operator[](int idx) const { return words[idx]; }
};

// ============================================================================
// AXI3 Write Address Channel (AW)
// Master -> Slave
// ============================================================================
struct AXI3_AW_t {
  wire1_t awvalid;
  wire1_t awready;

  wire32_t awid;   // 22-bit in target controller (packed in wire32_t)
  wire32_t awaddr; // byte address
  wire4_t awlen;   // AXI3: 4-bit length (0=1 beat, 15=16 beats)
  wire3_t awsize;  // log2(bytes per beat) (must be 5 for 32B)
  wire2_t awburst; // FIXED/INCR/WRAP (INCR only)
};

// ============================================================================
// AXI3 Write Data Channel (W)
// Master -> Slave
// ============================================================================
struct AXI3_W_t {
  wire1_t wvalid;
  wire1_t wready;

  wire32_t wid;     // AXI3 WID (matches AWID when no interleaving)
  Data256_t wdata;  // 256-bit write data
  wire32_t wstrb;   // 32-byte strobe (1 bit per byte)
  wire1_t wlast;    // last beat of burst
};

// ============================================================================
// AXI3 Write Response Channel (B)
// Slave -> Master
// ============================================================================
struct AXI3_B_t {
  wire1_t bvalid;
  wire1_t bready;

  wire32_t bid;
  wire2_t bresp;
};

// ============================================================================
// AXI3 Read Address Channel (AR)
// Master -> Slave
// ============================================================================
struct AXI3_AR_t {
  wire1_t arvalid;
  wire1_t arready;

  wire32_t arid;
  wire32_t araddr;
  wire4_t arlen;
  wire3_t arsize;
  wire2_t arburst;
};

// ============================================================================
// AXI3 Read Data Channel (R)
// Slave -> Master
// ============================================================================
struct AXI3_R_t {
  wire1_t rvalid;
  wire1_t rready;

  wire32_t rid;
  Data256_t rdata;
  wire2_t rresp;
  wire1_t rlast;
};

// ============================================================================
// Combined SimDDR AXI3 IO
// ============================================================================
struct SimDDR_AXI3_IO_t {
  AXI3_AW_t aw;
  AXI3_W_t w;
  AXI3_B_t b;
  AXI3_AR_t ar;
  AXI3_R_t r;
};

} // namespace sim_ddr_axi3

