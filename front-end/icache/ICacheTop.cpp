#include "include/ICacheTop.h"
#include "../front_module.h"
#include "../frontend.h"
#include "RISCV.h"
#include "TOP.h"
#include "config.h" // For SimContext
#include "cvt.h"
#include "include/icache_module.h"
#include "include/icache_module_v2.h"
#ifdef CONFIG_MMU
#include "mmu_io.h"
#include <MMU.h>
#endif
#include <SimCpu.h>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>

// External dependencies
extern SimCpu cpu;
extern uint32_t *p_memory;
extern icache_module_n::ICache icache;
extern icache_module_v2_n::ICacheV2 icache_v2;

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

namespace {

// Implementation using the Simple ICache Model (Ideal P-Memory Access)
class SimpleICacheTop : public ICacheTop {
public:
  void comb() override;
  void seq() override;
  void flush() override {}
};

// Implementation using the True ICache Module (Detailed Simulation)
template <typename HW> class TrueICacheTopT : public ICacheTop {
private:
  // Simple non-AXI memory model with multiple outstanding transactions (0..15)
  std::array<uint8_t, 16> mem_valid{};
  std::array<uint32_t, 16> mem_addr{};
  std::array<uint32_t, 16> mem_age{};

  HW &icache_hw;

  // When CONFIG_MMU is disabled, ICacheTop provides a minimal translation stub
  // using va2pa() (if paging is enabled) or identity mapping. The stub returns
  // translation results one cycle after the request.
  bool mmu_stub_req_valid_r = false;
  uint32_t mmu_stub_req_vtag_r = 0;

public:
  explicit TrueICacheTopT(HW &hw) : icache_hw(hw) {}
  void comb() override;
  void seq() override;
  void flush() override { icache_hw.invalidate_all(); }
};

#ifdef USE_SIM_DDR
// Implementation using TrueICache + AXI-Interconnect + SimDDR
template <typename HW> class SimDDRICacheTopT : public ICacheTop {
private:
  HW &icache_hw;

  bool mmu_stub_req_valid_r = false;
  uint32_t mmu_stub_req_vtag_r = 0;

public:
  explicit SimDDRICacheTopT(HW &hw) : icache_hw(hw) {}
  void comb() override;
  void seq() override;
  void flush() override { icache_hw.invalidate_all(); }
};
#endif

} // namespace

// --- TrueICacheTop Implementation ---
template <typename HW> void TrueICacheTopT<HW>::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache_top] reset\n");
    out->icache_read_complete = false;
    out->icache_read_ready = true;
    out->fetch_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
#ifdef CONFIG_MMU
    // Keep MMU interface idle during reset.
    cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
    cpu.mmu.io.in.mmu_ifu_resp.ready = true;
    cpu.mmu.io.in.mmu_ifu_req.valid = false;
    cpu.mmu.io.in.mmu_ifu_req.vtag = 0;
#endif
    icache_hw.io.in.flush = false;
    return;
  }

  icache_hw.io.in.refetch = in->refetch;
  icache_hw.io.in.flush = in->flush;

  // IFU request
  icache_hw.io.in.pc = in->fetch_address;
  icache_hw.io.in.ifu_req_valid = in->icache_read_valid;
  icache_hw.io.in.ifu_resp_ready = in->icache_resp_ready;

  // MMU response (from last cycle)
#ifdef CONFIG_MMU
  mmu_resp_master_t mmu_resp = cpu.mmu.io.out.mmu_ifu_resp;
  icache_hw.io.in.ppn = mmu_resp.ptag;
  icache_hw.io.in.ppn_valid = mmu_resp.valid && !in->refetch && !mmu_resp.miss;
  icache_hw.io.in.page_fault = mmu_resp.excp;
#else
  icache_hw.io.in.ppn = 0;
  icache_hw.io.in.ppn_valid = false;
  icache_hw.io.in.page_fault = false;

  if (mmu_stub_req_valid_r && !in->refetch) {
    uint32_t v_addr = mmu_stub_req_vtag_r << 12;
    uint32_t p_addr = v_addr;
    bool page_fault = false;
    if ((cpu.back.out.satp & 0x80000000) && cpu.back.out.privilege != 3) {
      bool mstatus[32], sstatus[32];
      cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
      cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);
      page_fault = !va2pa(p_addr, v_addr, cpu.back.out.satp, 0, mstatus,
                          sstatus, static_cast<int>(cpu.back.out.privilege),
                          p_memory);
    }
    icache_hw.io.in.ppn = p_addr >> 12;
    icache_hw.io.in.ppn_valid = true;
    icache_hw.io.in.page_fault = page_fault;
  }
