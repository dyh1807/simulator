/**
 * @file AXI_Interconnect_AXI3.cpp
 * @brief AXI-Interconnect (CPU-side ports) -> constrained AXI3 (256-bit) bridge
 */

#include "AXI_Interconnect_AXI3.h"
#include <mmio_map.h>
#include <cstdio>

namespace axi_interconnect {

uint32_t AXI_Interconnect_AXI3::make_axi_id(uint8_t master_id, uint8_t orig_id,
                                            uint8_t offset_bytes,
                                            uint8_t total_size) {
  // Pack metadata into ID to survive downstream backpressure/latching:
  // [3:0]   orig_id (upstream)
  // [5:4]   master_id
  // [10:6]  offset_bytes (0..31)
  // [15:11] total_size (0..31) meaning bytes=total_size+1
  return (orig_id & 0xF) | ((master_id & 0x3) << 4) |
         ((offset_bytes & 0x1F) << 6) | ((total_size & 0x1F) << 11);
}

void AXI_Interconnect_AXI3::decode_axi_id(uint32_t axi_id, uint8_t &master_id,
                                          uint8_t &orig_id,
                                          uint8_t &offset_bytes,
                                          uint8_t &total_size) {
  orig_id = axi_id & 0xF;
  master_id = (axi_id >> 4) & 0x3;
  offset_bytes = (axi_id >> 6) & 0x1F;
  total_size = (axi_id >> 11) & 0x1F;
}

uint8_t AXI_Interconnect_AXI3::calc_total_beats(uint8_t offset_bytes,
                                                uint8_t total_size) {
  uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
  uint32_t span = static_cast<uint32_t>(offset_bytes) + bytes;
  return static_cast<uint8_t>((span + sim_ddr_axi3::AXI_DATA_BYTES - 1) /
                              sim_ddr_axi3::AXI_DATA_BYTES);
}

uint32_t AXI_Interconnect_AXI3::load_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void AXI_Interconnect_AXI3::store_le32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void AXI_Interconnect_AXI3::init() {
  r_arb_rr_idx = 0;

  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    req_ready_r[i] = false;
    read_ports[i].req.ready = false;
    read_ports[i].resp.valid = false;
    read_ports[i].resp.data.clear();
    read_ports[i].resp.id = 0;
  }

  w_req_ready_r = false;
  write_port.req.ready = false;
  write_port.resp.valid = false;
  write_port.resp.id = 0;
  write_port.resp.resp = 0;

  // Downstream defaults
  axi_io.ar.arvalid = false;
  axi_io.ar.arid = 0;
  axi_io.ar.araddr = 0;
  axi_io.ar.arlen = 0;
  axi_io.ar.arsize = 5;
  axi_io.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

  axi_io.r.rready = true;

  axi_io.aw.awvalid = false;
  axi_io.aw.awid = 0;
  axi_io.aw.awaddr = 0;
  axi_io.aw.awlen = 0;
  axi_io.aw.awsize = 5;
  axi_io.aw.awburst = sim_ddr_axi3::AXI_BURST_INCR;

  axi_io.w.wvalid = false;
  axi_io.w.wid = 0;
  axi_io.w.wdata.clear();
  axi_io.w.wstrb = 0;
  axi_io.w.wlast = false;

  axi_io.b.bready = true;

  ar_latched = {};
  aw_latched = {};

  r_resp_valid = false;
  r_resp_master = 0;
  r_resp_data.clear();
  r_resp_id = 0;

  r_active = false;
  r_axi_id = 0;
  r_total_beats = 0;
  r_beats_done = 0;
  r_beats[0].clear();
  r_beats[1].clear();

  w_resp_valid = false;
  w_resp_id = 0;
  w_resp_resp = 0;

  w_active = false;
  w_axi_id = 0;
  w_total_beats = 0;
  w_beats_sent = 0;
  w_beats_data[0].clear();
  w_beats_data[1].clear();
  w_beats_strb[0] = 0;
  w_beats_strb[1] = 0;
  w_aw_done = false;
  w_w_done = false;
}

void AXI_Interconnect_AXI3::comb_outputs() {
  comb_read_response();
  comb_write_response();

  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = req_ready_r[i];
  }

  if (ar_latched.valid) {
    uint8_t master = 0, orig = 0, off = 0, ts = 0;
    decode_axi_id(ar_latched.id, master, orig, off, ts);
    if (master < NUM_READ_MASTERS) {
      read_ports[master].req.ready = true;
    }
  }

  write_port.req.ready = w_req_ready_r;
}

void AXI_Interconnect_AXI3::comb_inputs() {
  comb_read_arbiter();
  comb_write_request();
}

