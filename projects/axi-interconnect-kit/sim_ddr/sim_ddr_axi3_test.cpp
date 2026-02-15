/**
 * @file sim_ddr_axi3_test.cpp
 * @brief Standalone test harness for SimDDR AXI3 (256-bit) module
 *
 * Build: cmake .. && make sim_ddr_axi3_test
 * Run: ./sim_ddr_axi3_test
 */

#include "SimDDR_AXI3.h"
#include <cstdio>

// ============================================================================
// Test Memory (standalone, not using main simulator's memory image loader)
// ============================================================================
uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_WORDS = 1024 * 1024; // 4MB
constexpr int HANDSHAKE_TIMEOUT = 50;
constexpr int DATA_TIMEOUT = sim_ddr_axi3::SIM_DDR_AXI3_LATENCY * 8 + 200;

// ============================================================================
// Helpers
// ============================================================================

void sim_cycle(sim_ddr_axi3::SimDDR_AXI3 &ddr) {
  ddr.comb_outputs();
  ddr.comb_inputs();
  ddr.seq();
  sim_time++;
}

void clear_master_signals(sim_ddr_axi3::SimDDR_AXI3 &ddr) {
  // AW
  ddr.io.aw.awvalid = false;
  ddr.io.aw.awready = false;
  ddr.io.aw.awid = 0;
  ddr.io.aw.awaddr = 0;
  ddr.io.aw.awlen = 0;
  ddr.io.aw.awsize = 5; // 32B beats only
  ddr.io.aw.awburst = sim_ddr_axi3::AXI_BURST_INCR;

  // W
  ddr.io.w.wvalid = false;
  ddr.io.w.wready = false;
  ddr.io.w.wid = 0;
  ddr.io.w.wdata.clear();
  ddr.io.w.wstrb = 0;
  ddr.io.w.wlast = false;

  // B
  ddr.io.b.bready = true;

  // AR
  ddr.io.ar.arvalid = false;
  ddr.io.ar.arready = false;
  ddr.io.ar.arid = 0;
  ddr.io.ar.araddr = 0;
  ddr.io.ar.arlen = 0;
  ddr.io.ar.arsize = 5; // 32B beats only
  ddr.io.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

  // R
  ddr.io.r.rready = true;
}

