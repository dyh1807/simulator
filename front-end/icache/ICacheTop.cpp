#include "include/ICacheTop.h"
#include "../front_module.h"
#include "../frontend.h"
#include "RISCV.h"
#include "TOP.h"
#include "config.h" // For SimContext
#include "cvt.h"
#include "include/icache_module.h"
#include "mmu_io.h"
#include <MMU.h>
#include <SimCpu.h>
#include <cstdio>
#include <iostream>

// External dependencies
extern SimCpu cpu;
extern uint32_t *p_memory;
extern icache_module_n::ICache icache; // Defined in icache.cpp

// Initialize static member
bool ICacheTop::debug_enable = false;

// Forward declaration if not available in headers
bool va2pa(uint32_t &p_addr, uint32_t v_addr, uint32_t satp, uint32_t type,
           bool *mstatus, bool *sstatus, int privilege, uint32_t *p_memory);

// --- ICacheTop Implementation ---

void ICacheTop::syncPerf() {
  if (ctx) {
    ctx->perf.icache_access_num += access_delta;
    ctx->perf.icache_miss_num += miss_delta;
  }
  // Reset deltas
  access_delta = 0;
  miss_delta = 0;
}

// --- TrueICacheTop Implementation ---

TrueICacheTop::TrueICacheTop(icache_module_n::ICache &hw) : icache_hw(hw) {}

void TrueICacheTop::flush() {
  icache_hw.invalidate_all();
}

void TrueICacheTop::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache] reset\n");
    icache_hw.reset();
    out->icache_read_ready = true;
    mem_busy = false;
    mem_latency_cnt = 0;
    valid_reg = false;
    return;
  }

  // Pass refetch signal to HW
  icache_hw.io.in.refetch = in->refetch;

  // Note: We DO NOT reset mem_busy or mem_latency_cnt here on refetch.
  // The HW will handle DRAIN state.

  // set input for 1st pipeline stage (IFU)
  icache_hw.io.in.pc = in->fetch_address;
  icache_hw.io.in.ifu_req_valid = in->icache_read_valid;

  // set input for 2nd pipeline stage (IFU)
  icache_hw.io.in.ifu_resp_ready = true;

  // get ifu_resp from mmu (calculate last cycle)
  mmu_resp_master_t mmu_resp = cpu.mmu.io.out.mmu_ifu_resp;

  // set input for 2nd pipeline stage (MMU)
  icache_hw.io.in.ppn = mmu_resp.ptag;
  icache_hw.io.in.ppn_valid = mmu_resp.valid && !in->refetch && !mmu_resp.miss;
  icache_hw.io.in.page_fault = mmu_resp.excp;

  // set input for 2nd pipeline stage (Memory)
  if (mem_busy) {
    if (mem_latency_cnt >= ICACHE_MISS_LATENCY) {
      icache_hw.io.in.mem_resp_valid = true;
    } else {
      icache_hw.io.in.mem_resp_valid = false;
    }
    bool mem_resp_valid = icache_hw.io.in.mem_resp_valid;
    bool mem_resp_ready = icache_hw.io.out.mem_resp_ready;
    if (mem_resp_valid) {
      uint32_t mask = ~(ICACHE_LINE_SIZE - 1);
      uint32_t cacheline_base_addr = icache_hw.io.out.mem_req_addr & mask;
      for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
        icache_hw.io.in.mem_resp_data[i] =
            p_memory[cacheline_base_addr / 4 + i];
      }
    }
  } else {
    icache_hw.io.in.mem_req_ready = true;
    icache_hw.io.in.mem_resp_valid = false;
  }

  icache_hw.comb();

  // set input for request to mmu
  cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
  cpu.mmu.io.in.mmu_ifu_resp.ready = true;
  if (icache_hw.io.out.ifu_req_ready && icache_hw.io.in.ifu_req_valid) {
    cpu.mmu.io.in.mmu_ifu_req.valid =
        icache_hw.io.out.ifu_req_ready && in->icache_read_valid;
    cpu.mmu.io.in.mmu_ifu_req.vtag = in->fetch_address >> 12;
  } else if (!icache_hw.io.out.ifu_req_ready) {
    // Replay request using latched address (current_vaddr_reg)
    // But ONLY if we have a valid request pending (valid_reg)
    if (valid_reg) {
      cpu.mmu.io.in.mmu_ifu_req.valid = true;
      cpu.mmu.io.in.mmu_ifu_req.vtag = current_vaddr_reg >> 12;
    } else {
      cpu.mmu.io.in.mmu_ifu_req.valid = false;
    }
  } else {
    cpu.mmu.io.in.mmu_ifu_req.valid = false;
  }
  if (in->run_comb_only) {
    out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
    return;
  }

  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;
  bool miss = icache_hw.io.out.miss;
  if (ifu_resp_valid && ifu_resp_ready) {
    out->icache_read_complete = true;
    if (miss) {
      std::cout << "[icache_top] WARNING: miss is true when ifu_resp is valid"
                << std::endl;
      std::cout << "[icache_top] sim_time: " << std::dec << sim_time
                << std::endl;
      exit(1);
    }
    // Use current_vaddr_reg for response PC?
    // Yes, because response matches the request we latched.
    out->fetch_pc = current_vaddr_reg;

    // DEBUG: Print fetched instructions
    if (debug_enable) { // Optional: limit log volume
      std::cout << "[ICacheTop] Complete PC: " << std::hex << current_vaddr_reg
                << std::dec << " Data: ";
    }

    uint32_t mask = ICACHE_LINE_SIZE - 1;
    int base_idx = (current_vaddr_reg & mask) / 4;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      if (base_idx + i >= ICACHE_LINE_SIZE / 4) {
        out->fetch_group[i] = INST_NOP;
        out->page_fault_inst[i] = false;
        out->inst_valid[i] = false;
        continue;
      }
      out->fetch_group[i] = icache_hw.io.out.ifu_page_fault
                                ? INST_NOP
                                : icache_hw.io.out.rd_data[i + base_idx];

      if (debug_enable)
        std::cout << std::hex << out->fetch_group[i] << " ";

      out->page_fault_inst[i] = icache_hw.io.out.ifu_page_fault;
      out->inst_valid[i] = true;
    }
    if (debug_enable)
      std::cout << std::dec << std::endl;
  } else {
    out->icache_read_complete = false;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
  }

  out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
}

