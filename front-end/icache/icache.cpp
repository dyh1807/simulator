#include "../front_IO.h"
#include "../front_module.h"
#include "../frontend.h"
#include "RISCV.h"
#include "TOP.h"
#include "config.h"
#include "cvt.h"
#include <cstdint>
#include <cstdio>
// no actual icache, just a simple simulation
#include "./include/icache_module.h"
#include "mmu_io.h"
#include <MMU.h>
#include <SimCpu.h>
#include <queue>
#include <memory>

ICache icache;
extern SimCpu cpu;

extern uint32_t *p_memory;
bool va2pa(uint32_t &p_addr, uint32_t v_addr, uint32_t satp, uint32_t type,
           bool *mstatus, bool *sstatus, int privilege, uint32_t *p_memory);

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
  // register to keep track of whether memory is busy
  bool mem_busy = false;
  int mem_latency_cnt = 0;
  // register to keep track of the vaddr being handled by icache
  uint32_t current_vaddr_reg = 0; // the vaddr being processed by icache
  bool valid_reg = false;         // whether current_vaddr_reg is valid
  
  ICache& icache_hw;

public:
  TrueICacheTop(ICache& hw) : icache_hw(hw) {}

  void comb(struct icache_in *in, struct icache_out *out) override {
    if (in->reset) {
      DEBUG_LOG("[icache] reset\n");
      icache_hw.reset();
      out->icache_read_ready = true;
      return;
    }

    // deal with "refetch" signal (Async Reset behavior)
    if (in->refetch) {
      // clear the icache state
      icache_hw.set_refetch();
      // We also need to update local state immediately for this cycle's comb logic
      valid_reg = false;
      mem_busy = false;
      mem_latency_cnt = 0;
    }

    // set input for 1st pipeline stage (IFU)
    icache_hw.io.in.pc = in->fetch_address;
    icache_hw.io.in.ifu_req_valid = in->icache_read_valid;

    // set input for 2nd pipeline stage (IFU)
    icache_hw.io.in.ifu_resp_ready = true; // ifu ready to receive data from icache

    // get ifu_resp from mmu (calculate last cycle)
    mmu_resp_master_t mmu_resp = cpu.mmu.io.out.mmu_ifu_resp;

    // set input for 2nd pipeline stage (MMU)
    icache_hw.io.in.ppn = mmu_resp.ptag;
    icache_hw.io.in.ppn_valid = mmu_resp.valid && !in->refetch && !mmu_resp.miss;
    icache_hw.io.in.page_fault = mmu_resp.excp;

    // set input for 2nd pipeline stage (Memory)
    if (mem_busy) {
      // A memory request is ongoing, waiting for response from memory
      // In current design, memory always responds in 1 cycle
      if (mem_latency_cnt >= ICACHE_MISS_LATENCY) {
        icache_hw.io.in.mem_resp_valid = true;
      } else {
        icache_hw.io.in.mem_resp_valid = false;
      }
      bool mem_resp_valid = icache_hw.io.in.mem_resp_valid;
      bool mem_resp_ready = icache_hw.io.out.mem_resp_ready;
      if (mem_resp_valid) {
        // if 2 ^ k == ICACHE_LINE_SIZE, then mask = 0xFFFF_FFFF << k
        uint32_t mask = ~(ICACHE_LINE_SIZE - 1);
        uint32_t cacheline_base_addr = icache_hw.io.out.mem_req_addr & mask;
        // I-Cache receive data from memory
        for (int i = 0; i < ICACHE_LINE_SIZE / 4; i++) {
          icache_hw.io.in.mem_resp_data[i] = p_memory[cacheline_base_addr / 4 + i];
        }
      }
    } else {
      // memory is idle and able to receive new request from icache
      icache_hw.io.in.mem_req_ready = true;
    }

    icache_hw.comb();

    // set input for request to mmu
    cpu.mmu.io.in.mmu_ifu_req.op_type = mmu_n::OP_FETCH;
    cpu.mmu.io.in.mmu_ifu_resp.ready = true; // ready to receive resp
    if (icache_hw.io.out.ifu_req_ready && icache_hw.io.in.ifu_req_valid) {
      // case1: ICache 支持新的请求，故而可以将新的 IFU 请求发送给 MMU
      cpu.mmu.io.in.mmu_ifu_req.valid =
          icache_hw.io.out.ifu_req_ready && in->icache_read_valid;
      cpu.mmu.io.in.mmu_ifu_req.vtag = in->fetch_address >> 12;
    } else if (!icache_hw.io.out.ifu_req_ready) {
      // case 2: ICache 不支持新的请求，需要重发（replay），具体有两种可能：
      //  - icache miss，正在等待 memory 的数据返回
      //  - 上一个 mmu_ifu_req 请求返回了 miss
      cpu.mmu.io.in.mmu_ifu_req.valid = true; // replay request
      if (!valid_reg) {
        cout
            << "[icache_top] ERROR: valid_reg is false when replaying mmu_ifu_req"
            << std::endl;
        cout << "[icache_top] sim_time: " << dec << sim_time << std::endl;
        exit(1);
      }
      cpu.mmu.io.in.mmu_ifu_req.vtag = current_vaddr_reg >> 12;
    } else {
      // case3: ICache 支持新的请求，但是 IFU 没有发送新的请求
      cpu.mmu.io.in.mmu_ifu_req.valid = false;
    }

    // Set outputs
    if (in->run_comb_only) {
      // Only run combinational logic, do not update registers. This is
      // used for BPU module, which needs to know if icache is ready
      out->icache_read_ready = icache_hw.io.out.ifu_req_ready;
      return;
    }

    // Logic for output (to IFU)
    bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
    bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;
    bool miss = icache_hw.io.out.miss;
    if (ifu_resp_valid && ifu_resp_ready) {
      out->icache_read_complete = true;
      if (miss) {
        cout << "[icache_top] WARNING: miss is true when ifu_resp is valid"
             << std::endl;
        cout << "[icache_top] sim_time: " << dec << sim_time << std::endl;
        exit(1);
      }
      // keep index within a cacheline
      out->fetch_pc = current_vaddr_reg;
      uint32_t mask = ICACHE_LINE_SIZE - 1; // work for ICACHE_LINE_SIZE==2^k
      int base_idx = (current_vaddr_reg & mask) /
                     4; // index of the instruction in the cacheline
      for (int i = 0; i < FETCH_WIDTH; i++) {
        if (base_idx + i >= ICACHE_LINE_SIZE / 4) {
          // throw the instruction that exceeds the cacheline
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
      out->icache_read_complete = false;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        out->fetch_group[i] = INST_NOP;
        out->page_fault_inst[i] = false;
        out->inst_valid[i] = false;
      }
    }
  }

  void seq(struct icache_in *in) override {
     if (in->reset) return;

    icache_hw.seq();

    // sequential logic for memory (mem_busy)
    if (mem_busy) {
      mem_latency_cnt++;
    }
    bool mem_req_ready = !mem_busy;
    bool mem_req_valid = icache_hw.io.out.mem_req_valid;
    if (mem_req_ready && mem_req_valid) {
      // send request to memory
      mem_busy = true;
      mem_latency_cnt = 0;
      cpu.ctx.perf.icache_miss_num++;
    }
    bool mem_resp_valid = icache_hw.io.in.mem_resp_valid;
    bool mem_resp_ready = icache_hw.io.out.mem_resp_ready;
    if (mem_resp_valid && mem_resp_ready) {
      // have received response from memory
      mem_busy = false;
      icache_hw.io.in.mem_resp_valid = false; // clear the signal after receiving
    }

    // sequential logic for current_vaddr_reg and valid_reg
    bool ifu_resp_valid = icache_hw.io.out.ifu_resp_valid;
    bool ifu_resp_ready = icache_hw.io.in.ifu_resp_ready;

    if (icache_hw.io.in.ifu_req_valid && icache_hw.io.out.ifu_req_ready) {
      // only push when ifu_req is sent to icache
      current_vaddr_reg = in->fetch_address;
      valid_reg = true;
      cpu.ctx.perf.icache_access_num++;
    } else if (ifu_resp_valid && ifu_resp_ready) {
      valid_reg = false; // clear the valid_reg when ifu_resp is sent to ifu
    }
  }
};

