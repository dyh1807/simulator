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
#include "axi_mmio_map.h"
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
  uint8_t size;
  uint8_t burst;
};

struct AwEvent {
  uint32_t addr;
  uint32_t id;
  uint8_t len;
  uint8_t size;
  uint8_t burst;
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
         .len = static_cast<uint8_t>(env.intlv.axi_io.ar.arlen),
         .size = static_cast<uint8_t>(env.intlv.axi_io.ar.arsize),
         .burst = static_cast<uint8_t>(env.intlv.axi_io.ar.arburst)});
  }
  if (env.intlv.axi_io.aw.awvalid && env.intlv.axi_io.aw.awready) {
    env.aw_events.push_back(
        {.addr = env.intlv.axi_io.aw.awaddr,
         .id = env.intlv.axi_io.aw.awid,
         .len = static_cast<uint8_t>(env.intlv.axi_io.aw.awlen),
         .size = static_cast<uint8_t>(env.intlv.axi_io.aw.awsize),
         .burst = static_cast<uint8_t>(env.intlv.axi_io.aw.awburst)});
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
      {.master = axi_interconnect::MASTER_EXTRA_R, .addr = 0x380C, .total_size = 15, .id = 0x4},
  };

  bool accepted[axi_interconnect::NUM_READ_MASTERS] = {};
  bool done[axi_interconnect::NUM_READ_MASTERS] = {};
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