void TrueICacheTop::seq() {
  if (in->reset)
    return;

  // If refetch, clear valid_reg?
  if (in->refetch) {
    valid_reg = false;
    // Do NOT clear mem_busy here.
  }

  icache_hw.seq();

  if (mem_busy) {
    mem_latency_cnt++;
  }
  bool mem_req_ready =
      !mem_busy; // Simplified model: memory is ready if not busy
  bool mem_req_valid = icache_hw.io.out.mem_req_valid;
  if (mem_req_ready && mem_req_valid) {
    mem_busy = true;
    mem_latency_cnt = 0;
    miss_delta++; // Use local delta
  }
  bool mem_resp_valid = icache_hw.io.in.mem_resp_valid;
  bool mem_resp_ready = icache_hw.io.out.mem_resp_ready;
  if (mem_resp_valid && mem_resp_ready) {
    mem_busy = false;
  }

  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;

  if (icache_hw.io.in.ifu_req_valid && icache_hw.io.out.ifu_req_ready) {
    current_vaddr_reg = in->fetch_address;
    valid_reg = true;
    access_delta++; // Use local delta
  } else if (ifu_resp_valid && ifu_resp_ready) {
    valid_reg = false;
  }
}

// --- SimpleICacheTop Implementation ---

void SimpleICacheTop::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache] reset\n");
    out->icache_read_ready = true;
    return;
  }

  out->icache_read_complete = true;
  out->icache_read_ready = true;
  out->fetch_pc = in->fetch_address;

  if (in->icache_read_valid) {
    bool mstatus[32], sstatus[32];

    cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
    cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);

    for (int i = 0; i < FETCH_WIDTH; i++) {
      uint32_t v_addr = in->fetch_address + (i * 4);
      uint32_t p_addr;

      if (v_addr / ICACHE_LINE_SIZE != (in->fetch_address) / ICACHE_LINE_SIZE) {
        out->fetch_group[i] = INST_NOP;
        out->page_fault_inst[i] = false;
        out->inst_valid[i] = false;
        continue;
      }
      out->inst_valid[i] = true;

      if ((cpu.back.out.satp & 0x80000000) && cpu.back.out.privilege != 3) {
        out->page_fault_inst[i] =
            !va2pa(p_addr, v_addr, cpu.back.out.satp, 0, mstatus, sstatus,
                   cpu.back.out.privilege, p_memory);
        if (out->page_fault_inst[i]) {
          out->fetch_group[i] = INST_NOP;
        } else {
          out->fetch_group[i] = p_memory[p_addr / 4];
        }
      } else {
        out->page_fault_inst[i] = false;
        out->fetch_group[i] = p_memory[v_addr / 4];
      }

      if (DEBUG_PRINT) {
        printf("[icache] pmem_address: %x\n", p_addr);
        printf("[icache] instruction : %x\n", out->fetch_group[i]);
      }
    }
  } else {
    out->fetch_pc = 0;
  }
}

