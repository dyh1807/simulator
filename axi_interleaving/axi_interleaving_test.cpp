/**
 * @file axi_interleaving_test.cpp
 * @brief Test harness for AXI_Interleaving layer
 */

#include "AXI_Interleaving.h"
#include "SimDDR.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_SIZE = 0x100000;

// Clear DDR input signals (master side)
void clear_ddr_inputs(sim_ddr::SimDDR &ddr) {
  ddr.io.ar.arvalid = false;
  ddr.io.ar.araddr = 0;
  ddr.io.ar.arid = 0;
  ddr.io.ar.arlen = 0;
  ddr.io.ar.arsize = 2;
  ddr.io.ar.arburst = sim_ddr::AXI_BURST_INCR;

  ddr.io.aw.awvalid = false;
  ddr.io.aw.awaddr = 0;
  ddr.io.aw.awid = 0;
  ddr.io.aw.awlen = 0;
  ddr.io.aw.awsize = 2;
  ddr.io.aw.awburst = sim_ddr::AXI_BURST_INCR;

  ddr.io.w.wvalid = false;
  ddr.io.w.wdata = 0;
  ddr.io.w.wstrb = 0;
  ddr.io.w.wlast = false;

  ddr.io.r.rready = true;
  ddr.io.b.bready = true;
}

