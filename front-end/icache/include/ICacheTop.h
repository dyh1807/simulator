#ifndef ICACHE_TOP_H
#define ICACHE_TOP_H

#include "../../front_IO.h" // For icache_in, icache_out

#include <cstdint>

class SimContext; // Forward declaration

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

// Factory function to get the singleton instance
ICacheTop *get_icache_instance();

#endif
