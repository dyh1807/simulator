#include "../front_IO.h"
#include "include/ICacheTop.h"
#include "include/icache_module.h"

// Define global ICache instance
ICache icache;

void icache_top(struct icache_in *in, struct icache_out *out) {
  ICacheTop *instance = get_icache_instance();
  instance->setIO(in, out);
  instance->step();
}