#include "../front_IO.h"
#include "PtwMemPort.h"
#include "include/ICacheTop.h"
#include "include/icache_module.h"

// Define global ICache instance
ICache icache;
PtwMemPort *icache_ptw_mem_port = nullptr;
PtwWalkPort *icache_ptw_walk_port = nullptr;
static SimContext *icache_ctx = nullptr;

void icache_top(struct icache_in *in, struct icache_out *out) {
  static PtwMemPort *bound_mem_port = nullptr;
  static PtwWalkPort *bound_walk_port = nullptr;
  static SimContext *bound_ctx = nullptr;
  ICacheTop *instance = get_icache_instance();
  if (bound_mem_port != icache_ptw_mem_port) {
    instance->set_ptw_mem_port(icache_ptw_mem_port);
    bound_mem_port = icache_ptw_mem_port;
  }
  if (bound_walk_port != icache_ptw_walk_port) {
    instance->set_ptw_walk_port(icache_ptw_walk_port);
    bound_walk_port = icache_ptw_walk_port;
  }
  if (bound_ctx != icache_ctx) {
    instance->setContext(icache_ctx);
    bound_ctx = icache_ctx;
  }
  instance->setIO(in, out);
  instance->step();
}

void icache_set_context(SimContext *ctx) {
  icache_ctx = ctx;
}

void icache_set_ptw_mem_port(PtwMemPort *port) {
  ICacheTop *instance = get_icache_instance();
  instance->set_ptw_mem_port(port);
}

void icache_set_ptw_walk_port(PtwWalkPort *port) {
  ICacheTop *instance = get_icache_instance();
  instance->set_ptw_walk_port(port);
}