#endif

  // Memory request ready (simple model: always accept; limited by txid pool)
  icache_hw.io.in.mem_req_ready = true;

  // Memory response: pick one matured transaction per cycle
  icache_hw.io.in.mem_resp_valid = false;
  icache_hw.io.in.mem_resp_id = 0;
  for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
    icache_hw.io.in.mem_resp_data[i] = 0;
  }

  int resp_id = -1;
  for (int id = 0; id < 16; ++id) {
    if (!mem_valid[id]) {
      continue;
    }
    if (mem_age[id] >= static_cast<uint32_t>(ICACHE_MISS_LATENCY)) {
      resp_id = id;
      break;
    }
  }
  if (resp_id >= 0) {
    icache_hw.io.in.mem_resp_valid = true;
    icache_hw.io.in.mem_resp_id = static_cast<uint8_t>(resp_id & 0xF);
    uint32_t mask = ~(ICACHE_LINE_SIZE - 1);
    uint32_t base = mem_addr[resp_id] & mask;
    for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
      icache_hw.io.in.mem_resp_data[i] = p_memory[base / 4 + i];
    }
  }

  icache_hw.comb();

  // MMU request (drive vpn directly from icache module)
#ifdef CONFIG_MMU
  cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
  cpu.mmu.io.in.mmu_ifu_resp.ready = true;
  cpu.mmu.io.in.mmu_ifu_req.valid = icache_hw.io.out.mmu_req_valid;
  cpu.mmu.io.in.mmu_ifu_req.vtag = icache_hw.io.out.mmu_req_vtag;
#endif

  // Output to IFU
  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;
  if (ifu_resp_valid) {
    out->fetch_pc = icache_hw.io.out.ifu_resp_pc;

    uint32_t mask = ICACHE_LINE_SIZE - 1;
    int base_idx = (out->fetch_pc & mask) / 4;
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
      out->page_fault_inst[i] = icache_hw.io.out.ifu_page_fault;
      out->inst_valid[i] = true;
    }
  } else {
    out->fetch_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
  }

  out->icache_read_complete = ifu_resp_valid && ifu_resp_ready;
  out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
}

template <typename HW> void TrueICacheTopT<HW>::seq() {
  if (in->reset) {
    // Sequential reset: clear internal state at the cycle boundary.
    icache_hw.reset();
    mem_valid.fill(0);
    mem_addr.fill(0);
    mem_age.fill(0);
    mmu_stub_req_valid_r = false;
    mmu_stub_req_vtag_r = 0;
    return;
  }

  icache_hw.seq();

  // Perf: IFU accept and memory request handshake
  if (icache_hw.io.in.ifu_req_valid && icache_hw.io.out.ifu_req_ready) {
    access_delta++;
  }
  if (icache_hw.io.out.mem_req_valid && icache_hw.io.in.mem_req_ready) {
    miss_delta++;
  }

#ifndef CONFIG_MMU
  // MMU stub: capture req for next cycle's translation response
  mmu_stub_req_valid_r = icache_hw.io.out.mmu_req_valid;
  mmu_stub_req_vtag_r = icache_hw.io.out.mmu_req_vtag;
#endif

  // Age outstanding requests
  for (int id = 0; id < 16; ++id) {
    if (mem_valid[id] &&
        mem_age[id] < static_cast<uint32_t>(ICACHE_MISS_LATENCY)) {
      mem_age[id]++;
    }
  }

  // Consume memory response first (allow resp+new-req same cycle)
  if (icache_hw.io.in.mem_resp_valid && icache_hw.io.out.mem_resp_ready) {
    uint8_t id = static_cast<uint8_t>(icache_hw.io.in.mem_resp_id & 0xF);
    mem_valid[id] = 0;
    mem_addr[id] = 0;
    mem_age[id] = 0;
  }

  // Accept new memory request
  if (icache_hw.io.out.mem_req_valid && icache_hw.io.in.mem_req_ready) {
    uint8_t id = static_cast<uint8_t>(icache_hw.io.out.mem_req_id & 0xF);
    if (mem_valid[id]) {
      // Direct memory model is single-entry per txid. If the ICache reuses an
      // ID before the previous one returns, treat it as cancel+replace to
      // avoid delivering stale data for the old address.
      if (mem_addr[id] != icache_hw.io.out.mem_req_addr) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          std::cout << "[icache_top] WARN: mem_req_id reuse id=" << std::dec
                    << static_cast<int>(id) << " addr_old=0x" << std::hex
                    << mem_addr[id] << " addr_new=0x"
                    << icache_hw.io.out.mem_req_addr << std::dec
                    << " at sim_time=" << sim_time << std::endl;
        }
      }
      mem_addr[id] = icache_hw.io.out.mem_req_addr;
      mem_age[id] = 0;
      return;
    }
    mem_valid[id] = 1;
    mem_addr[id] = icache_hw.io.out.mem_req_addr;
    mem_age[id] = 0;
  }
}

