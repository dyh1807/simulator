/**
 * @file AXI_Interleaving.cpp
 * @brief AXI-Interleaving Layer Implementation
 */

#include "AXI_Interleaving.h"
#include <algorithm>
#include <cstdio>

namespace axi_interleaving {

// ============================================================================
// Initialization
// ============================================================================
void AXI_Interleaving::init() {
  r_arb_rr_idx = 0;
  r_arb_active = false;
  r_current_master = -1;
  r_pending.clear();

  w_active = false;
  w_current = {};
  while (!w_resp_pending.empty())
    w_resp_pending.pop();

  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = false;
    read_ports[i].resp.valid = false;
    read_ports[i].resp.data.clear();
    read_ports[i].resp.id = 0;
  }
  write_port.req.ready = false;
  write_port.resp.valid = false;
  write_port.resp.id = 0;
  write_port.resp.resp = 0;

  axi_io.ar.arvalid = false;
  axi_io.ar.arid = 0;
  axi_io.ar.araddr = 0;
  axi_io.ar.arlen = 0;
  axi_io.ar.arsize = 2;
  axi_io.ar.arburst = sim_ddr::AXI_BURST_INCR;
  axi_io.r.rready = true;

  axi_io.aw.awvalid = false;
  axi_io.aw.awid = 0;
  axi_io.aw.awaddr = 0;
  axi_io.aw.awlen = 0;
  axi_io.aw.awsize = 2;
  axi_io.aw.awburst = sim_ddr::AXI_BURST_INCR;
  axi_io.w.wvalid = false;
  axi_io.w.wdata = 0;
  axi_io.w.wstrb = 0xF;
  axi_io.w.wlast = false;
  axi_io.b.bready = true;
}

// ============================================================================
// Combinational Logic
// ============================================================================
void AXI_Interleaving::comb() {
  comb_read_arbiter();
  comb_read_response();
  comb_write_request();
  comb_write_response();
}

void AXI_Interleaving::comb_read_arbiter() {
  axi_io.ar.arvalid = false;
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = false;
  }

  if (r_arb_active)
    return;

  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    int idx = (r_arb_rr_idx + i) % NUM_READ_MASTERS;
    if (read_ports[idx].req.valid) {
      r_current_master = idx;

      axi_io.ar.arvalid = true;
      axi_io.ar.araddr = read_ports[idx].req.addr;
      axi_io.ar.arlen = calc_burst_len(read_ports[idx].req.total_size);
      axi_io.ar.arsize = 2;
      axi_io.ar.arburst = sim_ddr::AXI_BURST_INCR;
      axi_io.ar.arid = (idx << 2) | (read_ports[idx].req.id & 0x3);

      read_ports[idx].req.ready = true;
      break;
    }
  }
}

void AXI_Interleaving::comb_read_response() {
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].resp.valid = false;
  }
  axi_io.r.rready = true;

  for (auto &txn : r_pending) {
    if (txn.beats_done == txn.total_beats) {
      uint8_t master = txn.master_id;
      read_ports[master].resp.valid = true;
      read_ports[master].resp.data = txn.data;
      read_ports[master].resp.id = txn.orig_id;
      break;
    }
  }
}

void AXI_Interleaving::comb_write_request() {
  write_port.req.ready = false;
  axi_io.aw.awvalid = false;
  axi_io.w.wvalid = false;

  if (!w_active && write_port.req.valid) {
    write_port.req.ready = true;
  }

  if (w_active && !w_current.aw_done) {
    axi_io.aw.awvalid = true;
    axi_io.aw.awaddr = w_current.addr;
    axi_io.aw.awlen = w_current.total_beats - 1;
    axi_io.aw.awsize = 2;
    axi_io.aw.awid = (MASTER_DCACHE_W << 2) | (w_current.orig_id & 0x3);
  }

  if (w_active && w_current.aw_done && !w_current.w_done) {
    axi_io.w.wvalid = true;
    axi_io.w.wdata = w_current.wdata[w_current.beats_sent];
    axi_io.w.wstrb = (w_current.wstrb >> (w_current.beats_sent * 4)) & 0xF;
    axi_io.w.wlast = (w_current.beats_sent == w_current.total_beats - 1);
  }
}

void AXI_Interleaving::comb_write_response() {
  write_port.resp.valid = false;
  axi_io.b.bready = true;

  if (!w_resp_pending.empty() && axi_io.b.bvalid) {
    write_port.resp.valid = true;
    write_port.resp.id = axi_io.b.bid & 0x3;
    write_port.resp.resp = axi_io.b.bresp;
  }
}

// ============================================================================
// Sequential Logic
// ============================================================================
void AXI_Interleaving::seq() {
  // AR handshake
  if (axi_io.ar.arvalid && axi_io.ar.arready) {
    ReadPendingTxn txn;
    txn.master_id = r_current_master;
    txn.orig_id = read_ports[r_current_master].req.id;
    txn.total_beats = axi_io.ar.arlen + 1;
    txn.beats_done = 0;
    txn.data.clear();
    r_pending.push_back(txn);
    r_arb_rr_idx = (r_current_master + 1) % NUM_READ_MASTERS;
    r_arb_active = false;
  }

  // R handshake
  if (axi_io.r.rvalid && axi_io.r.rready) {
    uint8_t master = (axi_io.r.rid >> 2) & 0x3;
    for (auto &txn : r_pending) {
      if (txn.master_id == master && txn.beats_done < txn.total_beats) {
        txn.data[txn.beats_done] = axi_io.r.rdata;
        txn.beats_done++;
        break;
      }
    }
  }

  // Response handshake
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (read_ports[i].resp.valid && read_ports[i].resp.ready) {
      r_pending.erase(std::remove_if(r_pending.begin(), r_pending.end(),
                                     [i](const ReadPendingTxn &t) {
                                       return t.master_id == i &&
                                              t.beats_done == t.total_beats;
                                     }),
                      r_pending.end());
    }
  }

  // Write: accept request
  if (write_port.req.valid && write_port.req.ready) {
    w_active = true;
    w_current.master_id = MASTER_DCACHE_W;
    w_current.orig_id = write_port.req.id;
    w_current.addr = write_port.req.addr;
    w_current.wdata = write_port.req.wdata;
    w_current.wstrb = write_port.req.wstrb;
    w_current.total_beats = calc_burst_len(write_port.req.total_size) + 1;
    w_current.beats_sent = 0;
    w_current.aw_done = false;
    w_current.w_done = false;
  }

  // AW handshake
  if (axi_io.aw.awvalid && axi_io.aw.awready) {
    w_current.aw_done = true;
  }

  // W handshake
  if (axi_io.w.wvalid && axi_io.w.wready) {
    w_current.beats_sent++;
    if (axi_io.w.wlast) {
      w_current.w_done = true;
      w_resp_pending.push(w_current.orig_id);
      w_active = false;
    }
  }

  // B handshake
  if (axi_io.b.bvalid && axi_io.b.bready && !w_resp_pending.empty()) {
    w_resp_pending.pop();
  }
}

// ============================================================================
// Helpers
// ============================================================================
uint8_t AXI_Interleaving::calc_burst_len(uint8_t total_size) {
  uint8_t bytes = total_size + 1;
  uint8_t beats = (bytes + 3) / 4;
  return beats > 0 ? beats - 1 : 0;
}

} // namespace axi_interleaving
