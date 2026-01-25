/**
 * @file axi_interconnect_axi3_test.cpp
 * @brief Test harness for AXI_Interconnect_AXI3 bridge
 *
 * Verifies:
 * - Ready-first req handshake + AR latching under arready backpressure
 * - Correct ID packing/decoding and response routing
 * - Correct byte-lane alignment for unaligned reads (offset within 32B beat)
 * - Correct beat splitting/merging for span > 32B (max 2 beats)
 * - Write split across beats + resp backpressure robustness
 * - Randomized read/write stress vs reference model
 */

#include "AXI_Interconnect_AXI3.h"
#include "SimDDR_AXI3.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_WORDS = 0x100000; // 4MB

// ============================================================================
// Helpers
// ============================================================================

static uint32_t load_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static void store_le32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

static uint8_t pmem_read_byte(uint32_t addr) {
  uint32_t w = p_memory[addr >> 2];
  return static_cast<uint8_t>((w >> ((addr & 3u) * 8)) & 0xFFu);
}

static axi_interconnect::WideData256_t make_expected_read(
    const std::vector<uint8_t> &ref_mem, uint32_t addr, uint8_t total_size) {
  uint8_t out_bytes[32] = {0};
  uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
  for (uint32_t i = 0; i < bytes && i < 32; i++) {
    out_bytes[i] = ref_mem[addr + i];
  }

  axi_interconnect::WideData256_t out;
  out.clear();
  for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
    out[w] = load_le32(out_bytes + (w * 4));
  }
  return out;
}

static uint8_t calc_total_beats(uint32_t addr, uint8_t total_size) {
  uint32_t offset = addr & 0x1Fu;
  uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
  uint32_t span = offset + bytes;
  return static_cast<uint8_t>((span + sim_ddr_axi3::AXI_DATA_BYTES - 1) /
                              sim_ddr_axi3::AXI_DATA_BYTES);
}

struct DecodedId {
  uint8_t orig_id;
  uint8_t master_id;
  uint8_t offset_bytes;
  uint8_t total_size;
};

static DecodedId decode_id(uint32_t axi_id) {
  DecodedId d;
  d.orig_id = axi_id & 0xF;
  d.master_id = (axi_id >> 4) & 0x3;
  d.offset_bytes = (axi_id >> 6) & 0x1F;
  d.total_size = (axi_id >> 11) & 0x1F;
  return d;
}

struct ArEvent {
  uint32_t addr;
  uint32_t id;
  uint8_t len;
};

struct AwEvent {
  uint32_t addr;
  uint32_t id;
  uint8_t len;
};

struct WEvent {
  sim_ddr_axi3::Data256_t data;
  uint32_t strb;
  bool last;
};

struct TestEnv {
  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  sim_ddr_axi3::SimDDR_AXI3 ddr;
  std::vector<ArEvent> ar_events;
  std::vector<AwEvent> aw_events;
  std::vector<WEvent> w_events;

  void clear_events() {
    ar_events.clear();
    aw_events.clear();
    w_events.clear();
  }
};

static void clear_upstream_inputs(axi_interconnect::AXI_Interconnect_AXI3 &intlv) {
  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    intlv.read_ports[i].req.valid = false;
    intlv.read_ports[i].req.addr = 0;
    intlv.read_ports[i].req.total_size = 0;
    intlv.read_ports[i].req.id = 0;
    intlv.read_ports[i].resp.ready = false;
  }
  intlv.write_port.req.valid = false;
  intlv.write_port.req.addr = 0;
  intlv.write_port.req.wdata.clear();
  intlv.write_port.req.wstrb = 0;
  intlv.write_port.req.total_size = 0;
  intlv.write_port.req.id = 0;
  intlv.write_port.resp.ready = false;
}

static void init_memory(std::vector<uint8_t> &ref_mem) {
  ref_mem.assign(TEST_MEM_WORDS * 4, 0);
  for (uint32_t i = 0; i < TEST_MEM_WORDS; i++) {
    uint32_t v = (i * 0x9E3779B9u) ^ 0xA5A5A5A5u;
    p_memory[i] = v;
    store_le32(&ref_mem[i * 4], v);
  }
}

