#ifndef ICACHE_TOP_H
#define ICACHE_TOP_H

#include "../../front_IO.h" // For icache_in, icache_out
#include "../include/icache_module.h"
#include <memory>

// Abstract Base Class for ICache Top-level Logic
class ICacheTop {
public:
  virtual void comb(struct icache_in *in, struct icache_out *out) = 0;
  virtual void seq(struct icache_in *in) = 0;

  // Template method for execution step
  virtual void step(struct icache_in *in, struct icache_out *out) {
    comb(in, out);
    if (!in->run_comb_only) {
      seq(in);
    }
  }

  virtual ~ICacheTop() {}
};

// Implementation using the True ICache Module (Detailed Simulation)
class TrueICacheTop : public ICacheTop {
private:
  bool mem_busy = false;
  int mem_latency_cnt = 0;
  uint32_t current_vaddr_reg = 0;
  bool valid_reg = false;

  ICache &icache_hw;

public:
  TrueICacheTop(ICache &hw);
  void comb(struct icache_in *in, struct icache_out *out) override;
  void seq(struct icache_in *in) override;
};

// Implementation using the Simple ICache Model (Ideal P-Memory Access)
class SimpleICacheTop : public ICacheTop {
public:
  void comb(struct icache_in *in, struct icache_out *out) override;
  void seq(struct icache_in *in) override;
};

// Factory function to get the singleton instance
ICacheTop *get_icache_instance();

#endif
