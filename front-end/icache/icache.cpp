#include "../front_IO.h"
#include "include/ICacheTop.h"
#include "include/icache_module.h"
#include "include/icache_module_v2.h"
#include <SimCpu.h> // For cpu

extern SimCpu cpu;
static bool flush_pending = false;

// Global icache instances are shared by ICacheTop factory and perf/debug users.
icache_module_n::ICache icache;
icache_module_v2_n::ICacheV2 icache_v2;

void icache_top(struct icache_in *in, struct icache_out *out) {
  ICacheTop *instance = get_icache_instance();

  icache_in merged_in = *in;
  if (flush_pending && !in->run_comb_only) {
    // Consume fence.i flush on the real comb+seq step.
    merged_in.flush = true;
    flush_pending = false;
  }

  instance->setIO(&merged_in, out);
  instance->setContext(&cpu.ctx);
  instance->step();
}

void icache_flush() {
  flush_pending = true;
}
