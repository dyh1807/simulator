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

void sim_cycle(axi_interleaving::AXI_Interleaving &intlv,
               sim_ddr::SimDDR &ddr) {
  ddr.io = intlv.axi_io;
  ddr.comb();

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

  intlv.comb();
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

  int passed = 0, failed = 0;
  if (test_simple_read(intlv, ddr))
    passed++;
  else
    failed++;

  printf("\n====================================\n");
  printf("Results: %d passed, %d failed\n", passed, failed);
  printf("====================================\n");

  delete[] p_memory;
  return failed == 0 ? 0 : 1;
}
