#ifndef ICACHE_TOP_H
#define ICACHE_TOP_H

#include "../../front_IO.h" // For icache_in, icache_out
#include "../include/icache_module.h"
#include <memory>

// Abstract Base Class for ICache Top-level Logic
class ICacheTop {
protected:
  struct icache_in *in = nullptr;
  struct icache_out *out = nullptr;

public:
  void setIO(struct icache_in *in_ptr, struct icache_out *out_ptr) {
    in = in_ptr;
    out = out_ptr;
  }

  virtual void comb() = 0;
  virtual void seq() = 0;

  // Template method for execution step
  virtual void step() {
    comb();
    if (!in->run_comb_only) {
      seq();
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
  void comb() override;
  void seq() override;
};

// Implementation using the Simple ICache Model (Ideal P-Memory Access)
class SimpleICacheTop : public ICacheTop {
public:
  void comb() override;
  void seq() override;
};

// Factory function to get the singleton instance
ICacheTop *get_icache_instance();

#endif