/**
 * @file axi_interconnect_test.cpp
 * @brief Test harness for AXI_Interconnect layer
 */

#include "AXI_Interconnect.h"
#include "SimDDR.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_SIZE_WORDS = 0x100000;

// ============================================================================
// Helpers
// ============================================================================

struct ArEvent {
  uint32_t addr;
  uint8_t id;
  uint8_t len;
};

struct AwEvent {
  uint32_t addr;
  uint8_t id;
  uint8_t len;
};

struct WEvent {
  uint32_t data;
  uint8_t strb;
  bool last;
};

struct TestEnv {
  axi_interconnect::AXI_Interconnect intlv;
  sim_ddr::SimDDR ddr;
  std::vector<ArEvent> ar_events;
  std::vector<AwEvent> aw_events;
  std::vector<WEvent> w_events;

  void clear_events() {
    ar_events.clear();
    aw_events.clear();
    w_events.clear();
  }
};

uint8_t calc_burst_len(uint8_t total_size) {
  uint8_t bytes = total_size + 1;
  uint8_t beats = (bytes + 3) / 4;
  return beats > 0 ? (beats - 1) : 0;
}

uint8_t calc_total_beats(uint8_t total_size) { return calc_burst_len(total_size) + 1; }

void clear_upstream_inputs(axi_interconnect::AXI_Interconnect &intlv) {
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

void cycle_outputs(TestEnv &env) {
  clear_upstream_inputs(env.intlv);

  // Phase 1: DDR outputs → interconnect inputs → interconnect outputs
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

void cycle_inputs(TestEnv &env) {
  // Phase 2: interconnect inputs → DDR inputs
  env.intlv.comb_inputs();

  // Record DDR-side handshakes for protocol/beat-splitting checks
  if (env.intlv.axi_io.ar.arvalid && env.intlv.axi_io.ar.arready) {
    env.ar_events.push_back(
        {.addr = env.intlv.axi_io.ar.araddr,
         .id = static_cast<uint8_t>(env.intlv.axi_io.ar.arid),
         .len = static_cast<uint8_t>(env.intlv.axi_io.ar.arlen)});
  }
  if (env.intlv.axi_io.aw.awvalid && env.intlv.axi_io.aw.awready) {
    env.aw_events.push_back(
        {.addr = env.intlv.axi_io.aw.awaddr,
         .id = static_cast<uint8_t>(env.intlv.axi_io.aw.awid),
         .len = static_cast<uint8_t>(env.intlv.axi_io.aw.awlen)});
  }
  if (env.intlv.axi_io.w.wvalid && env.intlv.axi_io.w.wready) {
    env.w_events.push_back(
        {.data = env.intlv.axi_io.w.wdata,
         .strb = static_cast<uint8_t>(env.intlv.axi_io.w.wstrb),
         .last = env.intlv.axi_io.w.wlast});
  }

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

bool test_ar_latching_ready_first() {
  printf("=== Test 1: AR latching (mocked arready) ===\n");

  axi_interconnect::AXI_Interconnect intlv;
  intlv.init();

  // Drive a request and force arready low; interconnect should latch AR and
  // keep arvalid asserted even if req.valid drops.
  bool req_valid = true;
  bool saw_ready = false;

  for (int cyc = 0; cyc < 8; cyc++) {
    clear_upstream_inputs(intlv);

    // Mock AXI slave (no R channel activity)
    intlv.axi_io.ar.arready = (cyc >= 5); // backpressure for first 5 cycles
    intlv.axi_io.r.rvalid = false;
    intlv.axi_io.r.rid = 0;
    intlv.axi_io.r.rdata = 0;
    intlv.axi_io.r.rlast = false;
    intlv.axi_io.r.rresp = sim_ddr::AXI_RESP_OKAY;
    intlv.axi_io.aw.awready = true;
    intlv.axi_io.w.wready = true;
    intlv.axi_io.b.bvalid = false;
    intlv.axi_io.b.bid = 0;
    intlv.axi_io.b.bresp = sim_ddr::AXI_RESP_OKAY;

    intlv.comb_outputs();
    bool req_ready = intlv.read_ports[0].req.ready;

    // Keep valid asserted until we see ready (ready-first handshake)
    if (req_valid) {
      intlv.read_ports[0].req.valid = true;
      intlv.read_ports[0].req.addr = 0x2000;
      intlv.read_ports[0].req.total_size = 3;
      intlv.read_ports[0].req.id = 1;
    }

    if (req_valid && req_ready) {
      saw_ready = true;
      req_valid = false; // drop after handshake
    }

    intlv.comb_inputs();

    if (saw_ready && cyc >= 2 && cyc < 5 && !req_valid) {
      // After req.valid drops, arvalid must stay asserted while arready=0.
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

bool test_read_multi_master_var_sizes(TestEnv &env) {
  printf("=== Test 2: Multi-master reads (sizes/ids) ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  // master 0: 4B (1 beat), master 1: 8B (2 beats), master 2: 32B (8 beats)
  struct Req {
    uint8_t master;
    uint32_t addr;
    uint8_t total_size;
    uint8_t id;
  };
  Req reqs[axi_interconnect::NUM_READ_MASTERS] = {
      {.master = 0, .addr = 0x1000, .total_size = 3, .id = 0},
      {.master = 1, .addr = 0x2000, .total_size = 7, .id = 1},
      {.master = 2, .addr = 0x3000, .total_size = 31, .id = 2},
  };

  for (const auto &r : reqs) {
    uint8_t beats = calc_total_beats(r.total_size);
    for (int b = 0; b < beats; b++) {
      p_memory[(r.addr >> 2) + b] = (r.master << 28) | (b << 16) | 0x1234;
    }
  }

  bool issued[axi_interconnect::NUM_READ_MASTERS] = {false, false, false};
  int issue_timeout = 200;
  while (issue_timeout-- > 0) {
    bool all = true;
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      all &= issued[i];
    }
    if (all)
      break;

    cycle_outputs(env);
    bool ready_snapshot[axi_interconnect::NUM_READ_MASTERS];
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      ready_snapshot[i] = env.intlv.read_ports[i].req.ready;
    }

    for (const auto &r : reqs) {
      if (issued[r.master])
        continue;
      env.intlv.read_ports[r.master].req.valid = true;
      env.intlv.read_ports[r.master].req.addr = r.addr;
      env.intlv.read_ports[r.master].req.total_size = r.total_size;
      env.intlv.read_ports[r.master].req.id = r.id;
    }

    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      env.intlv.read_ports[i].resp.ready = true;
    }

    cycle_inputs(env);
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      if (!issued[i] && ready_snapshot[i]) {
        issued[i] = true;
      }
    }
  }

  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    if (!issued[i]) {
      printf("FAIL: master %d req not accepted\n", i);
      return false;
    }
  }

  if (env.ar_events.size() != axi_interconnect::NUM_READ_MASTERS) {
    printf("FAIL: expected %d AR handshakes, got %zu\n",
           axi_interconnect::NUM_READ_MASTERS, env.ar_events.size());
    return false;
  }

  // Validate AR fields per master
  for (const auto &e : env.ar_events) {
    uint8_t master_id = (e.id >> 2) & 0x3;
    uint8_t orig_id = e.id & 0x3;
    bool found = false;
    for (const auto &r : reqs) {
      if (r.master != master_id)
        continue;
      found = true;
      uint8_t exp_len = calc_burst_len(r.total_size);
      if (e.addr != r.addr || e.len != exp_len || orig_id != (r.id & 0x3)) {
        printf("FAIL: AR mismatch master=%u addr=0x%x len=%u id=%u\n",
               master_id, e.addr, e.len, orig_id);
        return false;
      }
    }
    if (!found) {
      printf("FAIL: unexpected AR master_id=%u\n", master_id);
      return false;
    }
  }

  // Wait for all responses (may arrive out-of-order)
  bool done[axi_interconnect::NUM_READ_MASTERS] = {false, false, false};
  int resp_timeout = sim_ddr::SIM_DDR_LATENCY * 40;
  while (resp_timeout-- > 0) {
    bool all = true;
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      all &= done[i];
    }
    if (all)
      break;

    cycle_outputs(env);

    for (const auto &r : reqs) {
      if (done[r.master])
        continue;
      if (!env.intlv.read_ports[r.master].resp.valid)
        continue;

      uint8_t exp_beats = calc_total_beats(r.total_size);
      if (env.intlv.read_ports[r.master].resp.id != r.id) {
        printf("FAIL: master %u resp.id mismatch exp=%u got=%u\n", r.master,
               r.id, env.intlv.read_ports[r.master].resp.id);
        return false;
      }

      for (int b = 0; b < exp_beats; b++) {
        uint32_t exp = p_memory[(r.addr >> 2) + b];
        if (env.intlv.read_ports[r.master].resp.data[b] != exp) {
          printf("FAIL: master %u beat %d exp=0x%08x got=0x%08x\n", r.master, b,
                 exp, env.intlv.read_ports[r.master].resp.data[b]);
          return false;
        }
      }
      for (int b = exp_beats; b < axi_interconnect::CACHELINE_WORDS; b++) {
        if (env.intlv.read_ports[r.master].resp.data[b] != 0) {
          printf("FAIL: master %u beat %d expected 0 padding\n", r.master, b);
          return false;
        }
      }

      env.intlv.read_ports[r.master].resp.ready = true;
      done[r.master] = true;
    }

    cycle_inputs(env);
  }

  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    if (!done[i]) {
      printf("FAIL: master %d resp timeout\n", i);
      return false;
    }
  }

  printf("PASS\n");
  return true;
}