void SimpleICacheTop::seq() {
  // No sequential logic
}

#ifdef USE_SIM_DDR
// --- SimDDRICacheTop Implementation ---
#include <MemorySubsystem.h>

SimDDRICacheTop::SimDDRICacheTop(icache_module_n::ICache &hw) : icache_hw(hw) {}

void SimDDRICacheTop::flush() {
  icache_hw.invalidate_all();
}

void SimDDRICacheTop::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache] reset\n");
    icache_hw.reset();
    out->icache_read_ready = true;
    valid_reg = false;
    return;
  }

  icache_hw.io.in.refetch = in->refetch;

  // Set input for 1st pipeline stage (IFU)
  icache_hw.io.in.pc = in->fetch_address;
  icache_hw.io.in.ifu_req_valid = in->icache_read_valid;
  icache_hw.io.in.ifu_resp_ready = true;

  // Get MMU response
  mmu_resp_master_t mmu_resp = cpu.mmu.io.out.mmu_ifu_resp;
  icache_hw.io.in.ppn = mmu_resp.ptag;
  icache_hw.io.in.ppn_valid = mmu_resp.valid && !in->refetch && !mmu_resp.miss;
  icache_hw.io.in.page_fault = mmu_resp.excp;

  // Wire Interconnect responses → ICache inputs (from previous cycle's
  // mem_subsystem.comb())
  auto &port = mem_subsystem().icache_port();
  icache_hw.io.in.mem_req_ready = port.req.ready;
  icache_hw.io.in.mem_resp_valid = port.resp.valid;
  for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
    icache_hw.io.in.mem_resp_data[i] = port.resp.data[i];
  }

  // Run ICache combinational logic
  icache_hw.comb();

  // Wire ICache outputs → Interconnect requests (will be processed by
  // mem_subsystem.comb() in next main loop iteration)
  if (!in->run_comb_only) {
    port.req.valid = icache_hw.io.out.mem_req_valid;
    port.req.addr = icache_hw.io.out.mem_req_addr;
    port.req.total_size = ICACHE_LINE_SIZE - 1;
    port.req.id = 0;
    port.resp.ready = icache_hw.io.out.mem_resp_ready;
  }

  // MMU request (same as TrueICacheTop)
  cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
  cpu.mmu.io.in.mmu_ifu_resp.ready = true;
  if (icache_hw.io.out.ifu_req_ready && icache_hw.io.in.ifu_req_valid) {
    cpu.mmu.io.in.mmu_ifu_req.valid = true;
    cpu.mmu.io.in.mmu_ifu_req.vtag = in->fetch_address >> 12;
  } else if (!icache_hw.io.out.ifu_req_ready && valid_reg) {
    cpu.mmu.io.in.mmu_ifu_req.valid = true;
    cpu.mmu.io.in.mmu_ifu_req.vtag = current_vaddr_reg >> 12;
  } else {
    cpu.mmu.io.in.mmu_ifu_req.valid = false;
  }

  if (in->run_comb_only) {
    out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
    return;
  }

  // Output to IFU
  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  if (ifu_resp_valid) {
    out->icache_read_complete = true;
    out->fetch_pc = current_vaddr_reg;
    uint32_t mask = ICACHE_LINE_SIZE - 1;
    int base_idx = (current_vaddr_reg & mask) / 4;
    bool mstatus[32], sstatus[32];
    cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
    cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);
    bool mmu_enabled =
        (cpu.back.out.satp & 0x80000000) && cpu.back.out.privilege != 3;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      if (base_idx + i >= ICACHE_LINE_SIZE / 4) {
        out->fetch_group[i] = INST_NOP;
        out->page_fault_inst[i] = false;
        out->inst_valid[i] = false;
      } else {
        out->fetch_group[i] = icache_hw.io.out.ifu_page_fault
                                  ? INST_NOP
                                  : icache_hw.io.out.rd_data[i + base_idx];
        out->page_fault_inst[i] = icache_hw.io.out.ifu_page_fault;
        out->inst_valid[i] = true;
        if (!out->page_fault_inst[i]) {
          uint32_t v_addr = current_vaddr_reg + (i * 4);
          uint32_t p_addr = v_addr;
          bool translate_ok = true;
          if (mmu_enabled) {
            translate_ok = va2pa(p_addr, v_addr, cpu.back.out.satp, 0, mstatus,
                                 sstatus, cpu.back.out.privilege, p_memory);
          }
          if (translate_ok) {
            uint32_t mem_inst = p_memory[p_addr / 4];
            if (mem_inst != out->fetch_group[i]) {
              static bool logged_mismatch = false;
              if (!logged_mismatch) {
                logged_mismatch = true;
                uint32_t line_base = p_addr & ~(ICACHE_LINE_SIZE - 1);
                uint32_t line_idx =
                    (current_vaddr_reg >> __builtin_ctz(ICACHE_LINE_SIZE)) &
                    ((1u << (12 - __builtin_ctz(ICACHE_LINE_SIZE))) - 1);
                std::cout << "[icache_check] mismatch vaddr=0x" << std::hex
                          << v_addr << " paddr=0x" << p_addr
                          << " pc=0x" << current_vaddr_reg
                          << " mmu_ppn=0x" << mmu_resp.ptag
                          << " mmu_v=" << mmu_resp.valid
                          << " mmu_miss=" << mmu_resp.miss
                          << std::dec << std::endl;
                std::cout << "[icache_check] icache line:";
                for (int w = 0; w < ICACHE_LINE_SIZE / 4; w++) {
                  std::cout << " " << std::hex
                            << icache_hw.io.out.rd_data[w];
                }
                std::cout << std::dec << std::endl;
                std::cout << "[icache_check] pmem line @0x" << std::hex
                          << line_base << ":";
                for (int w = 0; w < ICACHE_LINE_SIZE / 4; w++) {
                  std::cout << " " << std::hex
                            << p_memory[(line_base >> 2) + w];
                }
                std::cout << std::dec << std::endl;
                uint32_t page_base = p_addr & ~0xFFF;
                bool found_line = false;
                for (uint32_t off = 0; off < 0x1000; off += 4) {
                  uint32_t cand_addr = page_base + off;
                  if (p_memory[cand_addr >> 2] !=
                      icache_hw.io.out.rd_data[0]) {
                    continue;
                  }
                  bool line_match = true;
                  for (int w = 0; w < ICACHE_LINE_SIZE / 4; w++) {
                    if (p_memory[(cand_addr >> 2) + w] !=
                        icache_hw.io.out.rd_data[w]) {
                      line_match = false;
                      break;
                    }
                  }
                  if (line_match) {
                    std::cout << "[icache_check] rd_data matches pmem @0x"
                              << std::hex << cand_addr << std::dec
                              << std::endl;
                    found_line = true;
                    break;
                  }
                }
                if (!found_line) {
                  std::cout << "[icache_check] rd_data not found in page 0x"
                            << std::hex << page_base << std::dec << std::endl;
                  bool found_near = false;
                  for (int delta = -4; delta <= 4; delta++) {
                    if (delta == 0)
                      continue;
                    uint32_t cand_addr = p_addr + (delta * 0x1000);
                    bool line_match = true;
                    for (int w = 0; w < ICACHE_LINE_SIZE / 4; w++) {
                      if (p_memory[(cand_addr >> 2) + w] !=
                          icache_hw.io.out.rd_data[w]) {
                        line_match = false;
                        break;
                      }
                    }
                    if (line_match) {
                      std::cout
                          << "[icache_check] rd_data matches pmem @0x"
                          << std::hex << cand_addr << " (delta "
                          << std::showpos << (delta * 0x1000) << std::noshowpos
                          << ")" << std::dec << std::endl;
                      found_near = true;
                      break;
                    }
                  }
                  if (!found_near) {
                    std::cout << "[icache_check] rd_data not found in nearby "
                                 "pages"
                              << std::endl;
                    uint32_t search_base = 0x80400000;
                    uint32_t search_end = 0x90000000 - ICACHE_LINE_SIZE;
                    bool found_global = false;
                    for (uint32_t addr = search_base; addr <= search_end;
                         addr += 4) {
                      if (p_memory[addr >> 2] !=
                          icache_hw.io.out.rd_data[0]) {
                        continue;
                      }
                      bool line_match = true;
                      for (int w = 0; w < ICACHE_LINE_SIZE / 4; w++) {
                        if (p_memory[(addr >> 2) + w] !=
                            icache_hw.io.out.rd_data[w]) {
                          line_match = false;
                          break;
                        }
                      }
                      if (line_match) {
                        std::cout << "[icache_check] rd_data matches pmem @0x"
                                  << std::hex << addr << std::dec << std::endl;
                        found_global = true;
                        break;
                      }
                    }
                    if (!found_global) {
                      std::cout << "[icache_check] rd_data not found in "
                                   "0x80400000-0x90000000"
                                << std::endl;
                    }
                  }
                }
                std::cout << "[icache_check] set index=" << std::hex
                          << line_idx << std::dec << std::endl;
                std::cout << "[icache_check] mem_resp_v="
                          << icache_hw.io.in.mem_resp_valid
                          << " mem_resp_rdy=" << icache_hw.io.out.mem_resp_ready
                          << " mem_req_v=" << icache_hw.io.out.mem_req_valid
                          << std::endl;
                icache_hw.log_state();
                icache_hw.log_tag(line_idx);
                icache_hw.log_pipeline();
              }
            }
          }
        }
      }
    }
  } else {
    out->icache_read_complete = false;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
  }
  out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
}

void SimDDRICacheTop::seq() {
  if (in->reset)
    return;
  if (in->refetch)
    valid_reg = false;

  icache_hw.seq();

  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  if (icache_hw.io.in.ifu_req_valid && icache_hw.io.out.ifu_req_ready) {
    current_vaddr_reg = in->fetch_address;
    valid_reg = true;
    access_delta++;
  } else if (ifu_resp_valid) {
    valid_reg = false;
  }

  // Track misses
  if (icache_hw.io.out.mem_req_valid &&
      mem_subsystem().icache_port().req.ready) {
    miss_delta++;
  }
}
#endif

// --- Factory ---

ICacheTop *get_icache_instance() {
  static std::unique_ptr<ICacheTop> instance = nullptr;
  if (!instance) {
#ifdef USE_SIM_DDR
    instance = std::make_unique<SimDDRICacheTop>(icache);
#elif defined(USE_TRUE_ICACHE)
    instance = std::make_unique<TrueICacheTop>(icache);
#else
    instance = std::make_unique<SimpleICacheTop>();
#endif
  }
  return instance.get();
}