void AXI_Interconnect_AXI3::comb_read_arbiter() {
  bool req_ready_curr[NUM_READ_MASTERS];
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    req_ready_curr[i] = req_ready_r[i];
    req_ready_r[i] = false;
  }

  // Detect dropped request (ready pulse but no valid).
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (req_ready_curr[i] && !read_ports[i].req.valid && DEBUG) {
      printf("[axi3] ready without valid (drop) master=%d\n", i);
    }
  }

  // If AR is latched, keep driving it until handshake.
  if (ar_latched.valid) {
    axi_io.ar.arvalid = true;
    axi_io.ar.araddr = ar_latched.addr;
    axi_io.ar.arlen = ar_latched.len;
    axi_io.ar.arsize = ar_latched.size;
    axi_io.ar.arburst = ar_latched.burst;
    axi_io.ar.arid = ar_latched.id;

    uint8_t master = 0, orig = 0, off = 0, ts = 0;
    decode_axi_id(ar_latched.id, master, orig, off, ts);
    if (master < NUM_READ_MASTERS) {
      req_ready_r[master] = true;
    }
    return;
  }

  axi_io.ar.arvalid = false;

  // Only one read in flight and one response buffer.
  if (r_active || r_resp_valid) {
    return;
  }

  // If a master saw ready last cycle, complete that handshake first.
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (!req_ready_curr[i] || !read_ports[i].req.valid) {
      continue;
    }

    uint32_t req_addr = read_ports[i].req.addr;
    uint8_t total_size = read_ports[i].req.total_size;
    uint8_t offset = req_addr & 0x1F;
    bool is_mmio = is_mmio_addr(req_addr);
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    uint8_t beats = 0;
    if (is_mmio) {
      if ((offset + bytes) > sim_ddr_axi3::AXI_DATA_BYTES) {
        if (DEBUG) {
          printf("[axi3] mmio read spans beats addr=0x%08x size=%u\n", req_addr,
                 total_size);
        }
        continue;
      }
      beats = 1;
    } else {
      beats = calc_total_beats(offset, total_size);
    }
    if (beats == 0 || beats > 2) {
      if (DEBUG) {
        printf("[axi3] invalid beats=%u addr=0x%08x size=%u\n", beats, req_addr,
               total_size);
      }
      continue;
    }

    uint32_t aligned_addr = req_addr & ~0x1Fu;
    axi_io.ar.arvalid = true;
    axi_io.ar.araddr = aligned_addr;
    axi_io.ar.arlen = static_cast<uint8_t>(beats - 1);
    axi_io.ar.arsize = 5; // 32B beats only
    axi_io.ar.arburst =
        is_mmio ? sim_ddr_axi3::AXI_BURST_FIXED : sim_ddr_axi3::AXI_BURST_INCR;
    axi_io.ar.arid = make_axi_id(i, read_ports[i].req.id, offset, total_size);
    return;
  }

  // Round-robin search for a valid request and raise ready-first pulse.
  for (int k = 0; k < NUM_READ_MASTERS; k++) {
    int idx = (r_arb_rr_idx + k) % NUM_READ_MASTERS;
    if (!read_ports[idx].req.valid) {
      continue;
    }
    if (!req_ready_curr[idx]) {
      req_ready_r[idx] = true;
      break;
    }

    uint32_t req_addr = read_ports[idx].req.addr;
    uint8_t total_size = read_ports[idx].req.total_size;
    uint8_t offset = req_addr & 0x1F;
    bool is_mmio = is_mmio_addr(req_addr);
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    uint8_t beats = 0;
    if (is_mmio) {
      if ((offset + bytes) > sim_ddr_axi3::AXI_DATA_BYTES) {
        if (DEBUG) {
          printf("[axi3] mmio read spans beats addr=0x%08x size=%u\n", req_addr,
                 total_size);
        }
        continue;
      }
      beats = 1;
    } else {
      beats = calc_total_beats(offset, total_size);
    }
    if (beats == 0 || beats > 2) {
      continue;
    }
    uint32_t aligned_addr = req_addr & ~0x1Fu;
    axi_io.ar.arvalid = true;
    axi_io.ar.araddr = aligned_addr;
    axi_io.ar.arlen = static_cast<uint8_t>(beats - 1);
    axi_io.ar.arsize = 5;
    axi_io.ar.arburst =
        is_mmio ? sim_ddr_axi3::AXI_BURST_FIXED : sim_ddr_axi3::AXI_BURST_INCR;
    axi_io.ar.arid =
        make_axi_id(idx, read_ports[idx].req.id, offset, total_size);
    break;
  }
}