static void cycle_outputs(TestEnv &env) {
  clear_upstream_inputs(env.intlv);

  // Phase 1: DDR outputs -> interconnect inputs -> interconnect outputs
  env.ddr.comb_outputs();
  env.intlv.axi_io.ar.arready = env.ddr.io.ar.arready;
  env.intlv.axi_io.r.rvalid = env.ddr.io.r.rvalid;
  env.intlv.axi_io.r.rid = env.ddr.io.r.rid;
  env.intlv.axi_io.r.rdata = env.ddr.io.r.rdata;
  env.intlv.axi_io.r.rlast = env.ddr.io.r.rlast;
  env.intlv.axi_io.r.rresp = env.ddr.io.r.rresp;

  env.intlv.axi_io.aw.awready = env.ddr.io.aw.awready;
  env.intlv.axi_io.w.wready = env.ddr.io.w.wready;
  env.intlv.axi_io.b.bvalid = env.ddr.io.b.bvalid;
  env.intlv.axi_io.b.bid = env.ddr.io.b.bid;
  env.intlv.axi_io.b.bresp = env.ddr.io.b.bresp;

  env.intlv.comb_outputs();
}

static void cycle_inputs(TestEnv &env) {
  env.intlv.comb_inputs();

  // Record downstream handshakes
  if (env.intlv.axi_io.ar.arvalid && env.intlv.axi_io.ar.arready) {
    env.ar_events.push_back(
        {.addr = env.intlv.axi_io.ar.araddr,
         .id = env.intlv.axi_io.ar.arid,
         .len = static_cast<uint8_t>(env.intlv.axi_io.ar.arlen)});
  }
  if (env.intlv.axi_io.aw.awvalid && env.intlv.axi_io.aw.awready) {
    env.aw_events.push_back(
        {.addr = env.intlv.axi_io.aw.awaddr,
         .id = env.intlv.axi_io.aw.awid,
         .len = static_cast<uint8_t>(env.intlv.axi_io.aw.awlen)});
  }
  if (env.intlv.axi_io.w.wvalid && env.intlv.axi_io.w.wready) {
    env.w_events.push_back({.data = env.intlv.axi_io.w.wdata,
                            .strb = env.intlv.axi_io.w.wstrb,
                            .last = static_cast<bool>(env.intlv.axi_io.w.wlast)});
  }

  // Wire interconnect -> DDR inputs
  env.ddr.io.ar.arvalid = env.intlv.axi_io.ar.arvalid;
  env.ddr.io.ar.araddr = env.intlv.axi_io.ar.araddr;
  env.ddr.io.ar.arid = env.intlv.axi_io.ar.arid;
  env.ddr.io.ar.arlen = env.intlv.axi_io.ar.arlen;
  env.ddr.io.ar.arsize = env.intlv.axi_io.ar.arsize;
  env.ddr.io.ar.arburst = env.intlv.axi_io.ar.arburst;

  env.ddr.io.aw.awvalid = env.intlv.axi_io.aw.awvalid;
  env.ddr.io.aw.awaddr = env.intlv.axi_io.aw.awaddr;
  env.ddr.io.aw.awid = env.intlv.axi_io.aw.awid;
  env.ddr.io.aw.awlen = env.intlv.axi_io.aw.awlen;
  env.ddr.io.aw.awsize = env.intlv.axi_io.aw.awsize;
  env.ddr.io.aw.awburst = env.intlv.axi_io.aw.awburst;

  env.ddr.io.w.wvalid = env.intlv.axi_io.w.wvalid;
  env.ddr.io.w.wid = env.intlv.axi_io.w.wid;
  env.ddr.io.w.wdata = env.intlv.axi_io.w.wdata;
  env.ddr.io.w.wstrb = env.intlv.axi_io.w.wstrb;
  env.ddr.io.w.wlast = env.intlv.axi_io.w.wlast;

  env.ddr.io.r.rready = env.intlv.axi_io.r.rready;
  env.ddr.io.b.bready = env.intlv.axi_io.b.bready;

  env.ddr.comb_inputs();
  env.ddr.seq();
  env.intlv.seq();
  sim_time++;
}

// ============================================================================
// Tests
// ============================================================================