// --- SimpleICacheTop Implementation ---

void SimpleICacheTop::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache] reset\n");
    out->icache_read_complete = false;
    out->icache_read_ready = true;
    out->fetch_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
    return;
  }

  // Ideal model: always ready; "complete" whenever a fetch request is issued.
  out->icache_read_ready = true;
  out->icache_read_complete = in->icache_read_valid;
  out->fetch_pc = in->icache_read_valid ? in->fetch_address : 0;

  if (!in->run_comb_only && in->icache_read_valid) {
    access_delta++;
  }

  if (in->icache_read_valid) {
    bool mstatus[32], sstatus[32];

    cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
    cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);

    for (int i = 0; i < FETCH_WIDTH; i++) {
      uint32_t v_addr = in->fetch_address + (i * 4);
      uint32_t p_addr = v_addr;

      if (v_addr / ICACHE_LINE_SIZE != (in->fetch_address) / ICACHE_LINE_SIZE) {
        out->fetch_group[i] = INST_NOP;
        out->page_fault_inst[i] = false;
        out->inst_valid[i] = false;
        continue;
      }
      out->inst_valid[i] = true;

      // Ideal ICache model still needs correct VA->PA translation and
      // instruction page fault detection for difftest/Linux boot.
      if ((cpu.back.out.satp & 0x80000000) && cpu.back.out.privilege != 3) {
        out->page_fault_inst[i] =
            !va2pa(p_addr, v_addr, cpu.back.out.satp, 0, mstatus, sstatus,
                   cpu.back.out.privilege, p_memory);
      } else {
        out->page_fault_inst[i] = false;
      }
      out->fetch_group[i] =
          out->page_fault_inst[i] ? INST_NOP : p_memory[p_addr / 4];

      if (DEBUG_PRINT) {
        printf("[icache] pmem_address: %x\n", p_addr);
        printf("[icache] instruction : %x\n", out->fetch_group[i]);
      }
    }
  } else {
    // No request this cycle: drive deterministic outputs (avoid re-sending
    // previous fetch group into FIFO).
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
  }
}

void SimpleICacheTop::seq() {
  // No sequential logic
}

#ifdef USE_SIM_DDR
// --- SimDDRICacheTop Implementation ---
#include <MemorySubsystem.h>

template <typename HW> void SimDDRICacheTopT<HW>::comb() {
  if (in->reset) {
    DEBUG_LOG("[icache_top] reset\n");
    out->icache_read_complete = false;
    out->icache_read_ready = true;
    out->fetch_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
#ifdef CONFIG_MMU
    // Keep MMU interface idle during reset.
    cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
    cpu.mmu.io.in.mmu_ifu_resp.ready = true;
    cpu.mmu.io.in.mmu_ifu_req.valid = false;
    cpu.mmu.io.in.mmu_ifu_req.vtag = 0;
#endif
    // Keep interconnect interface idle during reset.
    auto &port = mem_subsystem().icache_port();
    port.req.valid = false;
    port.req.addr = 0;
    port.req.total_size = 0;
    port.req.id = 0;
    port.resp.ready = false;
    icache_hw.io.in.flush = false;
    return;
  }

  icache_hw.io.in.refetch = in->refetch;
  icache_hw.io.in.flush = in->flush;

  // IFU request
  icache_hw.io.in.pc = in->fetch_address;
  icache_hw.io.in.ifu_req_valid = in->icache_read_valid;
  icache_hw.io.in.ifu_resp_ready = in->icache_resp_ready;

  // MMU response (from last cycle)
#ifdef CONFIG_MMU
  mmu_resp_master_t mmu_resp = cpu.mmu.io.out.mmu_ifu_resp;
  icache_hw.io.in.ppn = mmu_resp.ptag;
  icache_hw.io.in.ppn_valid = mmu_resp.valid && !in->refetch && !mmu_resp.miss;
  icache_hw.io.in.page_fault = mmu_resp.excp;
#else
  icache_hw.io.in.ppn = 0;
  icache_hw.io.in.ppn_valid = false;
  icache_hw.io.in.page_fault = false;

  if (mmu_stub_req_valid_r && !in->refetch) {
    uint32_t v_addr = mmu_stub_req_vtag_r << 12;
    uint32_t p_addr = v_addr;
    bool page_fault = false;
    if ((cpu.back.out.satp & 0x80000000) && cpu.back.out.privilege != 3) {
      bool mstatus[32], sstatus[32];
      cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
      cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);
      page_fault = !va2pa(p_addr, v_addr, cpu.back.out.satp, 0, mstatus,
                          sstatus, static_cast<int>(cpu.back.out.privilege),
                          p_memory);
    }
    icache_hw.io.in.ppn = p_addr >> 12;
    icache_hw.io.in.ppn_valid = true;
    icache_hw.io.in.page_fault = page_fault;
  }
