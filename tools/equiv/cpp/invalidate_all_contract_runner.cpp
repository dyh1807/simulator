#include <cstdio>
#include <cstdint>

#define private public
#include "AXI_Interconnect.h"
#undef private

uint32_t *p_memory = nullptr;
long long sim_time = 0;

int main() {
  axi_interconnect::AXI_Interconnect interconnect;
  axi_interconnect::AXI_LLCConfig cfg;
  cfg.enable = true;
  cfg.size_bytes = 512;
  cfg.line_bytes = 64;
  cfg.ways = 2;
  cfg.mshr_num = 2;
  interconnect.set_llc_config(cfg);
  interconnect.init();

  interconnect.set_llc_invalidate_all(true);
  if (!interconnect.invalidate_all_requested()) {
    std::printf("FAIL: invalidate_all request was not visible before accept\n");
    return 1;
  }

  interconnect.llc.io.ext_out.mem.invalidate_all_accepted = true;
  interconnect.seq();
  if (interconnect.invalidate_all_requested()) {
    std::printf("FAIL: invalidate_all request remained visible after accept\n");
    return 1;
  }

  interconnect.llc.io.ext_out.mem.invalidate_all_accepted = false;
  interconnect.seq();
  if (interconnect.invalidate_all_requested()) {
    std::printf("FAIL: invalidate_all request re-armed while input stayed high\n");
    return 1;
  }

  interconnect.set_llc_invalidate_all(false);
  if (interconnect.invalidate_all_requested()) {
    std::printf("FAIL: invalidate_all request stayed visible after deassert\n");
    return 1;
  }

  interconnect.set_llc_invalidate_all(true);
  if (!interconnect.invalidate_all_requested()) {
    std::printf("FAIL: invalidate_all request did not re-arm after deassert\n");
    return 1;
  }

  std::printf("PASS\n");
  return 0;
}
