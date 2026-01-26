/**
 * @file mmio_router_axi3_test.cpp
 * @brief AXI3 MMIO router + MMIO bus unit tests
 */

#include "AXI_Router_AXI3.h"
#include "MMIO_Bus_AXI3.h"
#include "SimDDR_AXI3.h"
#include <mmio_map.h>
#include <cstdio>
#include <cstring>
#include <vector>

uint32_t *p_memory = nullptr;
long long sim_time = 0;
constexpr uint32_t TEST_MEM_WORDS = 0x100000; // 4MB
constexpr int TIMEOUT = 1000;

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

static uint32_t make_axi_id(uint8_t master_id, uint8_t orig_id,
                            uint8_t offset_bytes, uint8_t total_size) {
  return (orig_id & 0xF) | ((master_id & 0x3) << 4) |
         ((offset_bytes & 0x1F) << 6) | ((total_size & 0x1F) << 11);
}

class DummyDevice : public mmio::MMIO_Device {
public:
  explicit DummyDevice(uint32_t size) : mem(size, 0) {}

  void read(uint32_t addr, uint8_t *data, uint32_t len) override {
    for (uint32_t i = 0; i < len; i++) {
      data[i] = mem[(addr - base) + i];
    }
  }

  void write(uint32_t addr, const uint8_t *data, uint32_t len,
             uint32_t wstrb) override {
    for (uint32_t i = 0; i < len; i++) {
      if ((wstrb >> i) & 1u) {
        mem[(addr - base) + i] = data[i];
      }
    }
  }

  void set_base(uint32_t b) { base = b; }
  uint8_t &at(uint32_t addr) { return mem[(addr - base)]; }

private:
  uint32_t base = 0;
  std::vector<uint8_t> mem;
};

struct TestEnv {
  axi_interconnect::AXI_Router_AXI3 router;
  sim_ddr_axi3::SimDDR_AXI3 ddr;
  mmio::MMIO_Bus_AXI3 mmio;
  sim_ddr_axi3::SimDDR_AXI3_IO_t up; // upstream AXI master
};

static void clear_upstream(TestEnv &env) {
  env.up.ar.arvalid = false;
  env.up.ar.araddr = 0;
  env.up.ar.arid = 0;
  env.up.ar.arlen = 0;
  env.up.ar.arsize = 5;
  env.up.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

  env.up.aw.awvalid = false;
  env.up.aw.awaddr = 0;
  env.up.aw.awid = 0;
  env.up.aw.awlen = 0;
  env.up.aw.awsize = 5;
  env.up.aw.awburst = sim_ddr_axi3::AXI_BURST_INCR;

  env.up.w.wvalid = false;
  env.up.w.wid = 0;
  env.up.w.wdata.clear();
  env.up.w.wstrb = 0;
  env.up.w.wlast = false;

  env.up.r.rready = true;
  env.up.b.bready = true;
}

static void cycle_outputs(TestEnv &env) {
  env.ddr.comb_outputs();
  env.mmio.comb_outputs();
  env.router.comb_outputs(env.up, env.ddr.io, env.mmio.io);
}

static void cycle_inputs(TestEnv &env) {
  env.router.comb_inputs(env.up, env.ddr.io, env.mmio.io);
  env.ddr.comb_inputs();
  env.mmio.comb_inputs();
  env.router.seq(env.up, env.ddr.io, env.mmio.io);
  env.ddr.seq();
  env.mmio.seq();
  sim_time++;
}

static bool test_router_ram_read(TestEnv &env) {
  printf("=== Test 1: Router RAM read ===\n");

  uint32_t addr = 0x2000;
  uint8_t total_size = 3; // 4B
  uint8_t offset = addr & 0x1F;
  uint32_t id = make_axi_id(0, 1, offset, total_size);

  p_memory[addr >> 2] = 0xAABBCCDDu;

  bool issued = false;
  bool done = false;
  bool mmio_saw_ar = false;
  int timeout = TIMEOUT;
  while (timeout-- > 0 && !done) {
    clear_upstream(env);
    cycle_outputs(env);

    env.up.ar.arvalid = true;
    env.up.ar.araddr = addr & ~0x1Fu;
    env.up.ar.arid = id;
    env.up.ar.arlen = 0;
    env.up.ar.arsize = 5;
    env.up.ar.arburst = sim_ddr_axi3::AXI_BURST_INCR;

    if (env.mmio.io.ar.arvalid) {
      mmio_saw_ar = true;
    }

    cycle_inputs(env);

    if (!issued && env.up.ar.arvalid && env.up.ar.arready) {
      issued = true;
    }

    if (env.up.r.rvalid && env.up.r.rready) {
      done = true;
      uint8_t bytes[32] = {0};
      for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
        store_le32(bytes + (w * 4), env.up.r.rdata[w]);
      }
      uint32_t got = load_le32(bytes + offset);
      if (got != 0xAABBCCDDu) {
        printf("FAIL: RAM read mismatch exp=0x%08x got=0x%08x\n", 0xAABBCCDDu,
               got);
        return false;
      }
    }
  }

  if (!issued || !done) {
    printf("FAIL: RAM read timeout\n");
    return false;
  }
  if (mmio_saw_ar) {
    printf("FAIL: MMIO saw AR during RAM read\n");
    return false;
  }
  printf("PASS\n");
  return true;
}

