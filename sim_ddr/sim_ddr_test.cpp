/**
 * @file sim_ddr_test.cpp
 * @brief Standalone test harness for SimDDR AXI4 module
 *
 * This test program verifies the correctness of the SimDDR AXI4 interface.
 * It can be compiled and run independently of the main simulator.
 *
 * Build: cmake .. && make sim_ddr_test
 * Run: ./sim_ddr_test
 */

#include "SimDDR.h"
#include <cassert>
#include <cstdio>
#include <cstring>

// ============================================================================
// Test Memory (standalone, not using main simulator's p_memory)
// ============================================================================
uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_SIZE = 1024 * 1024; // 1MB for testing

// ============================================================================
// Test Helper Functions
// ============================================================================

// Simulate one clock cycle
void sim_cycle(sim_ddr::SimDDR &ddr) {
  ddr.comb();
  ddr.seq();
  sim_time++;
}

// Simulate multiple cycles
void sim_cycles(sim_ddr::SimDDR &ddr, int n) {
  for (int i = 0; i < n; i++) {
    sim_cycle(ddr);
  }
}

// Clear all master signals
void clear_master_signals(sim_ddr::SimDDR &ddr) {
  // AW channel (master -> slave)
  ddr.io.aw.awvalid = false;
  ddr.io.aw.awid = 0;
  ddr.io.aw.awaddr = 0;
  ddr.io.aw.awlen = 0;
  ddr.io.aw.awsize = 2; // 4 bytes (32-bit)
  ddr.io.aw.awburst = sim_ddr::AXI_BURST_INCR;

  // W channel (master -> slave)
  ddr.io.w.wvalid = false;
  ddr.io.w.wdata = 0;
  ddr.io.w.wstrb = 0xF; // All bytes enabled
  ddr.io.w.wlast = false;

  // B channel (slave -> master, master provides ready)
  ddr.io.b.bready = true;

  // AR channel (master -> slave)
  ddr.io.ar.arvalid = false;
  ddr.io.ar.arid = 0;
  ddr.io.ar.araddr = 0;
  ddr.io.ar.arlen = 0;
  ddr.io.ar.arsize = 2; // 4 bytes
  ddr.io.ar.arburst = sim_ddr::AXI_BURST_INCR;

  // R channel (slave -> master, master provides ready)
  ddr.io.r.rready = true;
}

// ============================================================================
// Test Cases
// ============================================================================