bool test_single_partial_write(sim_ddr_axi3::SimDDR_AXI3 &ddr) {
  printf("=== Test 1: Single-beat partial write ===\n");

  clear_master_signals(ddr);

  uint32_t addr = 0x1000;
  uint32_t init = 0x11223344;
  p_memory[addr >> 2] = init;

  // AW
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = addr;
  ddr.io.aw.awid = 0x11;
  ddr.io.aw.awlen = 0;
  ddr.io.aw.awsize = 5;
  ddr.io.aw.awburst = sim_ddr_axi3::AXI_BURST_INCR;

  int timeout = HANDSHAKE_TIMEOUT;
  while (!ddr.io.aw.awready && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.aw.awready) {
    printf("FAIL: AW handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // complete handshake
  ddr.io.aw.awvalid = false;

  // W (byte0 + byte2 only)
  ddr.io.w.wvalid = true;
  ddr.io.w.wid = 0x11;
  ddr.io.w.wdata.clear();
  ddr.io.w.wdata[0] = 0xAABBCCDD;
  ddr.io.w.wstrb = 0x5;
  ddr.io.w.wlast = true;

  timeout = HANDSHAKE_TIMEOUT;
  while (!ddr.io.w.wready && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.w.wready) {
    printf("FAIL: W handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // complete handshake
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  // Wait for B; apply backpressure before bvalid is asserted
  ddr.io.b.bready = false;
  timeout = DATA_TIMEOUT;
  while (!ddr.io.b.bvalid && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.b.bvalid) {
    printf("FAIL: B response timeout\n");
    return false;
  }
  if (ddr.io.b.bresp != sim_ddr_axi3::AXI_RESP_OKAY) {
    printf("FAIL: B resp not OKAY: %u\n", static_cast<uint32_t>(ddr.io.b.bresp));
    return false;
  }

  for (int i = 0; i < 3; i++) {
    sim_cycle(ddr);
    if (!ddr.io.b.bvalid) {
      printf("FAIL: bvalid dropped under backpressure\n");
      return false;
    }
  }
  ddr.io.b.bready = true;
  sim_cycle(ddr); // consume

  uint32_t expected = 0x11BB33DD;
  if (p_memory[addr >> 2] != expected) {
    printf("FAIL: mem mismatch exp=0x%08x got=0x%08x\n", expected,
           p_memory[addr >> 2]);
    return false;
  }

  printf("PASS\n");
  return true;
}

bool test_single_read_backpressure(sim_ddr_axi3::SimDDR_AXI3 &ddr) {
  printf("=== Test 2: Single-beat read + rready backpressure ===\n");

  clear_master_signals(ddr);

  uint32_t addr = 0x2000;
  for (int i = 0; i < sim_ddr_axi3::AXI_DATA_WORDS; i++) {
    p_memory[(addr >> 2) + i] = 0xA0000000u | (i * 0x111111u);
  }

  ddr.io.ar.arvalid = true;
  ddr.io.ar.araddr = addr;
  ddr.io.ar.arid = 0x22;
  ddr.io.ar.arlen = 0;
  ddr.io.ar.arsize = 5;
  ddr.io.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

  int timeout = HANDSHAKE_TIMEOUT;
  while (!ddr.io.ar.arready && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.ar.arready) {
    printf("FAIL: AR handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // complete handshake
  ddr.io.ar.arvalid = false;

  // Apply backpressure before rvalid is asserted
  ddr.io.r.rready = false;
  timeout = DATA_TIMEOUT;
  while (!ddr.io.r.rvalid && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.r.rvalid) {
    printf("FAIL: R data timeout\n");
    return false;
  }
  if (ddr.io.r.rresp != sim_ddr_axi3::AXI_RESP_OKAY) {
    printf("FAIL: R resp not OKAY: %u\n", static_cast<uint32_t>(ddr.io.r.rresp));
    return false;
  }

  sim_ddr_axi3::Data256_t snapshot = ddr.io.r.rdata;

  for (int i = 0; i < 3; i++) {
    sim_cycle(ddr);
    if (!ddr.io.r.rvalid) {
      printf("FAIL: rvalid dropped under backpressure\n");
      return false;
    }
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      if (ddr.io.r.rdata[w] != snapshot[w]) {
        printf("FAIL: rdata changed under backpressure\n");
        return false;
      }
    }
  }

  ddr.io.r.rready = true;
  sim_cycle(ddr); // consume

  for (int i = 0; i < sim_ddr_axi3::AXI_DATA_WORDS; i++) {
    uint32_t exp = p_memory[(addr >> 2) + i];
    if (snapshot[i] != exp) {
      printf("FAIL: data[%d] exp=0x%08x got=0x%08x\n", i, exp, snapshot[i]);
      return false;
    }
  }

  printf("PASS\n");
  return true;
}

bool test_two_beat_burst(sim_ddr_axi3::SimDDR_AXI3 &ddr) {
  printf("=== Test 3: Two-beat burst read/write ===\n");

  clear_master_signals(ddr);

  uint32_t base = 0x3000;
  // Initialize 2 beats (64B)
  for (int i = 0; i < sim_ddr_axi3::AXI_DATA_WORDS * 2; i++) {
    p_memory[(base >> 2) + i] = 0x33000000u | i;
  }

  // 2-beat read
  ddr.io.ar.arvalid = true;
  ddr.io.ar.araddr = base;
  ddr.io.ar.arid = 0x33;
  ddr.io.ar.arlen = 1;
  ddr.io.ar.arsize = 5;
  ddr.io.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

  int timeout = HANDSHAKE_TIMEOUT;
  while (!ddr.io.ar.arready && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.ar.arready) {
    printf("FAIL: AR(2-beat) handshake timeout\n");
    return false;
  }
  sim_cycle(ddr);
  ddr.io.ar.arvalid = false;

  int got_beats = 0;
  timeout = DATA_TIMEOUT;
  while (got_beats < 2 && timeout-- > 0) {
    sim_cycle(ddr);
    if (!ddr.io.r.rvalid) {
      continue;
    }
    if (ddr.io.r.rlast != (got_beats == 1)) {
      printf("FAIL: rlast mismatch beat=%d rlast=%d\n", got_beats,
             static_cast<int>(ddr.io.r.rlast));
      return false;
    }
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      uint32_t exp = p_memory[(base >> 2) + (got_beats * 8) + w];
      if (ddr.io.r.rdata[w] != exp) {
        printf("FAIL: R beat=%d word=%d exp=0x%08x got=0x%08x\n", got_beats, w,
               exp, ddr.io.r.rdata[w]);
        return false;
      }
    }
    got_beats++;
  }
  if (got_beats != 2) {
    printf("FAIL: R(2-beat) timeout\n");
    return false;
  }

  // 2-beat write (overwrite)
  clear_master_signals(ddr);
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = base;
  ddr.io.aw.awid = 0x44;
  ddr.io.aw.awlen = 1;
  ddr.io.aw.awsize = 5;
  ddr.io.aw.awburst = sim_ddr_axi3::AXI_BURST_INCR;

  timeout = HANDSHAKE_TIMEOUT;
  while (!ddr.io.aw.awready && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.aw.awready) {
    printf("FAIL: AW(2-beat) handshake timeout\n");
    return false;
  }
  sim_cycle(ddr);
  ddr.io.aw.awvalid = false;

  for (int beat = 0; beat < 2; beat++) {
    ddr.io.w.wvalid = true;
    ddr.io.w.wid = 0x44;
    ddr.io.w.wdata.clear();
    for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
      ddr.io.w.wdata[w] = 0x44000000u | (beat * 8 + w);
    }
    ddr.io.w.wstrb = 0xFFFFFFFFu;
    ddr.io.w.wlast = (beat == 1);

    timeout = HANDSHAKE_TIMEOUT;
    while (!ddr.io.w.wready && timeout-- > 0) {
      sim_cycle(ddr);
    }
    if (!ddr.io.w.wready) {
      printf("FAIL: W(2-beat) handshake timeout beat=%d\n", beat);
      return false;
    }
    sim_cycle(ddr);
    ddr.io.w.wvalid = false;
    ddr.io.w.wlast = false;
  }

  timeout = DATA_TIMEOUT;
  while (!ddr.io.b.bvalid && timeout-- > 0) {
    sim_cycle(ddr);
  }
  if (!ddr.io.b.bvalid) {
    printf("FAIL: B(2-beat) timeout\n");
    return false;
  }
  ddr.io.b.bready = true;
  sim_cycle(ddr);

  for (int i = 0; i < sim_ddr_axi3::AXI_DATA_WORDS * 2; i++) {
    uint32_t exp = 0x44000000u | i;
    if (p_memory[(base >> 2) + i] != exp) {
      printf("FAIL: mem[%d] exp=0x%08x got=0x%08x\n", i, exp,
             p_memory[(base >> 2) + i]);
      return false;
    }
  }

  printf("PASS\n");
  return true;
}

int main() {
  p_memory = new uint32_t[TEST_MEM_WORDS];
  for (uint32_t i = 0; i < TEST_MEM_WORDS; i++) {
    p_memory[i] = 0;
  }

  sim_ddr_axi3::SimDDR_AXI3 ddr;
  ddr.init();

  bool ok = true;
  ok &= test_single_partial_write(ddr);
  ddr.init();
  ok &= test_single_read_backpressure(ddr);
  ddr.init();
  ok &= test_two_beat_burst(ddr);

  delete[] p_memory;
  if (!ok) {
    printf("FAIL: sim_ddr_axi3_test\n");
    return 1;
  }
  printf("PASS: sim_ddr_axi3_test\n");
  return 0;
}