bool test_write_burst_split_and_backpressure(TestEnv &env) {
  printf("=== Test 3: Write burst splitting + resp backpressure ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  // Case A: 32B full-line write (8 beats)
  uint32_t base_addr = 0x8000;
  axi_interconnect::WideData256_t wdata;
  for (int i = 0; i < axi_interconnect::CACHELINE_WORDS; i++) {
    wdata[i] = 0xA0A00000u | static_cast<uint32_t>(i);
    p_memory[(base_addr >> 2) + i] = 0x0;
  }

  uint32_t wstrb = 0xFFFFFFFFu;
  uint8_t total_size = 31;
  uint8_t req_id = 2;

  bool issued = false;
  int issue_timeout = 200;
  while (!issued && issue_timeout-- > 0) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.write_port.req.ready;

    env.intlv.write_port.req.valid = true;
    env.intlv.write_port.req.addr = base_addr;
    env.intlv.write_port.req.wdata = wdata;
    env.intlv.write_port.req.wstrb = wstrb;
    env.intlv.write_port.req.total_size = total_size;
    env.intlv.write_port.req.id = req_id;

    cycle_inputs(env);
    if (ready_snapshot) {
      issued = true;
    }
  }
  if (!issued) {
    printf("FAIL: write req not accepted\n");
    return false;
  }

  // Wait for write resp.valid, but keep resp.ready low for a few cycles first.
  bool saw_resp = false;
  int resp_timeout = sim_ddr::SIM_DDR_LATENCY * 40;
  while (!saw_resp && resp_timeout-- > 0) {
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

  // Backpressure upstream for 5 cycles; resp.valid must remain asserted.
  for (int i = 0; i < 5; i++) {
    cycle_outputs(env);
    if (!env.intlv.write_port.resp.valid) {
      printf("FAIL: resp.valid dropped under backpressure\n");
      return false;
    }
    if (env.intlv.axi_io.b.bready) {
      printf("FAIL: expected DDR bready=0 while resp pending\n");
      return false;
    }
    if (env.intlv.write_port.req.ready) {
      printf("FAIL: write req.ready should stay low while resp pending\n");
      return false;
    }
    cycle_inputs(env);
  }

  // Now accept the response.
  cycle_outputs(env);
  if (!env.intlv.write_port.resp.valid) {
    printf("FAIL: resp.valid unexpectedly low before accept\n");
    return false;
  }
  if (env.intlv.write_port.resp.id != req_id ||
      env.intlv.write_port.resp.resp != sim_ddr::AXI_RESP_OKAY) {
    printf("FAIL: write resp mismatch id=%u resp=%u\n",
           env.intlv.write_port.resp.id, env.intlv.write_port.resp.resp);
    return false;
  }
  env.intlv.write_port.resp.ready = true;
  cycle_inputs(env);

  // Next cycle: resp.valid should be cleared.
  cycle_outputs(env);
  if (env.intlv.write_port.resp.valid) {
    printf("FAIL: resp.valid not cleared after handshake\n");
    return false;
  }
  cycle_inputs(env);

  // Verify memory contents
  for (int i = 0; i < axi_interconnect::CACHELINE_WORDS; i++) {
    uint32_t got = p_memory[(base_addr >> 2) + i];
    if (got != wdata[i]) {
      printf("FAIL: mem[%d] exp=0x%08x got=0x%08x\n", i, wdata[i], got);
      return false;
    }
  }

  // Validate AW/W splitting
  if (env.aw_events.size() != 1) {
    printf("FAIL: expected 1 AW handshake, got %zu\n", env.aw_events.size());
    return false;
  }
  uint8_t exp_awid =
      static_cast<uint8_t>((axi_interconnect::MASTER_DCACHE_W << 2) |
                           (req_id & 0x3));
  if (env.aw_events[0].addr != base_addr || env.aw_events[0].len != 7 ||
      env.aw_events[0].id != exp_awid) {
    printf("FAIL: AW mismatch addr=0x%x len=%u id=0x%x\n", env.aw_events[0].addr,
           env.aw_events[0].len, env.aw_events[0].id);
    return false;
  }
  if (env.w_events.size() != 8) {
    printf("FAIL: expected 8 W handshakes, got %zu\n", env.w_events.size());
    return false;
  }
  for (int i = 0; i < 8; i++) {
    if (env.w_events[i].data != wdata[i] || env.w_events[i].strb != 0xF ||
        env.w_events[i].last != (i == 7)) {
      printf("FAIL: W[%d] mismatch data=0x%08x strb=0x%x last=%d\n", i,
             env.w_events[i].data, env.w_events[i].strb, env.w_events[i].last);
      return false;
    }
  }

  // Case B: partial strobe single-beat write
  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint32_t paddr = 0x9000;
  p_memory[paddr >> 2] = 0x11223344;
  axi_interconnect::WideData256_t pwdata;
  pwdata.clear();
  pwdata[0] = 0xAABBCCDD;

  issued = false;
  issue_timeout = 200;
  while (!issued && issue_timeout-- > 0) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.write_port.req.ready;

    env.intlv.write_port.req.valid = true;
    env.intlv.write_port.req.addr = paddr;
    env.intlv.write_port.req.wdata = pwdata;
    env.intlv.write_port.req.wstrb = 0x5; // byte0 + byte2
    env.intlv.write_port.req.total_size = 3;
    env.intlv.write_port.req.id = 1;
    env.intlv.write_port.resp.ready = true;

    cycle_inputs(env);
    if (ready_snapshot) {
      issued = true;
    }
  }
  if (!issued) {
    printf("FAIL: partial write req not accepted\n");
    return false;
  }

  // Wait for resp to be consumed
  resp_timeout = sim_ddr::SIM_DDR_LATENCY * 40;
  bool consumed = false;
  while (resp_timeout-- > 0) {
    cycle_outputs(env);
    env.intlv.write_port.resp.ready = true;
    bool done = env.intlv.write_port.resp.valid;
    cycle_inputs(env);
    if (done) {
      consumed = true;
      break;
    }
  }
  if (!consumed) {
    printf("FAIL: partial write resp timeout\n");
    return false;
  }
  uint32_t expected = 0x11BB33DD;
  if (p_memory[paddr >> 2] != expected) {
    printf("FAIL: partial strobe exp=0x%08x got=0x%08x\n", expected,
           p_memory[paddr >> 2]);
    return false;
  }

  printf("PASS\n");
  return true;
}