static void set_slave_read_idle(axi_interconnect::AXI_Interconnect_AXI3 &intlv) {
  intlv.axi_io.ar.arready = true;
  intlv.axi_io.r.rvalid = false;
  intlv.axi_io.r.rid = 0;
  intlv.axi_io.r.rdata.clear();
  intlv.axi_io.r.rlast = false;
  intlv.axi_io.r.rresp = sim_ddr_axi3::AXI_RESP_OKAY;
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

static bool test_aw_latching_w_gate() {
  printf("=== Test 6: AW latching + W gate (mocked awready) ===\n");

  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  intlv.init();

  // One-beat write; keep upstream valid asserted until ready (ready-first),
  // then drop valid and force downstream awready low to verify latching.
  uint32_t addr = 0x4004;
  uint8_t total_size = 3; // 4B
  uint8_t req_id = 0xB;

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0xDEADBEEFu;
  uint32_t wstrb = 0xF;

  bool issued = false;
  bool aw_hs_seen = false;
  bool w_last_seen = false;
  bool b_hs_seen = false;
  bool resp_seen = false;

  // Hold awready low for a few cycles after the write is accepted to stress
  // AW latch behavior.
  int aw_hold_left = 3;
  uint32_t aw_addr_snap = 0;
  uint32_t aw_id_snap = 0;
  uint8_t aw_len_snap = 0;
  bool aw_snapped = false;

  // Simple B responder (assert bvalid a few cycles after WLAST)
  int b_delay = -1;
  bool b_valid = false;
  uint32_t b_id = 0;

  for (int cyc = 0; cyc < 300; cyc++) {
    clear_upstream_inputs(intlv);

    // Downstream: keep read channel idle
    set_slave_read_idle(intlv);

    // Downstream: write channel backpressure
    bool awready = !(issued && !aw_hs_seen && (aw_hold_left > 0));
    intlv.axi_io.aw.awready = awready;
    intlv.axi_io.w.wready = true;

    if (b_delay == 0) {
      b_valid = true;
    }
    intlv.axi_io.b.bvalid = b_valid;
    intlv.axi_io.b.bid = b_id;
    intlv.axi_io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

    intlv.comb_outputs();
    bool req_ready = intlv.write_port.req.ready;

    if (!issued) {
      intlv.write_port.req.valid = true;
      intlv.write_port.req.addr = addr;
      intlv.write_port.req.wdata = wdata;
      intlv.write_port.req.wstrb = wstrb;
      intlv.write_port.req.total_size = total_size;
      intlv.write_port.req.id = req_id;
    }
    intlv.write_port.resp.ready = true;

    intlv.comb_inputs();

    if (!issued && req_ready) {
      issued = true;
    }

    bool aw_hs_now = intlv.axi_io.aw.awvalid && intlv.axi_io.aw.awready;
    // While AW is backpressured, awvalid must remain asserted and stable, and
    // W must be gated off until AW handshake completes (or same-cycle handshake).
    if (!aw_hs_seen && !aw_hs_now && intlv.axi_io.w.wvalid) {
      printf("FAIL: wvalid asserted before AW handshake\n");
      return false;
    }

    if (intlv.axi_io.aw.awvalid) {
      if (!aw_snapped) {
        aw_snapped = true;
        aw_addr_snap = intlv.axi_io.aw.awaddr;
        aw_id_snap = intlv.axi_io.aw.awid;
        aw_len_snap = static_cast<uint8_t>(intlv.axi_io.aw.awlen);
      } else if (!intlv.axi_io.aw.awready) {
        if (intlv.axi_io.aw.awaddr != aw_addr_snap ||
            intlv.axi_io.aw.awid != aw_id_snap ||
            intlv.axi_io.aw.awlen != aw_len_snap) {
          printf("FAIL: AW changed under backpressure\n");
          return false;
        }
      }

      if (!intlv.axi_io.aw.awready && aw_hold_left > 0) {
        aw_hold_left--;
      }
    }

    bool aw_hs = aw_hs_now;
    if (aw_hs) {
      aw_hs_seen = true;
      b_id = intlv.axi_io.aw.awid;
    }

    bool w_hs = intlv.axi_io.w.wvalid && intlv.axi_io.w.wready;
    if (w_hs && intlv.axi_io.w.wlast) {
      w_last_seen = true;
      b_delay = 2;
    }

    if (b_delay > 0) {
      b_delay--;
    }

    bool b_hs = intlv.axi_io.b.bvalid && intlv.axi_io.b.bready;
    if (b_hs) {
      b_hs_seen = true;
      b_valid = false;
      b_delay = -1;
    }

    if (intlv.write_port.resp.valid) {
      resp_seen = true;
    }

    intlv.seq();
    sim_time++;

    if (issued && aw_hs_seen && w_last_seen && b_hs_seen && resp_seen &&
        !intlv.write_port.resp.valid) {
      break;
    }
  }

  if (!issued) {
    printf("FAIL: write req not accepted\n");
    return false;
  }
  if (!aw_snapped || !aw_hs_seen) {
    printf("FAIL: did not observe AW handshake\n");
    return false;
  }
  if (!w_last_seen) {
    printf("FAIL: did not observe WLAST handshake\n");
    return false;
  }
  if (!b_hs_seen) {
    printf("FAIL: did not observe B handshake\n");
    return false;
  }
  if (!resp_seen) {
    printf("FAIL: did not observe upstream write resp\n");
    return false;
  }

  // Basic AW field sanity
  DecodedId d = decode_id(aw_id_snap);
  if (aw_addr_snap != (addr & ~0x1Fu) || aw_len_snap != 0 ||
      d.master_id != axi_interconnect::MASTER_DCACHE_W ||
      d.orig_id != (req_id & 0xF) || d.offset_bytes != (addr & 0x1F) ||
      d.total_size != total_size) {
    printf("FAIL: AW mismatch addr=0x%x len=%u master=%u id=0x%x off=%u ts=%u\n",
           aw_addr_snap, aw_len_snap, d.master_id, d.orig_id, d.offset_bytes,
           d.total_size);
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_w_backpressure_data_stability() {
  printf("=== Test 7: W backpressure holds data stable (mocked wready) ===\n");

  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  intlv.init();

  // 32B write at offset 24 => 2 beats (8B in beat0, 24B in beat1)
  uint32_t addr = 0x5018; // offset 24
  uint8_t total_size = 31;
  uint8_t req_id = 0x3;

  uint8_t in_bytes[32] = {0};
  for (int i = 0; i < 32; i++) {
    in_bytes[i] = static_cast<uint8_t>(i);
  }

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
    wdata[w] = load_le32(in_bytes + (w * 4));
  }

  uint32_t wstrb = 0xFFFFFFFFu;

  bool issued = false;
  bool aw_hs_seen = false;
  uint32_t aw_id = 0;
  AwEvent aw_event = {};
  std::vector<WEvent> w_events;

  // Apply per-beat W backpressure
  int w_hold_left[2] = {3, 2};
  bool w_snapped[2] = {false, false};
  sim_ddr_axi3::Data256_t w_data_snap[2];
  uint32_t w_strb_snap[2] = {0, 0};
  bool w_last_snap[2] = {false, false};
  int w_hs_cnt = 0;

  // B responder
  int b_delay = -1;
  bool b_valid = false;
  uint32_t b_id = 0;
  bool resp_seen = false;

  for (int cyc = 0; cyc < 600; cyc++) {
    clear_upstream_inputs(intlv);
    set_slave_read_idle(intlv);

    // Downstream AW always ready for this test.
    intlv.axi_io.aw.awready = true;

    // Downstream W backpressure based on current beat index.
    bool wready = true;
    if (aw_hs_seen && w_hs_cnt < 2 && w_hold_left[w_hs_cnt] > 0) {
      wready = false;
    }
    intlv.axi_io.w.wready = wready;

    if (b_delay == 0) {
      b_valid = true;
    }
    intlv.axi_io.b.bvalid = b_valid;
    intlv.axi_io.b.bid = b_id;
    intlv.axi_io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

    intlv.comb_outputs();
    bool req_ready = intlv.write_port.req.ready;

    if (!issued) {
      intlv.write_port.req.valid = true;
      intlv.write_port.req.addr = addr;
      intlv.write_port.req.wdata = wdata;
      intlv.write_port.req.wstrb = wstrb;
      intlv.write_port.req.total_size = total_size;
      intlv.write_port.req.id = req_id;
    }
    intlv.write_port.resp.ready = true;

    intlv.comb_inputs();

    if (!issued && req_ready) {
      issued = true;
    }

    bool aw_hs_now = intlv.axi_io.aw.awvalid && intlv.axi_io.aw.awready;
    if (!aw_hs_seen && !aw_hs_now && intlv.axi_io.w.wvalid) {
      printf("FAIL: wvalid asserted before AW handshake\n");
      return false;
    }

    // Check W stability while backpressured
    if (intlv.axi_io.w.wvalid && !intlv.axi_io.w.wready) {
      if (w_hs_cnt >= 2) {
        printf("FAIL: wvalid asserted beyond expected beats\n");
        return false;
      }
      if (!w_snapped[w_hs_cnt]) {
        w_snapped[w_hs_cnt] = true;
        w_data_snap[w_hs_cnt] = intlv.axi_io.w.wdata;
        w_strb_snap[w_hs_cnt] = intlv.axi_io.w.wstrb;
        w_last_snap[w_hs_cnt] = static_cast<bool>(intlv.axi_io.w.wlast);
      } else {
        for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
          if (intlv.axi_io.w.wdata[w] != w_data_snap[w_hs_cnt][w]) {
            printf("FAIL: wdata changed under backpressure beat=%d\n", w_hs_cnt);
            return false;
          }
        }
        if (intlv.axi_io.w.wstrb != w_strb_snap[w_hs_cnt] ||
            static_cast<bool>(intlv.axi_io.w.wlast) != w_last_snap[w_hs_cnt]) {
          printf("FAIL: wstrb/wlast changed under backpressure beat=%d\n", w_hs_cnt);
          return false;
        }
      }

      if (w_hold_left[w_hs_cnt] > 0) {
        w_hold_left[w_hs_cnt]--;
      }
    }

    bool aw_hs = aw_hs_now;
    if (aw_hs) {
      aw_hs_seen = true;
      aw_id = intlv.axi_io.aw.awid;
      aw_event.addr = intlv.axi_io.aw.awaddr;
      aw_event.id = aw_id;
      aw_event.len = static_cast<uint8_t>(intlv.axi_io.aw.awlen);
      b_id = aw_id;
    }

    bool w_hs = intlv.axi_io.w.wvalid && intlv.axi_io.w.wready;
    if (w_hs) {
      if (intlv.axi_io.w.wid != aw_id) {
        printf("FAIL: wid mismatch exp=0x%x got=0x%x\n", aw_id,
               intlv.axi_io.w.wid);
        return false;
      }
      w_events.push_back({.data = intlv.axi_io.w.wdata,
                          .strb = intlv.axi_io.w.wstrb,
                          .last = static_cast<bool>(intlv.axi_io.w.wlast)});
      w_hs_cnt++;
      if (intlv.axi_io.w.wlast) {
        b_delay = 2;
      }
    }

    if (b_delay > 0) {
      b_delay--;
    }
    bool b_hs = intlv.axi_io.b.bvalid && intlv.axi_io.b.bready;
    if (b_hs) {
      b_valid = false;
      b_delay = -1;
    }

    if (intlv.write_port.resp.valid) {
      resp_seen = true;
    }

    intlv.seq();
    sim_time++;

    if (issued && aw_hs_seen && w_hs_cnt == 2 && resp_seen &&
        !intlv.write_port.resp.valid) {
      break;
    }
  }

  if (!issued) {
    printf("FAIL: write req not accepted\n");
    return false;
  }
  if (!aw_hs_seen) {
    printf("FAIL: did not observe AW handshake\n");
    return false;
  }
  if (aw_event.addr != (addr & ~0x1Fu) || aw_event.len != 1) {
    printf("FAIL: AW addr/len mismatch addr=0x%x len=%u\n", aw_event.addr,
           aw_event.len);
    return false;
  }
  DecodedId awid = decode_id(aw_event.id);
  if (awid.master_id != axi_interconnect::MASTER_DCACHE_W ||
      awid.orig_id != (req_id & 0xF) || awid.offset_bytes != (addr & 0x1F) ||
      awid.total_size != total_size) {
    printf("FAIL: AW id decode mismatch master=%u id=0x%x off=%u ts=%u\n",
           awid.master_id, awid.orig_id, awid.offset_bytes, awid.total_size);
    return false;
  }

  if (w_events.size() != 2) {
    printf("FAIL: expected 2 W handshakes, got %zu\n", w_events.size());
    return false;
  }
  if (w_events[0].last) {
    printf("FAIL: W[0] last should be 0\n");
    return false;
  }
  if (!w_events[1].last) {
    printf("FAIL: W[1] last should be 1\n");
    return false;
  }

  if (w_events[0].strb != 0xFF000000u) {
    printf("FAIL: W[0] strb mismatch exp=0xFF000000 got=0x%x\n", w_events[0].strb);
    return false;
  }
  if (w_events[1].strb != 0x00FFFFFFu) {
    printf("FAIL: W[1] strb mismatch exp=0x00FFFFFF got=0x%x\n", w_events[1].strb);
    return false;
  }

  // Spot-check alignment / byte placement.
  if (w_events[0].data[6] != 0x03020100u || w_events[0].data[7] != 0x07060504u ||
      w_events[1].data[0] != 0x0B0A0908u || w_events[1].data[5] != 0x1F1E1D1Cu) {
    printf("FAIL: W data mismatch b0.w6=0x%08x b0.w7=0x%08x b1.w0=0x%08x b1.w5=0x%08x\n",
           w_events[0].data[6], w_events[0].data[7], w_events[1].data[0],
           w_events[1].data[5]);
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_random_write_stress_ready_backpressure() {
  printf("=== Test 8: Randomized write stress (AW/W backpressure) ===\n");

  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  intlv.init();

  constexpr uint32_t MEM_BYTES = 64 * 1024;
  std::vector<uint8_t> ref_mem(MEM_BYTES);
  std::vector<uint8_t> dut_mem(MEM_BYTES);

  std::mt19937 rng(2);
  for (uint32_t i = 0; i < MEM_BYTES; i++) {
    uint8_t v = static_cast<uint8_t>(rng() & 0xFFu);
    ref_mem[i] = v;
    dut_mem[i] = v;
  }

  auto apply_upstream_write = [&](uint32_t addr, uint8_t total_size,
                                  const axi_interconnect::WideData256_t &wdata,
                                  uint32_t wstrb) {
    uint32_t bytes = static_cast<uint32_t>(total_size) + 1;
    uint8_t in_bytes[32] = {0};
    for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
      store_le32(in_bytes + (w * 4), wdata[w]);
    }
    for (uint32_t i = 0; i < bytes && i < 32; i++) {
      if ((wstrb >> i) & 1u) {
        ref_mem[addr + i] = in_bytes[i];
      }
    }
  };

  auto apply_axi_wbeat = [&](uint32_t beat_addr, const sim_ddr_axi3::Data256_t &data,
                             uint32_t strb) {
    uint8_t beat_bytes[32] = {0};
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      store_le32(beat_bytes + (w * 4), data[w]);
    }
    for (uint32_t i = 0; i < 32; i++) {
      if ((strb >> i) & 1u) {
        dut_mem[beat_addr + i] = beat_bytes[i];
      }
    }
  };

  // Slave-side transaction tracking (single outstanding)
  bool slave_aw_seen = false;
  uint32_t slave_aw_addr = 0;
  uint32_t slave_aw_id = 0;
  uint8_t slave_aw_len = 0;
  uint32_t slave_beats_expected = 0;
  uint32_t slave_beats_rcv = 0;
  int slave_b_delay = -1;
  bool slave_b_valid = false;

  struct PendingWrite {
    bool active;
    uint32_t addr;
    uint8_t total_size;
    uint8_t id;
    axi_interconnect::WideData256_t wdata;
    uint32_t wstrb;
  } op = {};

  static constexpr uint8_t kSizes[] = {0, 1, 3, 7, 15, 31};

  constexpr int ITERS = 400;
  for (int iter = 0; iter < ITERS; iter++) {
    // New operation
    op.active = true;
    op.total_size = ((rng() & 1u) ? static_cast<uint8_t>(rng() % 32)
                                  : kSizes[rng() % (sizeof(kSizes) /
                                                   sizeof(kSizes[0]))]);
    uint32_t bytes = static_cast<uint32_t>(op.total_size) + 1;
    op.id = static_cast<uint8_t>(rng() & 0xF);

    // Ensure aligned_addr+64 and addr+bytes both fit in MEM_BYTES.
    uint32_t addr = static_cast<uint32_t>(rng() % (MEM_BYTES - 128));
    uint32_t aligned_addr = addr & ~0x1Fu;
    addr = aligned_addr | (rng() & 0x1Fu);
    if (aligned_addr + 64 >= MEM_BYTES || addr + bytes >= MEM_BYTES) {
      iter--;
      continue;
    }
    op.addr = addr;

    uint8_t in_bytes[32] = {0};
    for (int i = 0; i < 32; i++) {
      in_bytes[i] = static_cast<uint8_t>(rng() & 0xFFu);
    }
    op.wdata.clear();
    for (int w = 0; w < axi_interconnect::CACHELINE_WORDS; w++) {
      op.wdata[w] = load_le32(in_bytes + (w * 4));
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
    op.wstrb = wstrb;

    bool accepted = false;
    bool resp_done = false;
    int resp_hold = static_cast<int>(rng() % 3);

    // Reset slave-side state for this op.
    slave_aw_seen = false;
    slave_aw_addr = 0;
    slave_aw_id = 0;
    slave_aw_len = 0;
    slave_beats_expected = 0;
    slave_beats_rcv = 0;
    slave_b_delay = -1;
    slave_b_valid = false;

    int timeout = 20000;
    while (!resp_done && timeout-- > 0) {
      clear_upstream_inputs(intlv);
      set_slave_read_idle(intlv);

      // Drive slave ready/valid (random backpressure, single outstanding)
      bool allow_aw = !slave_aw_seen && !slave_b_valid;
      intlv.axi_io.aw.awready = allow_aw && ((rng() & 3u) != 0u);

      bool allow_w = slave_aw_seen && (slave_beats_rcv < slave_beats_expected);
      intlv.axi_io.w.wready = allow_w && ((rng() & 1u) != 0u);

      if (slave_b_delay == 0) {
        slave_b_valid = true;
      }
      intlv.axi_io.b.bvalid = slave_b_valid;
      intlv.axi_io.b.bid = slave_aw_id;
      intlv.axi_io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

      intlv.comb_outputs();

      if (!accepted) {
        intlv.write_port.req.valid = true;
        intlv.write_port.req.addr = op.addr;
        intlv.write_port.req.wdata = op.wdata;
        intlv.write_port.req.wstrb = op.wstrb;
        intlv.write_port.req.total_size = op.total_size;
        intlv.write_port.req.id = op.id;
      } else {
        // Once the op is accepted, req.ready should stay low until response is consumed.
        if (intlv.write_port.req.ready) {
          printf("FAIL: req.ready high while write in-flight (iter=%d)\n", iter);
          return false;
        }
      }

      if (accepted && intlv.write_port.resp.valid) {
        if (resp_hold-- <= 0) {
          intlv.write_port.resp.ready = true;
        } else {
          intlv.write_port.resp.ready = false;
        }
      } else {
        intlv.write_port.resp.ready = false;
      }

      intlv.comb_inputs();

      // Upstream request handshake
      if (!accepted && intlv.write_port.req.valid && intlv.write_port.req.ready) {
        accepted = true;
        apply_upstream_write(op.addr, op.total_size, op.wdata, op.wstrb);
      }

      // AW handshake
      bool aw_hs = intlv.axi_io.aw.awvalid && intlv.axi_io.aw.awready;
      if (aw_hs) {
        if (slave_aw_seen) {
          printf("FAIL: multiple AW handshakes without completing previous op\n");
          return false;
        }
        if (intlv.axi_io.aw.awsize != 5 ||
            intlv.axi_io.aw.awburst != sim_ddr_axi3::AXI_BURST_INCR) {
          printf("FAIL: AW size/burst mismatch size=%u burst=%u\n",
                 static_cast<uint32_t>(intlv.axi_io.aw.awsize),
                 static_cast<uint32_t>(intlv.axi_io.aw.awburst));
          return false;
        }
        slave_aw_seen = true;
        slave_aw_addr = intlv.axi_io.aw.awaddr;
        slave_aw_id = intlv.axi_io.aw.awid;
        slave_aw_len = static_cast<uint8_t>(intlv.axi_io.aw.awlen);
        slave_beats_expected = static_cast<uint32_t>(slave_aw_len) + 1;
        slave_beats_rcv = 0;
      }

      // W must not appear before AW handshake in this constrained bridge.
      if (!slave_aw_seen && intlv.axi_io.w.wvalid) {
        printf("FAIL: wvalid asserted before AW handshake (iter=%d)\n", iter);
        return false;
      }

      // W handshake
      bool w_hs = intlv.axi_io.w.wvalid && intlv.axi_io.w.wready;
      if (w_hs) {
        if (!slave_aw_seen) {
          printf("FAIL: W handshake before AW handshake (iter=%d)\n", iter);
          return false;
        }
        if (intlv.axi_io.w.wid != slave_aw_id) {
          printf("FAIL: WID mismatch exp=0x%x got=0x%x (iter=%d)\n", slave_aw_id,
                 intlv.axi_io.w.wid, iter);
          return false;
        }
        uint32_t beat_addr = slave_aw_addr + (slave_beats_rcv * 32);
        if (beat_addr + 32 > MEM_BYTES) {
          printf("FAIL: beat_addr OOB beat_addr=0x%x (iter=%d)\n", beat_addr,
                 iter);
          return false;
        }
        apply_axi_wbeat(beat_addr, intlv.axi_io.w.wdata, intlv.axi_io.w.wstrb);
        slave_beats_rcv++;

        bool is_last = static_cast<bool>(intlv.axi_io.w.wlast);
        if (is_last != (slave_beats_rcv == slave_beats_expected)) {
          printf("FAIL: WLAST mismatch beats_rcv=%u exp=%u last=%d (iter=%d)\n",
                 slave_beats_rcv, slave_beats_expected, is_last, iter);
          return false;
        }
        if (is_last) {
          slave_b_delay = static_cast<int>(rng() % 6);
        }
      }

      if (slave_b_delay > 0) {
        slave_b_delay--;
      }

      // B handshake (accept response)
      bool b_hs = intlv.axi_io.b.bvalid && intlv.axi_io.b.bready;
      if (b_hs) {
        slave_b_valid = false;
        slave_b_delay = -1;
      }

      // Upstream response handshake (done)
      if (accepted && intlv.write_port.resp.valid && intlv.write_port.resp.ready) {
        resp_done = true;
      }

      intlv.seq();
      sim_time++;
    }

    if (!accepted) {
      printf("FAIL: write req not accepted (iter=%d)\n", iter);
      return false;
    }
    if (!resp_done) {
      printf("FAIL: write resp timeout (iter=%d)\n", iter);
      return false;
    }
    op.active = false;
  }

  for (uint32_t i = 0; i < MEM_BYTES; i++) {
    if (dut_mem[i] != ref_mem[i]) {
      printf("FAIL: mem mismatch at +0x%x exp=0x%02x got=0x%02x\n", i,
             ref_mem[i], dut_mem[i]);
      return false;
    }
  }

  printf("PASS\n");
  return true;
}

static bool test_aw_w_no_bubble() {
  printf("=== Test 9: AW/W same-cycle when awready=1 ===\n");

  axi_interconnect::AXI_Interconnect_AXI3 intlv;
  intlv.init();

  uint32_t addr = 0x4000;
  uint8_t total_size = 3; // 4B
  uint8_t req_id = 0x6;

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0x12345678u;
  uint32_t wstrb = 0xF;

  bool issued = false;
  int aw_cycle = -1;
  int w_cycle = -1;
  bool resp_done = false;

  int b_delay = -1;
  bool b_valid = false;
  uint32_t b_id = 0;

  for (int cyc = 0; cyc < 200; cyc++) {
    clear_upstream_inputs(intlv);
    set_slave_read_idle(intlv);

    intlv.axi_io.aw.awready = true;
    intlv.axi_io.w.wready = true;

    if (b_delay == 0) {
      b_valid = true;
    }
    intlv.axi_io.b.bvalid = b_valid;
    intlv.axi_io.b.bid = b_id;
    intlv.axi_io.b.bresp = sim_ddr_axi3::AXI_RESP_OKAY;

    intlv.comb_outputs();
    bool req_ready = intlv.write_port.req.ready;

    if (!issued) {
      intlv.write_port.req.valid = true;
      intlv.write_port.req.addr = addr;
      intlv.write_port.req.wdata = wdata;
      intlv.write_port.req.wstrb = wstrb;
      intlv.write_port.req.total_size = total_size;
      intlv.write_port.req.id = req_id;
    }
    intlv.write_port.resp.ready = true;

    intlv.comb_inputs();

    if (!issued && req_ready) {
      issued = true;
    }

    bool aw_hs = intlv.axi_io.aw.awvalid && intlv.axi_io.aw.awready;
    if (aw_hs && aw_cycle < 0) {
      aw_cycle = cyc;
      b_id = intlv.axi_io.aw.awid;
    }

    bool w_hs = intlv.axi_io.w.wvalid && intlv.axi_io.w.wready;
    if (w_hs && w_cycle < 0) {
      w_cycle = cyc;
      if (!intlv.axi_io.w.wlast) {
        printf("FAIL: single-beat write should assert wlast\n");
        return false;
      }
      b_delay = 1;
    }

    if (b_delay > 0) {
      b_delay--;
    }

    if (intlv.write_port.resp.valid) {
      intlv.write_port.resp.ready = true;
    }

    bool resp_hs =
        intlv.write_port.resp.valid && intlv.write_port.resp.ready;
    if (resp_hs) {
      resp_done = true;
    }

    intlv.seq();
    sim_time++;

    if (issued && resp_done) {
      break;
    }
  }

  if (!issued) {
    printf("FAIL: write req not accepted\n");
    return false;
  }
  if (aw_cycle < 0 || w_cycle < 0) {
    printf("FAIL: missing AW/W handshake (aw_cycle=%d w_cycle=%d)\n", aw_cycle,
           w_cycle);
    return false;
  }
  if (aw_cycle != w_cycle) {
    printf("FAIL: AW/W bubble detected aw_cycle=%d w_cycle=%d\n", aw_cycle,
           w_cycle);
    return false;
  }
  if (!resp_done) {
    printf("FAIL: write resp not observed\n");
    return false;
  }

  printf("PASS\n");
  return true;
}

static bool test_mmio_fixed_burst(TestEnv &env) {
  printf("=== Test 10: MMIO uses FIXED burst ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint32_t addr = MMIO_RANGE_BASE + 0x4;
  uint8_t total_size = 3; // 4B
  uint8_t id = 0x2;

  // Ensure backing memory exists for this address in test build
  if ((addr >> 2) < TEST_MEM_WORDS) {
    p_memory[addr >> 2] = 0x11223344u;
  }

  bool issued = false;
  bool done = false;
  int timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 200;
  while (timeout-- > 0 && !done) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.read_ports[axi_interconnect::MASTER_MMU].req.ready;

    if (!issued) {
      auto &rp = env.intlv.read_ports[axi_interconnect::MASTER_MMU];
      rp.req.valid = true;
      rp.req.addr = addr;
      rp.req.total_size = total_size;
      rp.req.id = id;
    }

    if (env.intlv.read_ports[axi_interconnect::MASTER_MMU].resp.valid) {
      env.intlv.read_ports[axi_interconnect::MASTER_MMU].resp.ready = true;
      done = true;
    }

    cycle_inputs(env);

    if (!issued && ready_snapshot) {
      issued = true;
    }
  }
  if (!issued || !done) {
    printf("FAIL: MMIO read did not complete\n");
    return false;
  }
  if (env.ar_events.size() != 1) {
    printf("FAIL: expected 1 AR handshake, got %zu\n", env.ar_events.size());
    return false;
  }
  const auto &ar = env.ar_events[0];
  if (ar.len != 0 || ar.size != 5 ||
      ar.burst != sim_ddr_axi3::AXI_BURST_FIXED) {
    printf("FAIL: MMIO AR burst/len/size mismatch len=%u size=%u burst=%u\n",
           ar.len, ar.size, ar.burst);
    return false;
  }

  // Write request to MMIO range
  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0xAABBCCDDu;

  issued = false;
  done = false;
  timeout = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 200;
  while (timeout-- > 0 && !done) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.write_port.req.ready;

    if (!issued) {
      env.intlv.write_port.req.valid = true;
      env.intlv.write_port.req.addr = addr;
      env.intlv.write_port.req.wdata = wdata;
      env.intlv.write_port.req.wstrb = 0xF;
      env.intlv.write_port.req.total_size = total_size;
      env.intlv.write_port.req.id = id;
    }

    if (env.intlv.write_port.resp.valid) {
      env.intlv.write_port.resp.ready = true;
      done = true;
    }

    cycle_inputs(env);

    if (!issued && ready_snapshot) {
      issued = true;
    }
  }
  if (!issued || !done) {
    printf("FAIL: MMIO write did not complete\n");
    return false;
  }
  if (env.aw_events.size() != 1) {
    printf("FAIL: expected 1 AW handshake, got %zu\n", env.aw_events.size());
    return false;
  }
  const auto &aw = env.aw_events[0];
  if (aw.len != 0 || aw.size != 5 ||
      aw.burst != sim_ddr_axi3::AXI_BURST_FIXED) {
    printf("FAIL: MMIO AW burst/len/size mismatch len=%u size=%u burst=%u\n",
           aw.len, aw.size, aw.burst);
    return false;
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
  ok &= test_aw_latching_w_gate();
  ok &= test_w_backpressure_data_stability();
  ok &= test_random_write_stress_ready_backpressure();
  ok &= test_aw_w_no_bubble();
  ok &= test_mmio_fixed_burst(env);

  delete[] p_memory;
  if (!ok) {
    printf("FAIL: axi_interconnect_axi3_test\n");
    return 1;
  }
  printf("PASS: axi_interconnect_axi3_test\n");
  return 0;
}