#endif

  // Interconnect -> ICache inputs
  auto &port = mem_subsystem().icache_port();
  icache_hw.io.in.mem_req_ready = port.req.ready;
  icache_hw.io.in.mem_resp_valid = port.resp.valid;
  icache_hw.io.in.mem_resp_id = static_cast<uint8_t>(port.resp.id & 0xF);
  for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
    icache_hw.io.in.mem_resp_data[i] = port.resp.data[i];
  }

  icache_hw.comb();

  // ICache outputs -> interconnect requests
  port.req.valid = icache_hw.io.out.mem_req_valid;
  port.req.addr = icache_hw.io.out.mem_req_addr;
  port.req.total_size = ICACHE_LINE_SIZE - 1;
  port.req.id = static_cast<uint8_t>(icache_hw.io.out.mem_req_id & 0xF);
  port.resp.ready = icache_hw.io.out.mem_resp_ready;

  // MMU request (drive vpn directly from icache module)
#ifdef CONFIG_MMU
  cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
  cpu.mmu.io.in.mmu_ifu_resp.ready = true;
  cpu.mmu.io.in.mmu_ifu_req.valid = icache_hw.io.out.mmu_req_valid;
  cpu.mmu.io.in.mmu_ifu_req.vtag = icache_hw.io.out.mmu_req_vtag;
#endif

  // Output to IFU
  bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
  bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;
  if (ifu_resp_valid) {
    out->fetch_pc = icache_hw.io.out.ifu_resp_pc;
    uint32_t mask = ICACHE_LINE_SIZE - 1;
    int base_idx = (out->fetch_pc & mask) / 4;

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

#ifndef CONFIG_MMU
        // Optional correctness check (instruction match against pmem)
        if (!out->page_fault_inst[i]) {
          uint32_t v_addr = out->fetch_pc + (i * 4);
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
                std::cout << "[icache_check] mismatch vaddr=0x" << std::hex
                          << v_addr << " paddr=0x" << p_addr
                          << " pc=0x" << out->fetch_pc << " got=0x"
                          << out->fetch_group[i] << " exp=0x" << mem_inst
                          << std::dec << std::endl;
              }
            }
          }
        }
#endif
      }
    }
  } else {
    out->fetch_pc = 0;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out->fetch_group[i] = INST_NOP;
      out->page_fault_inst[i] = false;
      out->inst_valid[i] = false;
    }
  }

  out->icache_read_complete = ifu_resp_valid && ifu_resp_ready;
  out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
}

template <typename HW> void SimDDRICacheTopT<HW>::seq() {
  if (in->reset) {
    icache_hw.reset();
    mmu_stub_req_valid_r = false;
    mmu_stub_req_vtag_r = 0;
    return;
  }

  icache_hw.seq();

#ifndef CONFIG_MMU
  // MMU stub: capture req for next cycle's translation response
  mmu_stub_req_valid_r = icache_hw.io.out.mmu_req_valid;
  mmu_stub_req_vtag_r = icache_hw.io.out.mmu_req_vtag;
#endif

  if (icache_hw.io.in.ifu_req_valid && icache_hw.io.out.ifu_req_ready) {
    access_delta++;
  }

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
#ifdef USE_IDEAL_ICACHE
    instance = std::make_unique<SimpleICacheTop>();
#elif defined(USE_SIM_DDR)
#ifdef USE_ICACHE_V2
    instance =
        std::make_unique<SimDDRICacheTopT<icache_module_v2_n::ICacheV2>>(
            icache_v2);
#else
    instance = std::make_unique<SimDDRICacheTopT<icache_module_n::ICache>>(
        icache);
#endif
#elif defined(USE_TRUE_ICACHE)
#ifdef USE_ICACHE_V2
    instance = std::make_unique<TrueICacheTopT<icache_module_v2_n::ICacheV2>>(
        icache_v2);
#else
    instance = std::make_unique<TrueICacheTopT<icache_module_n::ICache>>(
        icache);
#endif
#else
    instance = std::make_unique<SimpleICacheTop>();
#endif
  }
  return instance.get();
}