bool test_read_write_parallel(TestEnv &env) {
  printf("=== Test 4: Read/write parallel ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint32_t r_addr = 0x10000;
  uint32_t w_addr = 0x11000;
  p_memory[r_addr >> 2] = 0xDEADBEEF;
  p_memory[w_addr >> 2] = 0;

  axi_interconnect::WideData256_t wdata;
  wdata.clear();
  wdata[0] = 0xCAFEBABE;

  bool r_issued = false;
  bool w_issued = false;
  int issue_timeout = 300;
  while (issue_timeout-- > 0) {
    if (r_issued && w_issued)
      break;

    cycle_outputs(env);
    bool r_ready_snapshot = env.intlv.read_ports[0].req.ready;
    bool w_ready_snapshot = env.intlv.write_port.req.ready;

    if (!r_issued) {
      env.intlv.read_ports[0].req.valid = true;
      env.intlv.read_ports[0].req.addr = r_addr;
      env.intlv.read_ports[0].req.total_size = 3;
      env.intlv.read_ports[0].req.id = 3;
    }
    if (!w_issued) {
      env.intlv.write_port.req.valid = true;
      env.intlv.write_port.req.addr = w_addr;
      env.intlv.write_port.req.wdata = wdata;
      env.intlv.write_port.req.wstrb = 0xF;
      env.intlv.write_port.req.total_size = 3;
      env.intlv.write_port.req.id = 0;
    }

    env.intlv.read_ports[0].resp.ready = true;
    env.intlv.write_port.resp.ready = true;
    cycle_inputs(env);

    if (!r_issued && r_ready_snapshot)
      r_issued = true;
    if (!w_issued && w_ready_snapshot)
      w_issued = true;
  }
  if (!r_issued || !w_issued) {
    printf("FAIL: parallel issue timeout r=%d w=%d\n", r_issued, w_issued);
    return false;
  }

  bool r_done = false;
  bool w_done = false;
  int timeout = sim_ddr::SIM_DDR_LATENCY * 60;
  while (timeout-- > 0) {
    if (r_done && w_done)
      break;

    cycle_outputs(env);
    if (!r_done && env.intlv.read_ports[0].resp.valid) {
      if (env.intlv.read_ports[0].resp.data[0] != p_memory[r_addr >> 2]) {
        printf("FAIL: read data mismatch\n");
        return false;
      }
      env.intlv.read_ports[0].resp.ready = true;
      r_done = true;
    }
    if (!w_done && env.intlv.write_port.resp.valid) {
      env.intlv.write_port.resp.ready = true;
      w_done = true;
    }
    cycle_inputs(env);
  }
  if (!r_done || !w_done) {
    printf("FAIL: parallel completion timeout r=%d w=%d\n", r_done, w_done);
    return false;
  }

  if (p_memory[w_addr >> 2] != wdata[0]) {
    printf("FAIL: write memory mismatch exp=0x%08x got=0x%08x\n", wdata[0],
           p_memory[w_addr >> 2]);
    return false;
  }

  printf("PASS\n");
  return true;
}

bool test_read_resp_backpressure(TestEnv &env) {
  printf("=== Test 5: Read resp backpressure ===\n");

  env.clear_events();
  env.intlv.init();
  env.ddr.init();

  uint8_t master = axi_interconnect::MASTER_ICACHE;
  uint32_t addr = 0x12000;
  uint8_t total_size = 31;
  uint8_t id = 3;

  uint8_t beats = calc_total_beats(total_size);
  for (int b = 0; b < beats; b++) {
    p_memory[(addr >> 2) + b] = 0xFACE0000u | static_cast<uint32_t>(b);
  }

  // Issue request (ready-first)
  bool issued = false;
  int issue_timeout = 200;
  while (!issued && issue_timeout-- > 0) {
    cycle_outputs(env);
    bool ready_snapshot = env.intlv.read_ports[master].req.ready;

    env.intlv.read_ports[master].req.valid = true;
    env.intlv.read_ports[master].req.addr = addr;
    env.intlv.read_ports[master].req.total_size = total_size;
    env.intlv.read_ports[master].req.id = id;

    cycle_inputs(env);
    if (ready_snapshot) {
      issued = true;
    }
  }
  if (!issued) {
    printf("FAIL: read req not accepted\n");
    return false;
  }

  // Wait for response
  axi_interconnect::WideData256_t resp_data_snapshot;
  resp_data_snapshot.clear();
  bool saw_resp = false;
  int timeout = sim_ddr::SIM_DDR_LATENCY * 40;
  while (!saw_resp && timeout-- > 0) {
    cycle_outputs(env);
    if (env.intlv.read_ports[master].resp.valid) {
      if (env.intlv.read_ports[master].resp.id != id) {
        printf("FAIL: resp.id mismatch exp=%u got=%u\n", id,
               env.intlv.read_ports[master].resp.id);
        return false;
      }
      resp_data_snapshot = env.intlv.read_ports[master].resp.data;
      saw_resp = true;
    }
    cycle_inputs(env);
  }
  if (!saw_resp) {
    printf("FAIL: read resp timeout\n");
    return false;
  }

  // Backpressure for 5 cycles: resp.valid/data must remain stable
  for (int i = 0; i < 5; i++) {
    cycle_outputs(env);
    if (!env.intlv.read_ports[master].resp.valid) {
      printf("FAIL: resp.valid dropped under backpressure\n");
      return false;
    }
    if (env.intlv.read_ports[master].resp.id != id) {
      printf("FAIL: resp.id changed under backpressure\n");
      return false;
    }
    for (int b = 0; b < axi_interconnect::CACHELINE_WORDS; b++) {
      if (env.intlv.read_ports[master].resp.data[b] != resp_data_snapshot[b]) {
        printf("FAIL: resp.data changed under backpressure\n");
        return false;
      }
    }
    cycle_inputs(env);
  }

  // Consume the response
  cycle_outputs(env);
  if (!env.intlv.read_ports[master].resp.valid) {
    printf("FAIL: resp.valid unexpectedly low before consume\n");
    return false;
  }
  env.intlv.read_ports[master].resp.ready = true;
  cycle_inputs(env);

  cycle_outputs(env);
  if (env.intlv.read_ports[master].resp.valid) {
    printf("FAIL: resp.valid not cleared after handshake\n");
    return false;
  }
  cycle_inputs(env);

  printf("PASS\n");
  return true;
}

int main() {
  printf("====================================\n");
  printf("AXI-Interconnect Test Suite\n");
  printf("====================================\n\n");

  p_memory = new uint32_t[TEST_MEM_SIZE_WORDS];
  memset(p_memory, 0, TEST_MEM_SIZE_WORDS * sizeof(uint32_t));

  int passed = 0, failed = 0;

  if (test_ar_latching_ready_first())
    passed++;
  else
    failed++;

  TestEnv env;

  if (test_read_multi_master_var_sizes(env))
    passed++;
  else
    failed++;

  if (test_write_burst_split_and_backpressure(env))
    passed++;
  else
    failed++;

  if (test_read_write_parallel(env))
    passed++;
  else
    failed++;

  if (test_read_resp_backpressure(env))
    passed++;
  else
    failed++;

  printf("\n====================================\n");
  printf("Results: %d passed, %d failed\n", passed, failed);
  printf("====================================\n");

  delete[] p_memory;
  return failed == 0 ? 0 : 1;
}