// Clear Upstream Inputs (from masters)
void clear_upstream_inputs(axi_interleaving::AXI_Interleaving &intlv) {
  for (int i = 0; i < axi_interleaving::NUM_READ_MASTERS; i++) {
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

void sim_cycle(axi_interleaving::AXI_Interleaving &intlv,
               sim_ddr::SimDDR &ddr) {
  // Run interleaver comb first to generate AXI master signals
  intlv.comb();

  // Copy master-side signals from interleaver to DDR
  ddr.io.ar.arvalid = intlv.axi_io.ar.arvalid;
  ddr.io.ar.araddr = intlv.axi_io.ar.araddr;
  ddr.io.ar.arid = intlv.axi_io.ar.arid;
  ddr.io.ar.arlen = intlv.axi_io.ar.arlen;
  ddr.io.ar.arsize = intlv.axi_io.ar.arsize;
  ddr.io.ar.arburst = intlv.axi_io.ar.arburst;

  ddr.io.aw.awvalid = intlv.axi_io.aw.awvalid;
  ddr.io.aw.awaddr = intlv.axi_io.aw.awaddr;
  ddr.io.aw.awid = intlv.axi_io.aw.awid;
  ddr.io.aw.awlen = intlv.axi_io.aw.awlen;
  ddr.io.aw.awsize = intlv.axi_io.aw.awsize;
  ddr.io.aw.awburst = intlv.axi_io.aw.awburst;

  ddr.io.w.wvalid = intlv.axi_io.w.wvalid;
  ddr.io.w.wdata = intlv.axi_io.w.wdata;
  ddr.io.w.wstrb = intlv.axi_io.w.wstrb;
  ddr.io.w.wlast = intlv.axi_io.w.wlast;

  ddr.io.r.rready = intlv.axi_io.r.rready;
  ddr.io.b.bready = intlv.axi_io.b.bready;

  // Run DDR comb to generate slave-side signals
  ddr.comb();

  // Copy slave-side signals from DDR to interleaver
  intlv.axi_io.ar.arready = ddr.io.ar.arready;
  intlv.axi_io.r.rvalid = ddr.io.r.rvalid;
  intlv.axi_io.r.rid = ddr.io.r.rid;
  intlv.axi_io.r.rdata = ddr.io.r.rdata;
  intlv.axi_io.r.rlast = ddr.io.r.rlast;
  intlv.axi_io.r.rresp = ddr.io.r.rresp;
  intlv.axi_io.aw.awready = ddr.io.aw.awready;
  intlv.axi_io.w.wready = ddr.io.w.wready;
  intlv.axi_io.b.bvalid = ddr.io.b.bvalid;
  intlv.axi_io.b.bid = ddr.io.b.bid;
  intlv.axi_io.b.bresp = ddr.io.b.bresp;

  // Run seq for both
  ddr.seq();
  intlv.seq();
  sim_time++;
}

// Test 1: Simple 4-byte read
bool test_simple_read(axi_interleaving::AXI_Interleaving &intlv,
                      sim_ddr::SimDDR &ddr) {
  printf("=== Test 1: Simple Read (4B) ===\n");

  uint32_t test_addr = 0x1000;
  uint32_t expected = 0xDEADBEEF;
  p_memory[test_addr >> 2] = expected;

  intlv.read_ports[0].req.valid = true;
  intlv.read_ports[0].req.addr = test_addr;
  intlv.read_ports[0].req.total_size = 3;
  intlv.read_ports[0].req.id = 1;
  intlv.read_ports[0].resp.ready = true;

  int timeout = 10;
  while (!intlv.read_ports[0].req.ready && timeout > 0) {
    sim_cycle(intlv, ddr);
    timeout--;
  }
  sim_cycle(intlv, ddr);
  intlv.read_ports[0].req.valid = false;

  timeout = 250;
  while (!intlv.read_ports[0].resp.valid && timeout > 0) {
    sim_cycle(intlv, ddr);
    timeout--;
  }

  if (timeout == 0) {
    printf("FAIL: Timeout\n");
    return false;
  }

  uint32_t received = intlv.read_ports[0].resp.data[0];
  if (received != expected) {
    printf("FAIL: 0x%08x != 0x%08x\n", received, expected);
    return false;
  }

  sim_cycle(intlv, ddr);
  printf("PASS: Read 0x%08x\n", received);
  return true;
}

// Test 2: AXI Valid Latching - verify valid stays asserted after req_valid
// drops
bool test_valid_latching(axi_interleaving::AXI_Interleaving &intlv,
                         sim_ddr::SimDDR &ddr) {
  printf("=== Test 2: AXI Valid Latching (using mocked AXI slave) ===\n");

  // Reset state
  intlv.init();
  clear_upstream_inputs(intlv);

  // Assert req_valid (Master side)
  intlv.read_ports[0].req.valid = true;
  intlv.read_ports[0].req.addr = 0x2000;
  intlv.read_ports[0].req.total_size = 3;
  intlv.read_ports[0].req.id = 2;

  // Cycle 1: arready=0 (Backpressure)
  intlv.axi_io.ar.arready = false;

  intlv.comb();
  // Expect arvalid to be asserted combinatorially from req
  if (!intlv.axi_io.ar.arvalid) {
    printf("FAIL: arvalid should be asserted from req\n");
    return false;
  }

  // Latch it!
  intlv.seq();

  // Check if req was accepted
  // Note: req.ready is combinatorial based on round-robin.
  // It effectively says "I've seen your request and issued an AR or latched
  // it". Our implementation sets req.ready=true immediately if selected.
  if (!intlv.read_ports[0].req.ready) {
    printf("FAIL: req.ready should be true\n");
    return false;
  }

  // Cycle 2: Deassert req_valid
  intlv.read_ports[0].req.valid = false;

  // Still backpressure
  intlv.axi_io.ar.arready = false;

  intlv.comb();
  // Expect arvalid to be asserted (latched)
  if (!intlv.axi_io.ar.arvalid) {
    printf(
        "FAIL: arvalid dropped when req_valid deasserted! Latching broken.\n");
    return false;
  }
  printf("  PASS: arvalid stayed asserted despite req_valid dropping\n");

  // Cycle 3: Assert arready (Handshake)
  intlv.axi_io.ar.arready = true;
  intlv.comb(); // arvalid still high? Yes.
  intlv.seq();  // Handshake completes, latch clears

  // Cycle 4: Check latched clear
  intlv.axi_io.ar.arready = false;
  intlv.comb();
  if (intlv.axi_io.ar.arvalid) {
    printf("FAIL: arvalid should be low after handshake\n");
    return false;
  }

  printf("PASS: AXI Handshake complete\n");
  return true;
}

int main() {
  printf("====================================\n");
  printf("AXI-Interleaving Test Suite\n");
  printf("====================================\n\n");

  p_memory = new uint32_t[TEST_MEM_SIZE];
  memset(p_memory, 0, TEST_MEM_SIZE * sizeof(uint32_t));

  axi_interleaving::AXI_Interleaving intlv;
  sim_ddr::SimDDR ddr;

  intlv.init();
  ddr.init();
  clear_ddr_inputs(ddr);
  clear_upstream_inputs(intlv);

  int passed = 0, failed = 0;

  if (test_simple_read(intlv, ddr))
    passed++;
  else
    failed++;
  if (test_valid_latching(intlv, ddr))
    passed++;
  else
    failed++;

  printf("\n====================================\n");
  printf("Results: %d passed, %d failed\n", passed, failed);
  printf("====================================\n");

  delete[] p_memory;
  return failed == 0 ? 0 : 1;
}