static bool test_ar_latching_ready_first() {
  printf("=== Test 1: AR latching (mocked arready) ===\n");

  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  intlv.init();

  bool req_valid = true;
  bool saw_ready = false;

  for (int cyc = 0; cyc < 8; cyc++) {
    clear_upstream_inputs(intlv);

    // Mock slave (no R activity)
    intlv.axi_io.ar.arready = (cyc >= 5);
    intlv.axi_io.r.rvalid = false;
    intlv.axi_io.r.rid = 0;
    intlv.axi_io.r.rdata.clear();
    intlv.axi_io.r.rlast = false;
    intlv.axi_io.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
    intlv.axi_io.aw.awready = true;
    intlv.axi_io.w.wready = true;
    intlv.axi_io.b.bvalid = false;
    intlv.axi_io.b.bid = 0;
    intlv.axi_io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

    intlv.comb_outputs();
    bool req_ready = intlv.read_ports[0].req.ready;

    if (req_valid) {
      intlv.read_ports[0].req.valid = true;
      intlv.read_ports[0].req.addr = 0x2000;
      intlv.read_ports[0].req.total_size = 3;
      intlv.read_ports[0].req.id = 1;
    }

    if (req_valid && req_ready) {
      saw_ready = true;
      req_valid = false;
    }

    intlv.comb_inputs();

    if (saw_ready && cyc >= 2 && cyc < 5 && !req_valid) {
      if (!intlv.axi_io.ar.arvalid) {
        printf("FAIL: arvalid dropped under backpressure\n");
        return false;
      }
    }

    intlv.seq();
    sim_time++;
  }

  if (!saw_ready) {
    printf("FAIL: never observed req.ready high\n");
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_read_multi_master_offsets(TestEnv &env,
                                          const std::vector<uint8_t> &ref_mem) {
  printf("=== Test 2: Multi-master reads (unaligned offsets) ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  struct Req {
    uint8_t master;
    uint32_t addr;
    uint8_t total_size;
    uint8_t id;
  };

  Req reqs[axi_interconnect::NUM_READ_MASTERS] = {
      {.master = axi_interconnect::MASTER_MMU, .addr = 0x1004, .total_size = 3, .id = 0xA},
      {.master = axi_interconnect::MASTER_DCACHE_R, .addr = 0x201C, .total_size = 7, .id = 0x7},
      {.master = axi_interconnect::MASTER_ICACHE, .addr = 0x3008, .total_size = 31, .id = 0x3},
  };

  bool accepted[axi_interconnect::NUM_READ_MASTERS] = {false, false, false};
  bool done[axi_interconnect::NUM_READ_MASTERS] = {false, false, false};
  int timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 400;
  while (timeout-- > 0) {
    bool all_done = true;
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      all_done &= done[i];
    }
    if (all_done) {
      break;
    }

    cycle_outputs(env);
    bool ready_snapshot[axi_interconnect::NUM_READ_MASTERS];
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      ready_snapshot[i] = env.intlv.read_ports[i].req.ready;
    }

    // Drive pending requests until accepted.
    for (const auto &r : reqs) {
      if (accepted[r.master]) {
        continue;
      }
      env.intlv.read_ports[r.master].req.valid = true;
      env.intlv.read_ports[r.master].req.addr = r.addr;
      env.intlv.read_ports[r.master].req.total_size = r.total_size;
      env.intlv.read_ports[r.master].req.id = r.id;
    }

    // Consume responses when they appear.
    for (const auto &r : reqs) {
      auto &port = env.intlv.read_ports[r.master];
      if (done[r.master] || !port.resp.valid) {
        continue;
      }

      if (port.resp.id != (r.id & 0xF)) {
        printf("FAIL: master %u resp.id mismatch exp=%u got=%u\n", r.master,
               r.id & 0xF, port.resp.id);
        return false;
      }

      auto exp = make_expected_read(ref_mem, r.addr, r.total_size);
      for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
        if (port.resp.data[w] != exp[w]) {
          printf("FAIL: master %u word %d exp=0x%08x got=0x%08x\n", r.master, w,
                 exp[w], port.resp.data[w]);
          return false;
        }
      }

      port.resp.ready = true;
      done[r.master] = true;
    }

    cycle_inputs(env);

    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      if (!accepted[i] && ready_snapshot[i]) {
        accepted[i] = true;
      }
    }
  }

  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    if (!accepted[i]) {
      printf("FAIL: master %d req not accepted\n", i);
      return false;
    }
    if (!done[i]) {
      printf("FAIL: master %d resp timeout\n", i);
      return false;
    }
  }

  if (env.ar_events.size() != axi_interconnect::NUM_READ_MASTERS) {
    printf("FAIL: expected %d AR handshakes, got %zu\n",
           axi_interconnect::NUM_READ_MASTERS, env.ar_events.size());
    return false;
  }

  // Validate AR fields and packed ID metadata
  for (const auto &e : env.ar_events) {
    DecodedId d = decode_id(e.id);
    bool found = false;
    for (const auto &r : reqs) {
      if (r.master != d.master_id) {
        continue;
      }
      found = true;
      uint8_t exp_beats = calc_total_beats(r.addr, r.total_size);
      uint32_t exp_addr = r.addr & ~0x1Fu;
      uint8_t exp_len = static_cast<uint8_t>(exp_beats - 1);
      if (e.addr != exp_addr || e.len != exp_len || d.orig_id != (r.id & 0xF) ||
          d.offset_bytes != (r.addr & 0x1F) || d.total_size != r.total_size) {
        printf("FAIL: AR mismatch master=%u addr=0x%x len=%u id=0x%x off=%u ts=%u\n",
               d.master_id, e.addr, e.len, d.orig_id, d.offset_bytes, d.total_size);
        return false;
      }
    }
    if (!found) {
      printf("FAIL: unexpected AR master_id=%u\n", d.master_id);
      return false;
    }
  }

  printf("PASS\n");
  return true;
}