void AXI_Interconnect_AXI3::comb_read_response() {
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].resp.valid = false;
  }

  axi_io.r.rready = true;

  if (r_resp_valid && r_resp_master < NUM_READ_MASTERS) {
    read_ports[r_resp_master].resp.valid = true;
    read_ports[r_resp_master].resp.data = r_resp_data;
    read_ports[r_resp_master].resp.id = r_resp_id;
  }
}

void AXI_Interconnect_AXI3::comb_write_request() {
  bool w_req_ready_curr = w_req_ready_r;
  w_req_ready_r = false;

  if (w_req_ready_curr && !write_port.req.valid && DEBUG) {
    printf("[axi3] write ready without valid (drop)\n");
  }

  // Drive AW from latch until handshake.
  if (aw_latched.valid) {
    axi_io.aw.awvalid = true;
    axi_io.aw.awaddr = aw_latched.addr;
    axi_io.aw.awlen = aw_latched.len;
    axi_io.aw.awsize = aw_latched.size;
    axi_io.aw.awburst = aw_latched.burst;
    axi_io.aw.awid = aw_latched.id;
  } else {
    axi_io.aw.awvalid = false;

    // Ready-first for upstream write request; block when busy or resp pending.
    if (!w_active && !w_resp_valid && write_port.req.valid && !w_req_ready_curr) {
      w_req_ready_r = true;
    }
  }

  axi_io.w.wvalid = false;
  axi_io.w.wlast = false;

  bool aw_handshake_now = aw_latched.valid && axi_io.aw.awready;
  if (w_active && !w_w_done && (w_aw_done || aw_handshake_now)) {
    axi_io.w.wvalid = true;
    axi_io.w.wid = w_axi_id;
    axi_io.w.wdata = w_beats_data[w_beats_sent];
    axi_io.w.wstrb = w_beats_strb[w_beats_sent];
    axi_io.w.wlast = (w_beats_sent == (w_total_beats - 1));
  }
}

void AXI_Interconnect_AXI3::comb_write_response() {
  write_port.resp.valid = w_resp_valid;
  write_port.resp.id = w_resp_id;
  write_port.resp.resp = w_resp_resp;

  axi_io.b.bready = !w_resp_valid;
}

