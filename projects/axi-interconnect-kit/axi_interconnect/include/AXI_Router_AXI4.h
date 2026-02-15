#pragma once
/**
 * @file AXI_Router_AXI4.h
 * @brief Simple AXI4 address router (DDR vs MMIO).
 *
 * Routes one AXI4 master interface to either:
 * - SimDDR (normal memory range)
 * - MMIO bus (MMIO range)
 *
 * The router tracks one active read stream and one active write stream.
 */

#include "SimDDR_IO.h"

namespace axi_interconnect {

class AXI_Router_AXI4 {
public:
  void init();

  // Phase 1: route responses from downstream to upstream
  void comb_outputs(sim_ddr::SimDDR_IO_t &up, const sim_ddr::SimDDR_IO_t &ddr,
                    const sim_ddr::SimDDR_IO_t &mmio);

  // Phase 2: route requests from upstream to downstream
  void comb_inputs(sim_ddr::SimDDR_IO_t &up, sim_ddr::SimDDR_IO_t &ddr,
                   sim_ddr::SimDDR_IO_t &mmio);

  // Sequential update
  void seq(const sim_ddr::SimDDR_IO_t &up, const sim_ddr::SimDDR_IO_t &ddr,
           const sim_ddr::SimDDR_IO_t &mmio);

private:
  bool r_active;
  bool r_to_mmio;

  bool w_active;
  bool w_to_mmio;
};

} // namespace axi_interconnect
