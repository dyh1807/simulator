#pragma once
/**
 * @file MemorySubsystem.h
 * @brief Memory Subsystem Singleton
 *
 * Wraps AXI_Interconnect + SimDDR for use by ICache/DCache/MMU.
 * Provides single integration point for main simulation loop.
 */

// Build defaults to AXI4 (32-bit). Define USE_SIM_DDR_AXI3 to select AXI3 (256-bit).
#ifndef USE_SIM_DDR_AXI4
#include "AXI_Interconnect_AXI3.h"
#include "AXI_Router_AXI3.h"
#include "MMIO_Bus_AXI3.h"
#include "UART16550_Device.h"
#include "SimDDR_AXI3.h"
#else
#include "AXI_Interconnect.h"
#include "AXI_Router_AXI4.h"
#include "MMIO_Bus_AXI4.h"
#include "UART16550_Device.h"
#include "SimDDR.h"
#endif
#include <config.h>
#include <cstdint>

class MemorySubsystem {
public:
  // Singleton accessor
  static MemorySubsystem &getInstance() {
    static MemorySubsystem instance;
    return instance;
  }

  // Initialize subsystem
  void init() {
    interconnect.init();
    router.init();
    mmio.init();
    mmio.add_device(UART_BASE, 0x1000, &uart0);
    ddr.init();
    clear_inputs();
  }

  // ========================================================================
  // Two-Phase Combinational Logic
  // ========================================================================

  // Phase 1: Output signals for CPU (run BEFORE cpu.cycle())
  // Sets: resp.valid, resp.data, req.ready
  void comb_outputs() {
    // Reset upstream inputs each cycle; masters will drive them in comb.
    clear_inputs();

    // First get DDR/MMIO outputs for interconnect
    ddr.comb_outputs();
    mmio.comb_outputs();

    // Route DDR/MMIO responses to interconnect inputs via router
    router.comb_outputs(interconnect.axi_io, ddr.io, mmio.io);

    // Now run interconnect output phase
    interconnect.comb_outputs();
  }

  // Phase 2: Input signals from CPU (run AFTER cpu.cycle())
  // Processes: req.valid, req.addr, drives AR/AW/W signals to DDR
  void comb_inputs() {
    // Run interconnect input phase (arbiter, write request)
    interconnect.comb_inputs();

    // Route interconnect -> DDR/MMIO inputs via router
    router.comb_inputs(interconnect.axi_io, ddr.io, mmio.io);

    // Run DDR input phase
    ddr.comb_inputs();
    mmio.comb_inputs();
  }

  // Convenience wrapper (for tests that don't need two-phase)
  void comb() {
    comb_outputs();
    comb_inputs();
  }

  // Sequential logic (call in main loop after comb)
  void seq() {
    ddr.seq();
    mmio.seq();
    router.seq(interconnect.axi_io, ddr.io, mmio.io);
    interconnect.seq();
  }

  // Access interconnect ports (for ICache/DCache/MMU)
  axi_interconnect::ReadMasterPort_t &icache_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_ICACHE];
  }

  axi_interconnect::ReadMasterPort_t &dcache_read_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_DCACHE_R];
  }

  axi_interconnect::ReadMasterPort_t &mmu_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_MMU];
  }

  axi_interconnect::WriteMasterPort_t &dcache_write_port() {
    return interconnect.write_ports[axi_interconnect::MASTER_DCACHE_W];
  }

  axi_interconnect::WriteMasterPort_t &extra_write_port() {
    return interconnect.write_ports[axi_interconnect::MASTER_EXTRA_W];
  }

  // Debug print for stall diagnosis
  void debug_print() {
    printf("=== MemorySubsystem Debug ===\n");
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      printf("  read_port[%d]: req_v=%d req_rdy=%d addr=0x%08x resp_v=%d "
             "resp_rdy=%d\n",
             i, interconnect.read_ports[i].req.valid,
             interconnect.read_ports[i].req.ready,
             interconnect.read_ports[i].req.addr,
             interconnect.read_ports[i].resp.valid,
             interconnect.read_ports[i].resp.ready);
    }
    for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; i++) {
      printf("  write_port[%d]: req_v=%d req_rdy=%d addr=0x%08x resp_v=%d "
             "resp_rdy=%d\n",
             i, interconnect.write_ports[i].req.valid,
             interconnect.write_ports[i].req.ready,
             interconnect.write_ports[i].req.addr,
             interconnect.write_ports[i].resp.valid,
             interconnect.write_ports[i].resp.ready);
    }
    printf("  axi_io: arvalid=%d arready=%d rvalid=%d rready=%d\n",
           interconnect.axi_io.ar.arvalid, interconnect.axi_io.ar.arready,
           interconnect.axi_io.r.rvalid, interconnect.axi_io.r.rready);
    interconnect.debug_print();
  }

private:
  MemorySubsystem() = default;
  MemorySubsystem(const MemorySubsystem &) = delete;
  MemorySubsystem &operator=(const MemorySubsystem &) = delete;

#ifdef USE_SIM_DDR_AXI4
  axi_interconnect::AXI_Interconnect interconnect;
  sim_ddr::SimDDR ddr;
  axi_interconnect::AXI_Router_AXI4 router;
  mmio::MMIO_Bus_AXI4 mmio;
  mmio::UART16550_Device uart0{UART_BASE};
#else
  axi_interconnect::AXI_Interconnect_AXI3 interconnect;
  sim_ddr_axi3::SimDDR_AXI3 ddr;
  axi_interconnect::AXI_Router_AXI3 router;
  mmio::MMIO_Bus_AXI3 mmio;
  mmio::UART16550_Device uart0{UART_BASE};
#endif

  void clear_inputs() {
    // Clear all upstream ports
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      interconnect.read_ports[i].req.valid = false;
      interconnect.read_ports[i].req.addr = 0;
      interconnect.read_ports[i].req.total_size = 0;
      interconnect.read_ports[i].req.id = 0;
      interconnect.read_ports[i].resp.ready = false;
    }
    for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; i++) {
      interconnect.write_ports[i].req.valid = false;
      interconnect.write_ports[i].req.addr = 0;
      interconnect.write_ports[i].req.wdata.clear();
      interconnect.write_ports[i].req.wstrb = 0;
      interconnect.write_ports[i].req.total_size = 0;
      interconnect.write_ports[i].req.id = 0;
      interconnect.write_ports[i].resp.ready = false;
    }
  }
};

// Convenience accessor
inline MemorySubsystem &mem_subsystem() {
  return MemorySubsystem::getInstance();
}
