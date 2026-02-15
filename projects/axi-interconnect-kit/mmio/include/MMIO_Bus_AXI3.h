#pragma once
/**
 * @file MMIO_Bus_AXI3.h
 * @brief AXI3 MMIO bus (single outstanding, FIXED burst) for 256-bit bus.
 */

#include "MMIO_Device.h"
#include "SimDDR_AXI3_IO.h"
#include <array>
#include <cstdint>
#include <vector>

namespace mmio {

class MMIO_Bus_AXI3 {
public:
  void init();
  void add_device(uint32_t base, uint32_t size, MMIO_Device *dev);

  void comb_outputs();
  void comb_inputs();
  void seq();

  // AXI3 IO (router-facing)
  sim_ddr_axi3::SimDDR_AXI3_IO_t io;

private:
  struct Region {
    uint32_t base;
    uint32_t size;
    MMIO_Device *dev;
  };

  struct PendingRead {
    bool active;
    uint32_t id;
    uint8_t offset;
    uint8_t total_size;
    std::array<uint8_t, 32> data;
    uint32_t latency_cnt;
  };

  struct PendingWriteResp {
    bool active;
    uint32_t id;
    uint32_t latency_cnt;
  };

  std::vector<Region> regions;

  bool w_active;
  uint32_t w_addr;
  uint32_t w_id;

  PendingRead r_pending;
  PendingWriteResp w_resp;

  static constexpr uint32_t MMIO_LATENCY = 1;

  MMIO_Device *find_device(uint32_t addr, uint32_t &base) const;
  static uint32_t load_le32(const uint8_t *p);
  static void store_le32(uint8_t *p, uint32_t v);
  static void decode_axi_id(uint32_t axi_id, uint8_t &master_id,
                            uint8_t &orig_id, uint8_t &offset_bytes,
                            uint8_t &total_size);
};

} // namespace mmio