void AXI_Interconnect_AXI3::seq() {
  // Capture previous-cycle visibility for robust upstream handshakes.
  // (Prevents clearing a response in the same cycle it is produced.)
  bool r_resp_valid_curr = r_resp_valid;
  bool w_resp_valid_curr = w_resp_valid;

  // Upstream read response handshake
  if (r_resp_valid_curr && r_resp_master < NUM_READ_MASTERS &&
      read_ports[r_resp_master].resp.valid &&
      read_ports[r_resp_master].resp.ready) {
    r_resp_valid = false;
  }

  // Upstream write response handshake
  if (w_resp_valid_curr && write_port.resp.valid && write_port.resp.ready) {
    w_resp_valid = false;
    w_active = false;
    w_aw_done = false;
    w_w_done = false;
    w_total_beats = 0;
    w_beats_sent = 0;
  }

  // ========== AR latch ==========
  if (axi_io.ar.arvalid && !ar_latched.valid && !axi_io.ar.arready) {
    ar_latched.valid = true;
    ar_latched.addr = axi_io.ar.araddr;
    ar_latched.len = axi_io.ar.arlen;
    ar_latched.size = axi_io.ar.arsize;
    ar_latched.burst = axi_io.ar.arburst;
    ar_latched.id = axi_io.ar.arid;
  }

  if (axi_io.ar.arvalid && axi_io.ar.arready) {
    uint32_t id = axi_io.ar.arid;
    uint8_t len = axi_io.ar.arlen;
    if (ar_latched.valid) {
      id = ar_latched.id;
      len = ar_latched.len;
      ar_latched.valid = false;
    }

    r_active = true;
    r_axi_id = id;
    r_total_beats = static_cast<uint8_t>(len + 1);
    r_beats_done = 0;
    r_beats[0].clear();
    r_beats[1].clear();

    uint8_t master = 0, orig = 0, off = 0, ts = 0;
    decode_axi_id(id, master, orig, off, ts);
    r_arb_rr_idx = (master + 1) % NUM_READ_MASTERS;
  }

  // ========== R channel ==========
  if (axi_io.r.rvalid && axi_io.r.rready && r_active) {
    // Accept sequential beats (no interleaving).
    if (r_beats_done < 2) {
      r_beats[r_beats_done] = axi_io.r.rdata;
    }
    r_beats_done++;

    bool done = axi_io.r.rlast || (r_beats_done >= r_total_beats);
    if (done) {
      uint8_t master = 0, orig = 0, off = 0, ts = 0;
      decode_axi_id(axi_io.r.rid, master, orig, off, ts);
      uint32_t bytes = static_cast<uint32_t>(ts) + 1;

      uint8_t buf[64] = {0};
      for (int b = 0; b < r_total_beats && b < 2; b++) {
        for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
          store_le32(buf + (b * 32) + (w * 4), r_beats[b][w]);
        }
      }

      uint8_t out_bytes[32] = {0};
      for (uint32_t i = 0; i < bytes && i < 32; i++) {
        out_bytes[i] = buf[static_cast<uint32_t>(off) + i];
      }

      WideData256_t out;
      out.clear();
      for (int w = 0; w < CACHELINE_WORDS; w++) {
        out[w] = load_le32(out_bytes + (w * 4));
      }

      r_resp_valid = true;
      r_resp_master = master;
      r_resp_id = orig;
      r_resp_data = out;

      r_active = false;
      r_total_beats = 0;
      r_beats_done = 0;
    }
  }

  // ========== Accept new write request ==========
  if (!w_active && write_port.req.valid && write_port.req.ready) {
    uint32_t req_addr = write_port.req.addr;
    uint8_t total_size = write_port.req.total_size;
    uint8_t offset = req_addr & 0x1F;
    bool is_mmio = is_mmio_addr(req_addr);
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    uint8_t beats = 0;
    if (is_mmio) {
      if ((offset + bytes) > sim_ddr_axi3::AXI_DATA_BYTES) {
        if (DEBUG) {
          printf("[axi3] mmio write spans beats addr=0x%08x size=%u\n", req_addr,
                 total_size);
        }
      } else {
        beats = 1;
      }
    } else {
      beats = calc_total_beats(offset, total_size);
    }
    if (beats == 0 || beats > 2) {
      if (DEBUG) {
        printf("[axi3] invalid write beats=%u addr=0x%08x size=%u\n", beats,
               req_addr, total_size);
      }
    } else {
      uint32_t aligned_addr = req_addr & ~0x1Fu;
      uint32_t axi_id =
          make_axi_id(MASTER_DCACHE_W, write_port.req.id, offset, total_size);

      uint8_t in_bytes[32] = {0};
      for (int w = 0; w < CACHELINE_WORDS; w++) {
        store_le32(in_bytes + (w * 4), write_port.req.wdata[w]);
      }

      uint8_t beat_bytes[2][32] = {{0}};
      uint32_t beat_strb[2] = {0, 0};
      uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
      for (uint32_t i = 0; i < bytes && i < 32; i++) {
        if (((write_port.req.wstrb >> i) & 1u) == 0) {
          continue;
        }
        uint32_t dst = static_cast<uint32_t>(offset) + i;
        uint32_t beat = dst / 32;
        uint32_t pos = dst % 32;
        if (beat >= beats) {
          continue;
        }
        beat_bytes[beat][pos] = in_bytes[i];
        beat_strb[beat] |= (1u << pos);
      }

      for (int b = 0; b < beats; b++) {
        w_beats_data[b].clear();
        for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
          w_beats_data[b][w] = load_le32(beat_bytes[b] + (w * 4));
        }
        w_beats_strb[b] = beat_strb[b];
      }

      w_active = true;
      w_axi_id = axi_id;
      w_total_beats = beats;
      w_beats_sent = 0;
      w_aw_done = false;
      w_w_done = false;

      aw_latched.valid = true;
      aw_latched.addr = aligned_addr;
      aw_latched.len = static_cast<uint8_t>(beats - 1);
      aw_latched.size = 5;
      aw_latched.burst =
          is_mmio ? sim_ddr_axi3::AXI_BURST_FIXED : sim_ddr_axi3::AXI_BURST_INCR;
      aw_latched.id = axi_id;
    }
  }

  // AW handshake
  if (axi_io.aw.awvalid && axi_io.aw.awready && aw_latched.valid) {
    aw_latched.valid = false;
    w_aw_done = true;
  }

  // W handshake
  if (axi_io.w.wvalid && axi_io.w.wready && w_active) {
    w_beats_sent++;
    if (axi_io.w.wlast) {
      w_w_done = true;
    }
  }

  // B handshake (buffer response)
  if (axi_io.b.bvalid && axi_io.b.bready) {
    uint8_t master = 0, orig = 0, off = 0, ts = 0;
    decode_axi_id(axi_io.b.bid, master, orig, off, ts);
    w_resp_valid = true;
    w_resp_id = orig;
    w_resp_resp = axi_io.b.bresp;
  }

}

void AXI_Interconnect_AXI3::debug_print() {
  printf("  interconnect_axi3: r_active=%d r_resp=%d ar_latched=%d "
         "w_active=%d w_resp=%d aw_latched=%d\n",
         r_active, r_resp_valid, ar_latched.valid, w_active, w_resp_valid,
         aw_latched.valid);
}

} // namespace axi_interconnect