// Implementation using the Simple ICache Model (Ideal P-Memory Access)
class SimpleICacheTop : public ICacheTop {
public:
  void comb(struct icache_in *in, struct icache_out *out) override {
    if (in->reset) {
      DEBUG_LOG("[icache] reset\n");
      out->icache_read_ready = true;
      return;
    }

    // able to fetch instructions within 1 cycle
    out->icache_read_complete = true;
    out->icache_read_ready = true;
    out->fetch_pc = in->fetch_address;
    // when BPU sends a valid read request
    if (in->icache_read_valid) {
      // read instructions from pmem
      bool mstatus[32], sstatus[32];

      cvt_number_to_bit_unsigned(mstatus, cpu.back.out.mstatus, 32);
      cvt_number_to_bit_unsigned(sstatus, cpu.back.out.sstatus, 32);

      for (int i = 0; i < FETCH_WIDTH; i++) {
        uint32_t v_addr = in->fetch_address + (i * 4);
        uint32_t p_addr;
        // avoid crossing cacheline fetch
        if (v_addr / ICACHE_LINE_SIZE != (in->fetch_address) / ICACHE_LINE_SIZE) {
          // crossing cache line
          out->fetch_group[i] = INST_NOP;
          out->page_fault_inst[i] = false;
          out->inst_valid[i] = false;
          continue;
        }
        out->inst_valid[i] = true;

        //
        // fetch instructions not crossing cache line
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
      out->fetch_pc = 0; // Set default value when not valid
    }
  }

  void seq(struct icache_in *in) override {
      // No sequential logic for simple model
  }
};

// Singleton Factory
ICacheTop* get_icache_instance() {
    static std::unique_ptr<ICacheTop> instance = nullptr;
    if (!instance) {
#ifdef USE_TRUE_ICACHE
        instance = std::make_unique<TrueICacheTop>(icache);
#else
        instance = std::make_unique<SimpleICacheTop>();
#endif
    }
    return instance.get();
}

void icache_top(struct icache_in *in, struct icache_out *out) {
  get_icache_instance()->step(in, out);
}