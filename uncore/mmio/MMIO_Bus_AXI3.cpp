/**
 * @file MMIO_Bus_AXI3.cpp
 * @brief AXI3 MMIO bus (single outstanding, FIXED burst) for 256-bit bus.
 */

#include "MMIO_Bus_AXI3.h"
#include <cstring>

namespace mmio {

void MMIO_Bus_AXI3::init() {
  regions.clear();

  w_active = false;
  w_addr = 0;
  w_id = 0;

  r_pending = {};
  r_pending.active = false;
  r_pending.data.fill(0);

  w_resp = {};
  w_resp.active = false;

  io.aw.awready = false;
  io.aw.awvalid = false;
  io.aw.awid = 0;
  io.aw.awaddr = 0;
  io.aw.awlen = 0;
  io.aw.awsize = 5;
  io.aw.awburst = sim_ddr_axi3::AXI_BURST_FIXED;

  io.w.wready = false;
  io.w.wvalid = false;
  io.w.wid = 0;
  io.w.wdata.clear();
  io.w.wstrb = 0;
  io.w.wlast = false;

  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

  io.ar.arready = false;
  io.ar.arvalid = false;
  io.ar.arid = 0;
  io.ar.araddr = 0;
  io.ar.arlen = 0;
  io.ar.arsize = 5;
  io.ar.arburst = sim_ddr_axi3::AXI_BURST_FIXED;

  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata.clear();
  io.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
  io.r.rlast = false;
}

void MMIO_Bus_AXI3::add_device(uint32_t base, uint32_t size, MMIO_Device *dev) {
  regions.push_back({base, size, dev});
}

MMIO_Device *MMIO_Bus_AXI3::find_device(uint32_t addr, uint32_t &base) const {
  for (const auto &r : regions) {
    if (addr >= r.base && addr < (r.base + r.size)) {
      base = r.base;
      return r.dev;
    }
  }
  base = 0;
  return nullptr;
}

uint32_t MMIO_Bus_AXI3::load_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void MMIO_Bus_AXI3::store_le32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void MMIO_Bus_AXI3::decode_axi_id(uint32_t axi_id, uint8_t &master_id,
                                  uint8_t &orig_id, uint8_t &offset_bytes,
                                  uint8_t &total_size) {
  orig_id = axi_id & 0xF;
  master_id = (axi_id >> 4) & 0x3;
  offset_bytes = (axi_id >> 6) & 0x1F;
  total_size = (axi_id >> 11) & 0x1F;
}

void MMIO_Bus_AXI3::comb_outputs() {
  // Defaults
  io.ar.arready = false;
  io.aw.awready = false;
  io.w.wready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata.clear();
  io.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
  io.r.rlast = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

  // Read channel ready: only one outstanding
  if (!r_pending.active) {
    io.ar.arready = true;
  }

  // Write channel ready: only one outstanding (and no pending resp)
  if (!w_active && !w_resp.active) {
    io.aw.awready = true;
  }
  if (w_active) {
    io.w.wready = true;
  }

  // Read response after latency
  if (r_pending.active && r_pending.latency_cnt >= MMIO_LATENCY) {
    io.r.rvalid = true;
    io.r.rid = r_pending.id;
    io.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
    io.r.rlast = true;

    uint8_t out_bytes[32] = {0};
    uint32_t bytes = static_cast<uint32_t>(r_pending.total_size) + 1;
    for (uint32_t i = 0; i < bytes && i < 32; i++) {
      out_bytes[static_cast<uint32_t>(r_pending.offset) + i] =
          r_pending.data[i];
    }
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      io.r.rdata[w] = load_le32(out_bytes + (w * 4));
    }
  }

  // Write response after latency
  if (w_resp.active && w_resp.latency_cnt >= MMIO_LATENCY) {
    io.b.bvalid = true;
    io.b.bid = w_resp.id;
    io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;
  }
}

void MMIO_Bus_AXI3::comb_inputs() {
  // no-op
}

void MMIO_Bus_AXI3::seq() {
  // Tick devices
  for (auto &r : regions) {
    if (r.dev) {
      r.dev->tick();
    }
  }

  // Accept AR
  if (io.ar.arvalid && io.ar.arready) {
    uint8_t master = 0, orig = 0, offset = 0, ts = 0;
    decode_axi_id(io.ar.arid, master, orig, offset, ts);
    uint32_t bytes = static_cast<uint32_t>(ts) + 1;
    uint32_t base = 0;
    uint32_t addr = io.ar.araddr + offset;
    MMIO_Device *dev = find_device(addr, base);

    r_pending.active = true;
    r_pending.id = io.ar.arid;
    r_pending.offset = offset;
    r_pending.total_size = ts;
    r_pending.latency_cnt = 0;
    r_pending.data.fill(0);

    if (dev) {
      dev->read(addr, r_pending.data.data(), bytes);
    }
  }

  // Accept AW
  if (io.aw.awvalid && io.aw.awready) {
    w_active = true;
    w_addr = io.aw.awaddr;
    w_id = io.aw.awid;
  }

  // Accept W
  if (io.w.wvalid && io.w.wready && w_active) {
    uint8_t master = 0, orig = 0, offset = 0, ts = 0;
    decode_axi_id(w_id, master, orig, offset, ts);
    uint32_t bytes = static_cast<uint32_t>(ts) + 1;
    uint32_t base = 0;
    uint32_t addr = w_addr + offset;
    MMIO_Device *dev = find_device(addr, base);

    uint8_t in_bytes[32] = {0};
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      store_le32(in_bytes + (w * 4), io.w.wdata[w]);
    }
    uint32_t local_wstrb = 0;
    for (uint32_t i = 0; i < bytes && i < 32; i++) {
      if ((io.w.wstrb >> (offset + i)) & 1u) {
        local_wstrb |= (1u << i);
      }
    }

    if (dev) {
      dev->write(addr, in_bytes + offset, bytes, local_wstrb);
    }

    if (io.w.wlast) {
      w_active = false;
      w_resp.active = true;
      w_resp.id = w_id;
      w_resp.latency_cnt = 0;
    }
  }

  // Consume read response
  if (io.r.rvalid && io.r.rready && r_pending.active) {
    r_pending.active = false;
  }

  // Consume write response
  if (io.b.bvalid && io.b.bready && w_resp.active) {
    w_resp.active = false;
  }

  if (r_pending.active && r_pending.latency_cnt < MMIO_LATENCY) {
    r_pending.latency_cnt++;
  }
  if (w_resp.active && w_resp.latency_cnt < MMIO_LATENCY) {
    w_resp.latency_cnt++;
  }
}

} // namespace mmio