// Test 1: Single word write
bool test_single_write(sim_ddr::SimDDR &ddr) {
  printf("=== Test 1: Single Word Write ===\n");

  uint32_t test_addr = 0x1000;
  uint32_t test_data = 0xDEADBEEF;

  clear_master_signals(ddr);

  // Phase 1: Issue AW (write address)
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = test_addr;
  ddr.io.aw.awlen = 0;  // Single beat (len + 1 = 1)
  ddr.io.aw.awsize = 2; // 4 bytes

  // Wait for awready
  int timeout = 10;
  while (!ddr.io.aw.awready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: AW handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // Complete handshake
  ddr.io.aw.awvalid = false;

  // Phase 2: Issue W (write data)
  ddr.io.w.wvalid = true;
  ddr.io.w.wdata = test_data;
  ddr.io.w.wstrb = 0xF;
  ddr.io.w.wlast = true; // Single beat, so last

  // Wait for wready
  timeout = 10;
  while (!ddr.io.w.wready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: W handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // Complete handshake
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  // Phase 3: Wait for B (write response)
  ddr.io.b.bready = true;
  timeout = 200;
  while (!ddr.io.b.bvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: B response timeout\n");
    return false;
  }

  if (ddr.io.b.bresp != sim_ddr::AXI_RESP_OKAY) {
    printf("FAIL: Write response not OKAY: %d\n", ddr.io.b.bresp);
    return false;
  }
  sim_cycle(ddr); // Complete handshake

  // Verify memory content
  uint32_t word_addr = test_addr >> 2;
  if (p_memory[word_addr] != test_data) {
    printf("FAIL: Memory content mismatch: expected 0x%08x, got 0x%08x\n",
           test_data, p_memory[word_addr]);
    return false;
  }

  printf("PASS: Single write test completed. mem[0x%x] = 0x%08x\n", test_addr,
         p_memory[word_addr]);
  return true;
}

// Test 2: Single word read
bool test_single_read(sim_ddr::SimDDR &ddr) {
  printf("=== Test 2: Single Word Read ===\n");

  uint32_t test_addr = 0x2000;
  uint32_t expected_data = 0xCAFEBABE;

  // Pre-initialize memory
  p_memory[test_addr >> 2] = expected_data;

  clear_master_signals(ddr);

  // Phase 1: Issue AR (read address)
  ddr.io.ar.arvalid = true;
  ddr.io.ar.araddr = test_addr;
  ddr.io.ar.arlen = 0;  // Single beat
  ddr.io.ar.arsize = 2; // 4 bytes

  // Wait for arready
  int timeout = 10;
  while (!ddr.io.ar.arready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: AR handshake timeout\n");
    return false;
  }
  sim_cycle(ddr); // Complete handshake
  ddr.io.ar.arvalid = false;

  // Phase 2: Wait for R (read data)
  ddr.io.r.rready = true;
  timeout = 200;
  while (!ddr.io.r.rvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: R data timeout\n");
    return false;
  }

  if (ddr.io.r.rdata != expected_data) {
    printf("FAIL: Read data mismatch: expected 0x%08x, got 0x%08x\n",
           expected_data, ddr.io.r.rdata);
    return false;
  }

  if (!ddr.io.r.rlast) {
    printf("FAIL: Expected rlast to be set for single beat\n");
    return false;
  }

  if (ddr.io.r.rresp != sim_ddr::AXI_RESP_OKAY) {
    printf("FAIL: Read response not OKAY: %d\n", ddr.io.r.rresp);
    return false;
  }

  sim_cycle(ddr); // Complete handshake

  printf("PASS: Single read test completed. read(0x%x) = 0x%08x\n", test_addr,
         expected_data);
  return true;
}

// Test 3: Burst write (4 beats)
bool test_burst_write(sim_ddr::SimDDR &ddr) {
  printf("=== Test 3: Burst Write (4 beats) ===\n");

  uint32_t base_addr = 0x3000;
  uint32_t test_data[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

  clear_master_signals(ddr);

  // Phase 1: Issue AW
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = base_addr;
  ddr.io.aw.awlen = 3;  // 4 beats (len + 1)
  ddr.io.aw.awsize = 2; // 4 bytes

  // Wait for awready
  int timeout = 10;
  while (!ddr.io.aw.awready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);
  ddr.io.aw.awvalid = false;

  // Phase 2: Send 4 W beats
  for (int i = 0; i < 4; i++) {
    ddr.io.w.wvalid = true;
    ddr.io.w.wdata = test_data[i];
    ddr.io.w.wstrb = 0xF;
    ddr.io.w.wlast = (i == 3);

    // Call comb first to update outputs (including wready)
    ddr.comb();

    // Wait for wready if not already set
    timeout = 10;
    while (!ddr.io.w.wready && timeout > 0) {
      // Slave not ready, complete this cycle and try again
      ddr.seq();
      sim_time++;
      ddr.comb();
      timeout--;
    }

    // Now wvalid && wready - complete the handshake cycle
    ddr.seq();
    sim_time++;
  }
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  // Phase 3: Wait for B
  timeout = 200;
  while (!ddr.io.b.bvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  if (timeout == 0) {
    printf("FAIL: B response timeout\n");
    return false;
  }
  sim_cycle(ddr);

  // Verify memory
  for (int i = 0; i < 4; i++) {
    uint32_t word_addr = (base_addr >> 2) + i;
    if (p_memory[word_addr] != test_data[i]) {
      printf("FAIL: Memory[%d] mismatch: expected 0x%08x, got 0x%08x\n", i,
             test_data[i], p_memory[word_addr]);
      return false;
    }
  }

  printf("PASS: Burst write test completed\n");
  return true;
}

// Test 4: Burst read (4 beats)
bool test_burst_read(sim_ddr::SimDDR &ddr) {
  printf("=== Test 4: Burst Read (4 beats) ===\n");

  uint32_t base_addr = 0x4000;
  uint32_t expected_data[4] = {0xAAAA0001, 0xAAAA0002, 0xAAAA0003, 0xAAAA0004};

  // Pre-initialize memory
  for (int i = 0; i < 4; i++) {
    p_memory[(base_addr >> 2) + i] = expected_data[i];
  }

  clear_master_signals(ddr);

  // Phase 1: Issue AR
  ddr.io.ar.arvalid = true;
  ddr.io.ar.araddr = base_addr;
  ddr.io.ar.arlen = 3; // 4 beats
  ddr.io.ar.arsize = 2;

  int timeout = 10;
  while (!ddr.io.ar.arready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);
  ddr.io.ar.arvalid = false;

  // Phase 2: Receive 4 R beats
  for (int i = 0; i < 4; i++) {
    timeout = 200;
    while (!ddr.io.r.rvalid && timeout > 0) {
      sim_cycle(ddr);
      timeout--;
    }
    if (timeout == 0) {
      printf("FAIL: R data timeout at beat %d\n", i);
      return false;
    }

    if (ddr.io.r.rdata != expected_data[i]) {
      printf("FAIL: Read data[%d] mismatch: expected 0x%08x, got 0x%08x\n", i,
             expected_data[i], ddr.io.r.rdata);
      return false;
    }

    bool expected_last = (i == 3);
    if (ddr.io.r.rlast != expected_last) {
      printf("FAIL: rlast mismatch at beat %d: expected %d, got %d\n", i,
             expected_last, ddr.io.r.rlast);
      return false;
    }

    sim_cycle(ddr);
  }

  printf("PASS: Burst read test completed\n");
  return true;
}

// Test 5: Write with partial strobe
bool test_partial_strobe(sim_ddr::SimDDR &ddr) {
  printf("=== Test 5: Partial Write Strobe ===\n");

  uint32_t test_addr = 0x5000;

  // Initialize memory to known value
  p_memory[test_addr >> 2] = 0x12345678;

  clear_master_signals(ddr);

  // Write only upper 2 bytes (0xABCD -> position 2,3)
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = test_addr;
  ddr.io.aw.awlen = 0;

  int timeout = 10;
  while (!ddr.io.aw.awready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);
  ddr.io.aw.awvalid = false;

  ddr.io.w.wvalid = true;
  ddr.io.w.wdata = 0xABCD0000;
  ddr.io.w.wstrb = 0xC; // Only bytes 2,3 (upper half-word)
  ddr.io.w.wlast = true;

  timeout = 10;
  while (!ddr.io.w.wready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  // Wait for B
  timeout = 200;
  while (!ddr.io.b.bvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);

  // Expected: 0x12345678 & 0x0000FFFF | 0xABCD0000 = 0xABCD5678
  uint32_t expected = 0xABCD5678;
  if (p_memory[test_addr >> 2] != expected) {
    printf("FAIL: Partial strobe mismatch: expected 0x%08x, got 0x%08x\n",
           expected, p_memory[test_addr >> 2]);
    return false;
  }

  printf("PASS: Partial strobe test completed. mem[0x%x] = 0x%08x\n", test_addr,
         p_memory[test_addr >> 2]);
  return true;
}

// Test 6: Back-to-back transactions
bool test_back_to_back(sim_ddr::SimDDR &ddr) {
  printf("=== Test 6: Back-to-back Transactions ===\n");

  uint32_t addr1 = 0x6000;
  uint32_t addr2 = 0x6010;
  uint32_t data1 = 0x11110000;
  uint32_t data2 = 0x22220000;

  clear_master_signals(ddr);

  // Transaction 1: Write
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = addr1;
  ddr.io.aw.awlen = 0;

  int timeout = 10;
  while (!ddr.io.aw.awready && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }
  sim_cycle(ddr);
  ddr.io.aw.awvalid = false;

  ddr.io.w.wvalid = true;
  ddr.io.w.wdata = data1;
  ddr.io.w.wlast = true;

  while (!ddr.io.w.wready)
    sim_cycle(ddr);
  sim_cycle(ddr);
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  while (!ddr.io.b.bvalid)
    sim_cycle(ddr);
  sim_cycle(ddr);

  // Transaction 2: Write (immediately after)
  ddr.io.aw.awvalid = true;
  ddr.io.aw.awaddr = addr2;
  ddr.io.aw.awlen = 0;

  while (!ddr.io.aw.awready)
    sim_cycle(ddr);
  sim_cycle(ddr);
  ddr.io.aw.awvalid = false;

  ddr.io.w.wvalid = true;
  ddr.io.w.wdata = data2;
  ddr.io.w.wlast = true;

  while (!ddr.io.w.wready)
    sim_cycle(ddr);
  sim_cycle(ddr);
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  while (!ddr.io.b.bvalid)
    sim_cycle(ddr);
  sim_cycle(ddr);

  // Verify both writes
  if (p_memory[addr1 >> 2] != data1) {
    printf("FAIL: First write mismatch\n");
    return false;
  }
  if (p_memory[addr2 >> 2] != data2) {
    printf("FAIL: Second write mismatch\n");
    return false;
  }

  printf("PASS: Back-to-back test completed\n");
  return true;
}

// Test 7: ID signal verification
bool test_id_signals(sim_ddr::SimDDR &ddr) {
  printf("=== Test 7: ID Signal Verification ===\n");

  clear_master_signals(ddr);

  // Test write with specific ID
  uint8_t test_awid = 0x5;
  uint32_t test_addr = 0x7000;
  uint32_t test_data = 0xABCD1234;

  ddr.io.aw.awvalid = true;
  ddr.io.aw.awid = test_awid;
  ddr.io.aw.awaddr = test_addr;
  ddr.io.aw.awlen = 0;

  ddr.comb();
  int timeout = 10;
  while (!ddr.io.aw.awready && timeout > 0) {
    ddr.seq();
    sim_time++;
    ddr.comb();
    timeout--;
  }
  ddr.seq();
  sim_time++;
  ddr.io.aw.awvalid = false;

  // Send W data
  ddr.io.w.wvalid = true;
  ddr.io.w.wdata = test_data;
  ddr.io.w.wlast = true;

  ddr.comb();
  timeout = 10;
  while (!ddr.io.w.wready && timeout > 0) {
    ddr.seq();
    sim_time++;
    ddr.comb();
    timeout--;
  }
  ddr.seq();
  sim_time++;
  ddr.io.w.wvalid = false;
  ddr.io.w.wlast = false;

  // Wait for B response and check bid
  timeout = 200;
  while (!ddr.io.b.bvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }

  if (ddr.io.b.bid != test_awid) {
    printf("FAIL: Write bid mismatch: expected 0x%x, got 0x%x\n", test_awid,
           ddr.io.b.bid);
    return false;
  }
  sim_cycle(ddr);

  // Test read with specific ID
  uint8_t test_arid = 0xA;
  p_memory[test_addr >> 2] = test_data;

  ddr.io.ar.arvalid = true;
  ddr.io.ar.arid = test_arid;
  ddr.io.ar.araddr = test_addr;
  ddr.io.ar.arlen = 0;

  ddr.comb();
  timeout = 10;
  while (!ddr.io.ar.arready && timeout > 0) {
    ddr.seq();
    sim_time++;
    ddr.comb();
    timeout--;
  }
  ddr.seq();
  sim_time++;
  ddr.io.ar.arvalid = false;

  // Wait for R data and check rid
  timeout = 200;
  while (!ddr.io.r.rvalid && timeout > 0) {
    sim_cycle(ddr);
    timeout--;
  }

  if (ddr.io.r.rid != test_arid) {
    printf("FAIL: Read rid mismatch: expected 0x%x, got 0x%x\n", test_arid,
           ddr.io.r.rid);
    return false;
  }
  sim_cycle(ddr);

  printf("PASS: ID signal test completed. bid=0x%x, rid=0x%x\n", test_awid,
         test_arid);
  return true;
}

// Test 8: Stress test - multiple sequential transactions
bool test_stress_sequential(sim_ddr::SimDDR &ddr) {
  printf("=== Test 8: Stress Test (100 sequential transactions) ===\n");

  clear_master_signals(ddr);

  const int NUM_TRANSACTIONS = 100;
  uint32_t base_addr = 0x10000;
  int timeout;

  // Phase 1: Write 100 values
  for (int i = 0; i < NUM_TRANSACTIONS; i++) {
    uint32_t addr = base_addr + (i * 4);
    uint32_t data = 0xDEAD0000 | i;
    uint8_t id = i & 0xF;

    // AW phase
    ddr.io.aw.awvalid = true;
    ddr.io.aw.awid = id;
    ddr.io.aw.awaddr = addr;
    ddr.io.aw.awlen = 0;

    ddr.comb();
    timeout = 10;
    while (!ddr.io.aw.awready && timeout > 0) {
      ddr.seq();
      sim_time++;
      ddr.comb();
      timeout--;
    }
    ddr.seq();
    sim_time++;
    ddr.io.aw.awvalid = false;

    // W phase
    ddr.io.w.wvalid = true;
    ddr.io.w.wdata = data;
    ddr.io.w.wlast = true;

    ddr.comb();
    timeout = 10;
    while (!ddr.io.w.wready && timeout > 0) {
      ddr.seq();
      sim_time++;
      ddr.comb();
      timeout--;
    }
    ddr.seq();
    sim_time++;
    ddr.io.w.wvalid = false;
    ddr.io.w.wlast = false;

    // B phase
    timeout = 200;
    while (!ddr.io.b.bvalid && timeout > 0) {
      sim_cycle(ddr);
      timeout--;
    }
    if (timeout == 0) {
      printf("FAIL: Write %d B response timeout\n", i);
      return false;
    }
    if (ddr.io.b.bid != id) {
      printf("FAIL: Write %d bid mismatch\n", i);
      return false;
    }
    sim_cycle(ddr);
  }

  // Phase 2: Read back and verify all 100 values
  for (int i = 0; i < NUM_TRANSACTIONS; i++) {
    uint32_t addr = base_addr + (i * 4);
    uint32_t expected_data = 0xDEAD0000 | i;
    uint8_t id = (i + 7) & 0xF; // Different ID pattern

    // AR phase
    ddr.io.ar.arvalid = true;
    ddr.io.ar.arid = id;
    ddr.io.ar.araddr = addr;
    ddr.io.ar.arlen = 0;

    ddr.comb();
    timeout = 10;
    while (!ddr.io.ar.arready && timeout > 0) {
      ddr.seq();
      sim_time++;
      ddr.comb();
      timeout--;
    }
    ddr.seq();
    sim_time++;
    ddr.io.ar.arvalid = false;

    // R phase
    timeout = 200;
    while (!ddr.io.r.rvalid && timeout > 0) {
      sim_cycle(ddr);
      timeout--;
    }
    if (timeout == 0) {
      printf("FAIL: Read %d R data timeout\n", i);
      return false;
    }
    if (ddr.io.r.rdata != expected_data) {
      printf("FAIL: Read %d data mismatch: expected 0x%08x, got 0x%08x\n", i,
             expected_data, ddr.io.r.rdata);
      return false;
    }
    if (ddr.io.r.rid != id) {
      printf("FAIL: Read %d rid mismatch: expected 0x%x, got 0x%x\n", i, id,
             ddr.io.r.rid);
      return false;
    }
    sim_cycle(ddr);
  }

  printf("PASS: Stress test completed (100 writes + 100 reads)\n");
  return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
  printf("====================================\n");
  printf("SimDDR AXI4 Module Test Suite\n");
  printf("====================================\n\n");

  // Allocate test memory
  p_memory = new uint32_t[TEST_MEM_SIZE];
  memset(p_memory, 0, TEST_MEM_SIZE * sizeof(uint32_t));

  // Create and initialize SimDDR
  sim_ddr::SimDDR ddr;
  ddr.init();

  int passed = 0;
  int failed = 0;

  // Run tests
  if (test_single_write(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5); // Gap between tests

  if (test_single_read(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_burst_write(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_burst_read(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_partial_strobe(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_back_to_back(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_id_signals(ddr))
    passed++;
  else
    failed++;
  sim_cycles(ddr, 5);

  if (test_stress_sequential(ddr))
    passed++;
  else
    failed++;

  // Summary
  printf("\n====================================\n");
  printf("Test Results: %d passed, %d failed\n", passed, failed);
  printf("====================================\n");

  // Cleanup
  delete[] p_memory;

  return failed == 0 ? 0 : 1;
}
