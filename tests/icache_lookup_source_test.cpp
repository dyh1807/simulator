#include "front-end/icache/include/icache_module.h"
#include "front-end/icache/include/icache_module_v2.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>

// Some modules print using global sim_time.
long long sim_time = 0;

static void test_icache_v1_external_lookup_hit() {
  using icache_module_n::ICache;

  ICache ic;
  ic.reset();

  constexpr uint32_t pc = 0x00001234;
  constexpr uint32_t ppn = 0xABCDEu & 0xFFFFFu; // v1 compares ppn[19:0]

  // Cycle 0: issue request, stage1 latches set from external input.
  ic.io.in.pc = pc;
  ic.io.in.ifu_req_valid = true;
  ic.io.in.ifu_resp_ready = true;
  ic.io.in.refetch = false;
  ic.io.in.ppn_valid = false;
  ic.io.in.ppn = 0;
  ic.io.in.page_fault = false;
  ic.io.in.mem_req_ready = false;
  ic.io.in.mem_resp_valid = false;
  std::fill(std::begin(ic.io.in.mem_resp_data), std::end(ic.io.in.mem_resp_data),
            0u);

  // This test exercises the external-fed lookup path; compile with:
  //   -DICACHE_LOOKUP_FROM_INPUT=1
  ic.io.lookup_in.lookup_resp_valid = true;
  for (int way = 0; way < ICACHE_V1_WAYS; ++way) {
    ic.io.lookup_in.lookup_set_tag[way] = 0;
    ic.io.lookup_in.lookup_set_valid[way] = false;
    for (int w = 0; w < ICACHE_LINE_SIZE / 4; ++w) {
      ic.io.lookup_in.lookup_set_data[way][w] = 0;
    }
  }
  ic.io.lookup_in.lookup_set_tag[0] = ppn;
  ic.io.lookup_in.lookup_set_valid[0] = true;
  for (int w = 0; w < ICACHE_LINE_SIZE / 4; ++w) {
    ic.io.lookup_in.lookup_set_data[0][w] =
        0x11110000u + static_cast<uint32_t>(w);
  }

  ic.comb();
  ic.seq();

  // Cycle 1: translation ready, stage2 hits.
  ic.io.in.ifu_req_valid = false;
  ic.io.in.ppn_valid = true;
  ic.io.in.ppn = ppn;

  ic.comb();
  assert(ic.io.out.ifu_resp_valid && "v1 external lookup should hit");
  assert(ic.io.out.rd_data[0] == 0x11110000u);
  ic.seq();
}

static void test_icache_v2_external_lookup_hit() {
  using icache_module_v2_n::ICacheV2;

  ICacheV2 ic;
  ic.reset();

  constexpr uint32_t pc = 0x00005678;
  constexpr uint32_t ppn = 0x12345u;

  // Cycle 0: issue request and latch set view from external input.
  ic.io.in.pc = pc;
  ic.io.in.ifu_req_valid = true;
  ic.io.in.ifu_resp_ready = true;
  ic.io.in.refetch = false;
  ic.io.in.ppn_valid = false;
  ic.io.in.ppn = 0;
  ic.io.in.page_fault = false;
  ic.io.in.mem_req_ready = false;
  ic.io.in.mem_resp_valid = false;
  ic.io.in.mem_resp_id = 0;
  std::fill(std::begin(ic.io.in.mem_resp_data), std::end(ic.io.in.mem_resp_data),
            0u);

  ic.io.in.lookup_from_input = true;
  for (int way = 0; way < ICACHE_V2_WAYS; ++way) {
    ic.io.in.lookup_set_tag[way] = 0;
    ic.io.in.lookup_set_valid[way] = false;
    for (int w = 0; w < ICACHE_LINE_SIZE / 4; ++w) {
      ic.io.in.lookup_set_data[way][w] = 0;
    }
  }
  ic.io.in.lookup_set_tag[0] = ppn;
  ic.io.in.lookup_set_valid[0] = true;
  for (int w = 0; w < ICACHE_LINE_SIZE / 4; ++w) {
    ic.io.in.lookup_set_data[0][w] = 0x22220000u + static_cast<uint32_t>(w);
  }

  ic.comb();
  ic.seq();

  // Cycle 1: translation ready, stage2 hits and responds.
  ic.io.in.ifu_req_valid = false;
  ic.io.in.ppn_valid = true;
  ic.io.in.ppn = ppn;

  ic.comb();
  assert(ic.io.out.ifu_resp_valid && "v2 external lookup should hit");
  assert(ic.io.out.ifu_resp_pc == pc);
  assert(ic.io.out.rd_data[0] == 0x22220000u);
  ic.seq();
}

int main() {
  test_icache_v1_external_lookup_hit();
  test_icache_v2_external_lookup_hit();
  std::cout << "PASS: icache lookup source tests" << std::endl;
  return 0;
}
