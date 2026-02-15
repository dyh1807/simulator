#pragma once
/**
 * @file AXI_Router_AXI3.h
 * @brief Simple AXI3 address router (DDR vs MMIO) for 256-bit bus.
 *
 * Routes a single AXI3 master interface to either:
 * - SimDDR (INCR, aligned 32B beats)
 * - MMIO bus (FIXED, single beat)
 *
 * Assumes no read or write interleaving (single outstanding per channel).
 */

#include "SimDDR_AXI3_IO.h"

namespace axi_interconnect {

class AXI_Router_AXI3 {
public:
  void init();

  // Phase 1: route responses from downstream to upstream
  void comb_outputs(sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
                    const sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
                    const sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio);

  // Phase 2: route requests from upstream to downstream
  void comb_inputs(sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
                   sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
                   sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio);

  // Sequential update
  void seq(const sim_ddr_axi3::SimDDR_AXI3_IO_t &up,
           const sim_ddr_axi3::SimDDR_AXI3_IO_t &ddr,
           const sim_ddr_axi3::SimDDR_AXI3_IO_t &mmio);

private:
  bool r_active;
  bool r_to_mmio;

  bool w_active;
  bool w_to_mmio;
};

} // namespace axi_interconnect