static bool test_write_split_and_resp_backpressure(
    TestEnv &env, std::vector<uint8_t> &ref_mem) {
  printf("=== Test 3: Write split + resp backpressure ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint32_t addr = 0x501C; // offset 28 (cross 32B boundary when bytes=8)
  uint8_t total_size = 7;
  uint8_t bytes = total_size + 1;
  uint8_t req_id = 1;

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0x11223344;
  wdata[1] = 0x55667788;
  uint32_t wstrb = 0xFF;

  // Update reference memory
  uint8_t in_bytes[32] = {0};
  for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
    store_le32(in_bytes + (w * 4), wdata[w]);
  }
  for (uint8_t i = 0; i < bytes; i++) {
    if ((wstrb >> i) & 1u) {
      ref_mem[addr + i] = in_bytes[i];
    }
  }

  bool issued = false;
  int issue_timeout = 400;
  while (!issued && issue_timeout-- > 0) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.write_port.req.ready;

    env.intlv.write_port.req.valid = true;
    env.intlv.write_port.req.addr = addr;
    env.intlv.write_port.req.wdata = wdata;
    env.intlv.write_port.req.wstrb = wstrb;
    env.intlv.write_port.req.total_size = total_size;
    env.intlv.write_port.req.id = req_id;
    env.intlv.write_port.resp.ready = false;

    cycle_inputs(env);

    if (ready_snapshot) {
      issued = true;
    }
  }
  if (!issued) {
    printf("FAIL: write req not accepted\n");
    return false;
  }

  // Wait for resp.valid, then hold ready low for a few cycles.
  bool saw_resp = false;
  int resp_timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 200;
  while (resp_timeout-- > 0) {
    cycle_outputs(env);
    if (env.intlv.write_port.resp.valid) {
      saw_resp = true;
      break;
    }
    cycle_inputs(env);
  }
  if (!saw_resp) {
    printf("FAIL: write resp timeout\n");
    return false;
  }

  for (int i = 0; i < 3; i++) {
    cycle_outputs(env);
    if (!env.intlv.write_port.resp.valid) {
      printf("FAIL: resp.valid dropped under backpressure\n");
      return false;
    }
    if (env.intlv.write_port.req.ready) {
      printf("FAIL: req.ready should stay low while resp pending\n");
      return false;
    }
    env.intlv.write_port.resp.ready = false;
    cycle_inputs(env);
  }

  // Consume response
  cycle_outputs(env);
  if (!env.intlv.write_port.resp.valid) {
    printf("FAIL: resp.valid unexpectedly low before accept\n");
    return false;
  }
  if (env.intlv.write_port.resp.id != (req_id & 0xF) ||
      env.intlv.write_port.resp.resp != sim_ddr_axi3::AXI_RESP_OKAY) {
    printf("FAIL: write resp mismatch id=%u resp=%u\n",
           env.intlv.write_port.resp.id, env.intlv.write_port.resp.resp);
    return false;
  }
  env.intlv.write_port.resp.ready = true;
  cycle_inputs(env);

  cycle_outputs(env);
  if (env.intlv.write_port.resp.valid) {
    printf("FAIL: resp.valid not cleared after handshake\n");
    return false;
  }
  cycle_inputs(env);

  // Verify memory bytes updated
  for (uint8_t i = 0; i < bytes; i++) {
    uint8_t got = pmem_read_byte(addr + i);
    uint8_t exp = ref_mem[addr + i];
    if (got != exp) {
      printf("FAIL: mem byte[%u] exp=0x%02x got=0x%02x\n", i, exp, got);
      return false;
    }
  }

  // Validate downstream AW/W splitting behavior
  if (env.aw_events.size() != 1) {
    printf("FAIL: expected 1 AW handshake, got %zu\n", env.aw_events.size());
    return false;
  }
  DecodedId awid = decode_id(env.aw_events[0].id);
  if (env.aw_events[0].addr != (addr & ~0x1Fu) || env.aw_events[0].len != 1 ||
      awid.master_id != axi_interconnect::MASTER_DCACHE_W ||
      awid.orig_id != (req_id & 0xF) || awid.offset_bytes != (addr & 0x1F) ||
      awid.total_size != total_size) {
    printf("FAIL: AW mismatch addr=0x%x len=%u master=%u id=0x%x off=%u ts=%u\n",
           env.aw_events[0].addr, env.aw_events[0].len, awid.master_id,
           awid.orig_id, awid.offset_bytes, awid.total_size);
    return false;
  }

  if (env.w_events.size() != 2) {
    printf("FAIL: expected 2 W handshakes, got %zu\n", env.w_events.size());
    return false;
  }
  if (env.w_events[0].strb != 0xF0000000u || env.w_events[0].last) {
    printf("FAIL: W[0] strb/last mismatch strb=0x%x last=%d\n", env.w_events[0].strb,
           env.w_events[0].last);
    return false;
  }
  if (env.w_events[1].strb != 0x0000000Fu || !env.w_events[1].last) {
    printf("FAIL: W[1] strb/last mismatch strb=0x%x last=%d\n", env.w_events[1].strb,
           env.w_events[1].last);
    return false;
  }
  if (env.w_events[0].data[7] != 0x11223344u || env.w_events[1].data[0] != 0x55667788u) {
    printf("FAIL: W data mismatch beat0.word7=0x%08x beat1.word0=0x%08x\n",
           env.w_events[0].data[7], env.w_events[1].data[0]);
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_read_write_parallel(TestEnv &env,
                                     const std::vector<uint8_t> &ref_mem) {
  printf("=== Test 4: Read/write parallel ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint32_t r_addr = 0x6004;
  uint32_t w_addr = 0x7000;
  uint8_t r_id = 2;
  uint8_t w_id = 5;

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0xCAFEBABEu;

  bool r_issued = false;
  bool w_issued = false;
  int issue_timeout = 600;
  while (issue_timeout-- > 0) {
    if (r_issued && w_issued) {
      break;
    }

    cycle_outputs(env);
    bool r_ready_snapshot = env.intlv.read_ports[axi_interconnect::MASTER_ICACHE].req.ready;
    bool w_ready_snapshot = env.intlv.write_port.req.ready;

    if (!r_issued) {
      auto &rp = env.intlv.read_ports[axi_interconnect::MASTER_ICACHE];
      rp.req.valid = true;
      rp.req.addr = r_addr;
      rp.req.total_size = 3;
      rp.req.id = r_id;
      rp.resp.ready = true;
    }

    if (!w_issued) {
      env.intlv.write_port.req.valid = true;
      env.intlv.write_port.req.addr = w_addr;
      env.intlv.write_port.req.wdata = wdata;
      env.intlv.write_port.req.wstrb = 0xF;
      env.intlv.write_port.req.total_size = 3;
      env.intlv.write_port.req.id = w_id;
      env.intlv.write_port.resp.ready = true;
    }

    cycle_inputs(env);

    if (!r_issued && r_ready_snapshot) {
      r_issued = true;
    }
    if (!w_issued && w_ready_snapshot) {
      w_issued = true;
    }
  }
  if (!r_issued || !w_issued) {
    printf("FAIL: issue timeout r_issued=%d w_issued=%d\n", r_issued, w_issued);
    return false;
  }

  bool r_done = false;
  bool w_done = false;
  int timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 200;
  while (timeout-- > 0) {
    if (r_done && w_done) {
      break;
    }

    cycle_outputs(env);

    auto &rp = env.intlv.read_ports[axi_interconnect::MASTER_ICACHE];
    if (!r_done && rp.resp.valid) {
      auto exp = make_expected_read(ref_mem, r_addr, 3);
      if (rp.resp.id != (r_id & 0xF) || rp.resp.data[0] != exp[0]) {
        printf("FAIL: read resp mismatch id=%u data0=0x%08x exp=0x%08x\n",
               rp.resp.id, rp.resp.data[0], exp[0]);
        return false;
      }
      rp.resp.ready = true;
      r_done = true;
    }

    if (!w_done && env.intlv.write_port.resp.valid) {
      if (env.intlv.write_port.resp.id != (w_id & 0xF) ||
          env.intlv.write_port.resp.resp != sim_ddr_axi3::AXI_RESP_OKAY) {
        printf("FAIL: write resp mismatch id=%u resp=%u\n",
               env.intlv.write_port.resp.id, env.intlv.write_port.resp.resp);
        return false;
      }
      env.intlv.write_port.resp.ready = true;
      w_done = true;
    }

    cycle_inputs(env);
  }

  if (!r_done || !w_done) {
    printf("FAIL: resp timeout r_done=%d w_done=%d\n", r_done, w_done);
    return false;
  }

  if (p_memory[w_addr >> 2] != 0xCAFEBABEu) {
    printf("FAIL: write didn't land exp=0xCAFEBABE got=0x%08x\n",
           p_memory[w_addr >> 2]);
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_random_stress(TestEnv &env) {
  printf("=== Test 5: Randomized read/write stress ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  std::vector<uint8_t> ref_mem;
  init_memory(ref_mem);

  std::mt19937 rng(1);
  const uint8_t sizes[] = {0, 1, 3, 7, 15, 31};

  auto issue_one_read = [&](uint8_t master, uint32_t addr, uint8_t total_size,
                            uint8_t id) -> bool {
    bool issued = false;
    int issue_timeout = 800;
    while (!issued && issue_timeout-- > 0) {
      cycle_outputs(env);
      bool ready_snapshot = env.intlv.read_ports[master].req.ready;

      auto &rp = env.intlv.read_ports[master];
      rp.req.valid = true;
      rp.req.addr = addr;
      rp.req.total_size = total_size;
      rp.req.id = id;
      rp.resp.ready = false;

      cycle_inputs(env);
      if (ready_snapshot) {
        issued = true;
      }
    }
    if (!issued) {
      printf("FAIL: read issue timeout master=%u\n", master);
      return false;
    }

    bool saw_valid = false;
    axi_interconnect::WideData256_t first = {};
    int hold = static_cast<int>(rng() % 3);
    int timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 200;
    while (timeout-- > 0) {
      cycle_outputs(env);
      auto &rp = env.intlv.read_ports[master];
      if (rp.resp.valid) {
        if (!saw_valid) {
          saw_valid = true;
          first = rp.resp.data;
          if (rp.resp.id != (id & 0xF)) {
            printf("FAIL: read resp.id mismatch exp=%u got=%u\n", id & 0xF,
                   rp.resp.id);
            return false;
          }
          auto exp = make_expected_read(ref_mem, addr, total_size);
          for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
            if (first[w] != exp[w]) {
              printf("FAIL: read data mismatch word=%d exp=0x%08x got=0x%08x\n",
                     w, exp[w], first[w]);
              return false;
            }
          }
        } else {
          // Data must be stable while backpressured.
          for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
            if (rp.resp.data[w] != first[w]) {
              printf("FAIL: read data changed under backpressure\n");
              return false;
            }
          }
        }

        if (hold-- <= 0) {
          rp.resp.ready = true;
          cycle_inputs(env);
          return true;
        }
      }
      cycle_inputs(env);
    }
    printf("FAIL: read resp timeout\n");
    return false;
  };

  auto issue_one_write = [&](uint32_t addr, uint8_t total_size, uint8_t id,
                             const axi_interconnect::WideData256_t &wdata,
                             uint32_t wstrb) -> bool {
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    // Update reference model first (effectively commit-at-response, but no
    // overlapping ops in this test).
    uint8_t in_bytes[32] = {0};
    for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
      store_le32(in_bytes + (w * 4), wdata[w]);
    }
    for (uint32_t i = 0; i < bytes && i < 32; i++) {
      if ((wstrb >> i) & 1u) {
        ref_mem[addr + i] = in_bytes[i];
      }
    }

    bool issued = false;
    int issue_timeout = 800;
    while (!issued && issue_timeout-- > 0) {
      cycle_outputs(env);
      bool ready_snapshot = env.intlv.write_port.req.ready;

      env.intlv.write_port.req.valid = true;
      env.intlv.write_port.req.addr = addr;
      env.intlv.write_port.req.wdata = wdata;
      env.intlv.write_port.req.wstrb = wstrb;
      env.intlv.write_port.req.total_size = total_size;
      env.intlv.write_port.req.id = id;
      env.intlv.write_port.resp.ready = false;

      cycle_inputs(env);
      if (ready_snapshot) {
        issued = true;
      }
    }
    if (!issued) {
      printf("FAIL: write issue timeout\n");
      return false;
    }

    int hold = static_cast<int>(rng() % 3);
    int timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 400;
    while (timeout-- > 0) {
      cycle_outputs(env);
      if (env.intlv.write_port.resp.valid) {
        if (env.intlv.write_port.resp.id != (id & 0xF)) {
          printf("FAIL: write resp.id mismatch exp=%u got=%u\n", id & 0xF,
                 env.intlv.write_port.resp.id);
          return false;
        }
        if (env.intlv.write_port.resp.resp != sim_ddr_axi3::AXI_RESP_OKAY) {
          printf("FAIL: write resp not OKAY got=%u\n",
                 env.intlv.write_port.resp.resp);
          return false;
        }

        if (hold-- <= 0) {
          env.intlv.write_port.resp.ready = true;
          cycle_inputs(env);

          // Verify bytes landed
          for (uint32_t i = 0; i < bytes && i < 32; i++) {
            uint8_t got = pmem_read_byte(addr + i);
            uint8_t exp = ref_mem[addr + i];
            if (got != exp) {
              printf("FAIL: write byte mismatch i=%u exp=0x%02x got=0x%02x\n",
                     i, exp, got);
              return false;
            }
          }
          return true;
        }
      }
      cycle_inputs(env);
    }

    printf("FAIL: write resp timeout\n");
    return false;
  };

  constexpr uint32_t BASE = 0x8000;
  constexpr uint32_t RANGE = 0x4000;

  for (int iter = 0; iter < 1000; iter++) {
    uint8_t total_size = sizes[rng() % (sizeof(sizes) / sizeof(sizes[0]))];
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    uint32_t addr = BASE + (rng() % (RANGE - 128));
    addr &= ~0x3u;
    if (addr + bytes + 1 >= ref_mem.size()) {
      continue;
    }

    if ((rng() & 1u) == 0) {
      uint8_t master = rng() % axi_interconnect::NUM_READ_MASTERS;
      uint8_t id = rng() & 0xF;
      if (!issue_one_read(master, addr, total_size, id)) {
        return false;
      }
    } else {
      axi_interconnect::WideData256_t wdata;
      wdata.clear();
      for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
        wdata[w] = rng();
      }

      uint32_t wstrb = 0;
      for (uint32_t i = 0; i < bytes && i < 32; i++) {
        if (rng() & 1u) {
          wstrb |= (1u << i);
        }
      }
      if (wstrb == 0) {
        wstrb = 1u;
      }

      uint8_t id = rng() & 0xF;
      if (!issue_one_write(addr, total_size, id, wdata, wstrb)) {
        return false;
      }
    }
  }

  printf("PASS\n");
  return true;
}

int main() {
  p_memory = new uint32_t[TEST_MEM_WORDS];
  std::vector<uint8_t> ref_mem;
  init_memory(ref_mem);

  bool ok = true;
  ok &= test_ar_latching_ready_first();

  TestEnv env;
  ok &= test_read_multi_master_offsets(env, ref_mem);
  ok &= test_write_split_and_resp_backpressure(env, ref_mem);
  ok &= test_read_write_parallel(env, ref_mem);
  ok &= test_random_stress(env);

  delete[] p_memory;
  if (!ok) {
    printf("FAIL: axi_interconnect_axi3_test\n");
    return 1;
  }
  printf("PASS: axi_interconnect_axi3_test\n");
  return 0;
}
