#ifndef ICACHE_TOP_H
#define ICACHE_TOP_H

#include "../../front_IO.h" // For icache_in, icache_out
#include "../include/icache_module.h"
#ifdef USE_ICACHE_V2
#include "../include/icache_module_v2.h"
#endif
#include <array>
#include <memory>

class SimContext; // Forward declaration

// Select which icache module instance to wrap in ICacheTop.
#ifdef USE_ICACHE_V2
using ICacheHW = icache_module_v2_n::ICacheV2;
#else
using ICacheHW = icache_module_n::ICache;
#endif

// Abstract Base Class for ICache Top-level Logic
class ICacheTop {
protected:
  struct icache_in *in = nullptr;
  struct icache_out *out = nullptr;
  SimContext *ctx = nullptr;

  // Local performance counters (deltas for the current cycle)
  uint64_t access_delta = 0;
  uint64_t miss_delta = 0;

  static bool debug_enable;

public:
  static void setDebug(bool enable) { debug_enable = enable; }

  void setIO(struct icache_in *in_ptr, struct icache_out *out_ptr) {
    in = in_ptr;
    out = out_ptr;
  }

  void setContext(SimContext *c) { ctx = c; }

  virtual void comb() = 0;
  virtual void seq() = 0;
  virtual void flush() {}

  void syncPerf();

  // Template method for execution step
  virtual void step() {
    // Reset deltas at start of step? Or rely on syncPerf to clear them at end?
    // Clearing at end is safer if step is called multiple times? No, step is
    // once per cycle. But let's initialize deltas to 0 at constructor, and
    // clear in syncPerf.

    comb();
    if (!in->run_comb_only) {
      seq();
    }
    syncPerf();
  }

  virtual ~ICacheTop() {}
};

// Implementation using the True ICache Module (Detailed Simulation)
class TrueICacheTop : public ICacheTop {
private:
  // Simple non-AXI memory model with multiple outstanding transactions (0..15)
  std::array<uint8_t, 16> mem_valid{};
  std::array<uint32_t, 16> mem_addr{};
  std::array<uint32_t, 16> mem_age{};

  ICacheHW &icache_hw;

  // When CONFIG_MMU is disabled, ICacheTop provides a minimal translation stub
  // using va2pa() (if paging is enabled) or identity mapping. The stub returns
  // translation results one cycle after the request.
  bool mmu_stub_req_valid_r = false;
  uint32_t mmu_stub_req_vtag_r = 0;

public:
  TrueICacheTop(ICacheHW &hw);
  void comb() override;
  void seq() override;
  void flush() override;
};

// Implementation using the Simple ICache Model (Ideal P-Memory Access)
class SimpleICacheTop : public ICacheTop {
public:
  void comb() override;
  void seq() override;
  void flush() override {}
};

#ifdef USE_SIM_DDR
// Implementation using TrueICache + AXI-Interconnect + SimDDR
class SimDDRICacheTop : public ICacheTop {
private:
  ICacheHW &icache_hw;

  bool mmu_stub_req_valid_r = false;
  uint32_t mmu_stub_req_vtag_r = 0;

public:
  SimDDRICacheTop(ICacheHW &hw);
  void comb() override;
  void seq() override;
  void flush() override;
};
#endif

// Factory function to get the singleton instance
ICacheTop *get_icache_instance();

#endif
