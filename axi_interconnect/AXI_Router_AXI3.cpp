/**
 * @file AXI_Router_AXI3.cpp
 * @brief Simple AXI3 address router (DDR vs MMIO) for 256-bit bus.
 */

#include "AXI_Router_AXI3.h"
#include <mmio_map.h>

namespace axi_interconnect {

void AXI_Router_AXI3::init() {
  r_active = false;
  r_to_mmio = false;
  w_active = false;
  w_to_mmio = false;
}

void AXI_Router_AXI3::comb_outputs(sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
                                   const sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
                                   const sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio) {
  // Defaults for ready (filled in comb_inputs)
  up.ar.arready = false;
  up.aw.awready = false;
  up.w.wready = false;

  // Route R channel from selected target
  if (r_active) {
    const auto &src = r_to_mmio ? mmio.r : ddr.r;
    up.r.rvalid = src.rvalid;
    up.r.rid = src.rid;
    up.r.rdata = src.rdata;
    up.r.rresp = src.rresp;
    up.r.rlast = src.rlast;
  } else {
    up.r.rvalid = false;
    up.r.rid = 0;
    up.r.rdata.clear();
    up.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
    up.r.rlast = false;
  }

  // Route B channel from selected target
  if (w_active) {
    const auto &src = w_to_mmio ? mmio.b : ddr.b;
    up.b.bvalid = src.bvalid;
    up.b.bid = src.bid;
    up.b.bresp = src.bresp;
  } else {
    up.b.bvalid = false;
    up.b.bid = 0;
    up.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;
  }
}

void AXI_Router_AXI3::comb_inputs(sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
                                  sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
                                  sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio) {
  // Clear downstream request valids
  ddr.ar.arvalid = false;
  ddr.aw.awvalid = false;
  ddr.w.wvalid = false;
  ddr.w.wlast = false;

  mmio.ar.arvalid = false;
  mmio.aw.awvalid = false;
  mmio.w.wvalid = false;
  mmio.w.wlast = false;

  // Default ready outputs
  bool ar_ready = false;
  bool aw_ready = false;
  bool w_ready = false;

  // Read address routing
  bool ar_sel_mmio = is_mmio_addr(up.ar.araddr);
  if (!r_active) {
    ar_ready = ar_sel_mmio ? mmio.ar.arready : ddr.ar.arready;
    if (up.ar.arvalid) {
      auto &dst = ar_sel_mmio ? mmio.ar : ddr.ar;
      dst.arvalid = true;
      dst.araddr = up.ar.araddr;
      dst.arid = up.ar.arid;
      dst.arlen = up.ar.arlen;
      dst.arsize = up.ar.arsize;
      dst.arburst = up.ar.arburst;
    }
  }

  // Write address routing
  bool aw_sel_mmio = is_mmio_addr(up.aw.awaddr);
  bool aw_hs_now = up.aw.awvalid && (aw_sel_mmio ? mmio.aw.awready : ddr.aw.awready);
  if (!w_active) {
    aw_ready = aw_sel_mmio ? mmio.aw.awready : ddr.aw.awready;
    if (up.aw.awvalid) {
      auto &dst = aw_sel_mmio ? mmio.aw : ddr.aw;
      dst.awvalid = true;
      dst.awaddr = up.aw.awaddr;
      dst.awid = up.aw.awid;
      dst.awlen = up.aw.awlen;
      dst.awsize = up.aw.awsize;
      dst.awburst = up.aw.awburst;
    }
  }

  // Write data routing (allow AW/W same-cycle)
  bool w_sel_mmio = w_active ? w_to_mmio : (aw_hs_now ? aw_sel_mmio : false);
  if (w_active || aw_hs_now) {
    w_ready = w_sel_mmio ? mmio.w.wready : ddr.w.wready;
    if (up.w.wvalid) {
      auto &dst = w_sel_mmio ? mmio.w : ddr.w;
      dst.wvalid = true;
      dst.wid = up.w.wid;
      dst.wdata = up.w.wdata;
      dst.wstrb = up.w.wstrb;
      dst.wlast = up.w.wlast;
    }
  }

  // Route upstream ready signals
  up.ar.arready = ar_ready;
  up.aw.awready = aw_ready;
  up.w.wready = w_ready;

  // Route R/B ready to selected target
  if (r_active) {
    if (r_to_mmio) {
      mmio.r.rready = up.r.rready;
      ddr.r.rready = false;
    } else {
      ddr.r.rready = up.r.rready;
      mmio.r.rready = false;
    }
  } else {
    ddr.r.rready = false;
    mmio.r.rready = false;
  }

  if (w_active) {
    if (w_to_mmio) {
      mmio.b.bready = up.b.bready;
      ddr.b.bready = false;
    } else {
      ddr.b.bready = up.b.bready;
      mmio.b.bready = false;
    }
  } else {
    ddr.b.bready = false;
    mmio.b.bready = false;
  }
}

void AXI_Router_AXI3::seq(const sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
                          const sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
                          const sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio) {
  // Latch read target on AR handshake
  if (up.ar.arvalid && up.ar.arready) {
    r_active = true;
    r_to_mmio = is_mmio_addr(up.ar.araddr);
  }

  // Complete read on RLAST handshake from selected target
  if (r_active) {
    const auto &src = r_to_mmio ? mmio.r : ddr.r;
    if (src.rvalid && src.rready && src.rlast) {
      r_active = false;
    }
  }

  // Latch write target on AW handshake
  if (up.aw.awvalid && up.aw.awready) {
    w_active = true;
    w_to_mmio = is_mmio_addr(up.aw.awaddr);
  }

  // Complete write on B handshake from selected target
  if (w_active) {
    const auto &src = w_to_mmio ? mmio.b : ddr.b;
    if (src.bvalid && src.bready) {
      w_active = false;
    }
  }
}

} // namespace axi_interconnect
