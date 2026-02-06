#include "../front_IO.h"
#include "include/ICacheTop.h"
#include <SimCpu.h> // For cpu

extern SimCpu cpu;
static bool flush_pending = false;

void icache_top(struct icache_in *in, struct icache_out *out) {
  ICacheTop *instance = get_icache_instance();
  if (flush_pending) {
    instance->flush();
    flush_pending = false;
  }
  instance->setIO(in, out);
  instance->setContext(&cpu.ctx);
  instance->step();
}

void icache_flush() {
  flush_pending = true;
}