static bool test_router_mmio_read(TestEnv &env, DummyDevice &dev) {
  printf("=== Test 2: Router MMIO read ===\n");

  uint32_t addr = MMIO_RANGE_BASE + 0x10;
  uint8_t total_size = 3; // 4B
  uint8_t offset = addr & 0x1F;
  uint32_t id = make_axi_id(0, 2, offset, total_size);

  dev.at(addr) = 0x12;
  dev.at(addr + 1) = 0x34;
  dev.at(addr + 2) = 0x56;
  dev.at(addr + 3) = 0x78;

  bool issued = false;
  bool done = false;
  bool ddr_saw_ar = false;
  int timeout = TIMEOUT;
  while (timeout-- > 0 && !done) {
    clear_upstream(env);
    cycle_outputs(env);

    env.up.ar.arvalid = true;
    env.up.ar.araddr = addr & ~0x1Fu;
    env.up.ar.arid = id;
    env.up.ar.arlen = 0;
    env.up.ar.arsize = 5;
    env.up.ar.arburst = sim_ddr_axi3::AXI_BURST_FIXED;

    if (env.ddr.io.ar.arvalid) {
      ddr_saw_ar = true;
    }

    cycle_inputs(env);

    if (!issued && env.up.ar.arvalid && env.up.ar.arready) {
      issued = true;
    }

    if (env.up.r.rvalid && env.up.r.rready) {
      done = true;
      uint8_t bytes[32] = {0};
      for (int w = 0; w < sim_ddr_axi3::AXI_DATA_WORDS; w++) {
        store_le32(bytes + (w * 4), env.up.r.rdata[w]);
      }
      uint32_t got = load_le32(bytes + offset);
      if (got != 0x78563412u) {
        printf("FAIL: MMIO read mismatch exp=0x%08x got=0x%08x\n", 0x78563412u,
               got);
        return false;
      }
    }
  }

  if (!issued || !done) {
    printf("FAIL: MMIO read timeout\n");
    return false;
  }
  if (ddr_saw_ar) {
    printf("FAIL: DDR saw AR during MMIO read\n");
    return false;
  }
  printf("PASS\n");
  return true;
}

static bool test_router_mmio_write(TestEnv &env, DummyDevice &dev) {
  printf("=== Test 3: Router MMIO write ===\n");

  uint32_t addr = MMIO_RANGE_BASE + 0x20;
  uint8_t total_size = 3; // 4B
  uint8_t offset = addr & 0x1F;
  uint32_t id = make_axi_id(0, 3, offset, total_size);

  sim_ddr_axi3::Data256_t wdata;
  wdata.clear();
  wdata[0] = 0xDEADBEEFu;

  bool issued = false;
  bool done = false;
  bool ddr_saw_aw = false;
  int timeout = TIMEOUT;
  while (timeout-- > 0 && !done) {
    clear_upstream(env);
    cycle_outputs(env);

    env.up.aw.awvalid = true;
    env.up.aw.awaddr = addr & ~0x1Fu;
    env.up.aw.awid = id;
    env.up.aw.awlen = 0;
    env.up.aw.awsize = 5;
    env.up.aw.awburst = sim_ddr_axi3::AXI_BURST_FIXED;

    env.up.w.wvalid = true;
    env.up.w.wid = id;
    env.up.w.wdata = wdata;
    env.up.w.wstrb = 0xF << offset;
    env.up.w.wlast = true;

    if (env.ddr.io.aw.awvalid) {
      ddr_saw_aw = true;
    }

    cycle_inputs(env);

    if (!issued && env.up.aw.awvalid && env.up.aw.awready) {
      issued = true;
    }

    if (env.up.b.bvalid && env.up.b.bready) {
      done = true;
    }
  }

  if (!issued || !done) {
    printf("FAIL: MMIO write timeout\n");
    return false;
  }
  if (ddr_saw_aw) {
    printf("FAIL: DDR saw AW during MMIO write\n");
    return false;
  }

  uint32_t got = static_cast<uint32_t>(dev.at(addr)) |
                 (static_cast<uint32_t>(dev.at(addr + 1)) << 8) |
                 (static_cast<uint32_t>(dev.at(addr + 2)) << 16) |
                 (static_cast<uint32_t>(dev.at(addr + 3)) << 24);
  if (got != 0xDEADBEEFu) {
    printf("FAIL: MMIO write mismatch exp=0x%08x got=0x%08x\n", 0xDEADBEEFu,
           got);
    return false;
  }

  printf("PASS\n");
  return true;
}

int main() {
  p_memory = new uint32_t[TEST_MEM_WORDS];
  for (uint32_t i = 0; i < TEST_MEM_WORDS; i++) {
    p_memory[i] = 0;
  }

  TestEnv env;
  env.router.init();
  env.ddr.init();
  env.mmio.init();

  DummyDevice dev(MMIO_RANGE_SIZE);
  dev.set_base(MMIO_RANGE_BASE);
  env.mmio.add_device(MMIO_RANGE_BASE, MMIO_RANGE_SIZE, &dev);

  bool ok = true;
  ok &= test_router_ram_read(env);
  env.router.init();
  env.ddr.init();
  env.mmio.init();
  env.mmio.add_device(MMIO_RANGE_BASE, MMIO_RANGE_SIZE, &dev);
  ok &= test_router_mmio_read(env, dev);
  env.router.init();
  env.ddr.init();
  env.mmio.init();
  env.mmio.add_device(MMIO_RANGE_BASE, MMIO_RANGE_SIZE, &dev);
  ok &= test_router_mmio_write(env, dev);

  delete[] p_memory;
  if (!ok) {
    printf("FAIL: mmio_router_axi3_test\n");
    return 1;
  }
  printf("PASS: mmio_router_axi3_test\n");
  return 0;
}
