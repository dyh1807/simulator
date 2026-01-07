#include "include/ICacheTop.h"
#include "include/icache_module.h"
#include "../front_IO.h"

// Define global ICache instance
ICache icache;

void icache_top(struct icache_in *in, struct icache_out *out) {
  get_icache_instance()->step(in, out);
}
