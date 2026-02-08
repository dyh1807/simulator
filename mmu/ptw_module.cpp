#include "include/ptw_module.h"

namespace ptw_module_n {

namespace {

enum PTWState : uint8_t { IDLE = 0, CACHE_1 = 1, MEM_1 = 2, CACHE_2 = 3, MEM_2 = 4 };
enum DCacheState : uint8_t { DCACHE_IDLE = 0, DCACHE_BUSY = 1 };

inline bool pte_bit(uint32_t pte, uint32_t bit) { return ((pte >> bit) & 0x1u) != 0; }

inline uint32_t pte_ppn0(uint32_t pte) { return (pte >> 10) & 0x3ffu; }
inline uint32_t pte_ppn1(uint32_t pte) { return (pte >> 20) & 0xfffu; }

bool is_page_fault(uint32_t pte_raw, uint32_t op_type, bool stage_1,
                   uint32_t mstatus, uint32_t sstatus, uint32_t privilege,
                   bool &is_megapage) {
  bool valid = pte_bit(pte_raw, 0);
  bool read = pte_bit(pte_raw, 1);
  bool write = pte_bit(pte_raw, 2);
  bool execute = pte_bit(pte_raw, 3);
  bool user = pte_bit(pte_raw, 4);
  bool accessed = pte_bit(pte_raw, 6);
  bool dirty = pte_bit(pte_raw, 7);
  uint32_t ppn0 = pte_ppn0(pte_raw);
  is_megapage = stage_1 && (read || execute);

  if (!valid) {
    return true;
  }
  if (!read && write) {
    return true;
  }

  bool mxr = ((mstatus >> 19) & 0x1u) != 0;
  bool sum_m = ((mstatus >> 18) & 0x1u) != 0;
  bool sum_s = ((sstatus >> 18) & 0x1u) != 0;
  bool mprv = ((mstatus >> 17) & 0x1u) != 0;
  uint32_t mpp = (mstatus >> 11) & 0x3u;

  if (!((op_type == mmu_n::OP_FETCH && execute) ||
        (op_type == mmu_n::OP_LOAD && (read || (mxr && execute))) ||
        (op_type == mmu_n::OP_STORE && write))) {
    return true;
  }

  if (privilege == mmu_n::S_MODE && !sum_m && user && !sum_s) {
    return true;
  }
  if (privilege != mmu_n::S_MODE && mprv && mpp == mmu_n::S_MODE && !sum_m && user &&
      !sum_s) {
    return true;
  }
  if (is_megapage && ppn0 != 0) {
    return true;
  }
  if (!accessed) {
    return true;
  }
  if (op_type == mmu_n::OP_STORE && !dirty) {
    return true;
  }
  return false;
}

void fill_write_from_pte(PTW_out_t &out, PTW_table_write_t &table_write,
                         uint32_t pte_raw, uint32_t asid, uint32_t vpn1, uint32_t vpn0,
                         uint32_t op_type, bool is_megapage) {
  bool valid = pte_bit(pte_raw, 0);
  bool read = pte_bit(pte_raw, 1);
  bool write = pte_bit(pte_raw, 2);
  bool execute = pte_bit(pte_raw, 3);
  bool user = pte_bit(pte_raw, 4);
  bool global = pte_bit(pte_raw, 5);
  bool accessed = pte_bit(pte_raw, 6);
  bool dirty = pte_bit(pte_raw, 7);

  wire1_t dest =
      (op_type == static_cast<uint32_t>(mmu_n::OP_FETCH))
          ? static_cast<wire1_t>(mmu_n::DEST_ITLB)
          : static_cast<wire1_t>(mmu_n::DEST_DTLB);

  out.ptw_write_valid = true;
  out.ptw_write_dest = dest;
  out.ptw_write_vpn1 = vpn1;
  out.ptw_write_vpn0 = vpn0;
  out.ptw_write_ppn1 = pte_ppn1(pte_raw);
  out.ptw_write_ppn0 = pte_ppn0(pte_raw);
  out.ptw_write_asid = asid;
  out.ptw_write_megapage = is_megapage;
  out.ptw_write_dirty = dirty;
  out.ptw_write_accessed = accessed;
  out.ptw_write_global = global;
  out.ptw_write_user = user;
  out.ptw_write_execute = execute;
  out.ptw_write_write = write;
  out.ptw_write_read = read;
  out.ptw_write_valid_bit = valid;
  out.ptw_write_pte_valid = true;

  table_write.we = true;
  table_write.dest = dest;
  table_write.vpn1 = vpn1;
  table_write.vpn0 = vpn0;
  table_write.ppn1 = pte_ppn1(pte_raw);
  table_write.ppn0 = pte_ppn0(pte_raw);
  table_write.asid = asid;
  table_write.megapage = is_megapage;
  table_write.dirty = dirty;
  table_write.accessed = accessed;
  table_write.global = global;
  table_write.user = user;
  table_write.execute = execute;
  table_write.write = write;
  table_write.read = read;
  table_write.valid_bit = valid;
  table_write.pte_valid = true;
}

} // namespace

void PTWModule::reset() {
  io = {};
}

void PTWModule::comb() {
  io.out = {};
  io.table_write = {};
  io.reg_write = io.regs;

  PTWState state = static_cast<PTWState>(io.regs.state_r);
  DCacheState dcache_state = static_cast<DCacheState>(io.regs.dcache_state_r);

  auto handle_leaf_pte = [&](uint32_t pte_raw, bool stage_1) {
    bool is_megapage = false;
    bool fault = is_page_fault(pte_raw, io.regs.op_type_r, stage_1, io.in.mstatus,
                               io.in.sstatus, io.in.privilege, is_megapage);
    if (fault || is_megapage || !stage_1) {
      io.reg_write.state_r = IDLE;
      fill_write_from_pte(io.out, io.table_write, pte_raw, io.in.satp_asid, io.regs.vpn1_r,
                          io.regs.vpn0_r, io.regs.op_type_r, is_megapage);
      return true;
    }
    return false;
  };

  switch (state) {
  case IDLE:
    if (io.in.tlb_miss) {
      io.reg_write.state_r = CACHE_1;
      io.reg_write.vpn1_r = io.in.tlb_vpn1;
      io.reg_write.vpn0_r = io.in.tlb_vpn0;
      io.reg_write.op_type_r = io.in.tlb_op_type;
    }
    break;

  case CACHE_1: {
    io.reg_write.state_r = CACHE_1;
    if (dcache_state == DCACHE_IDLE) {
      io.out.dcache_req_valid = true;
      io.out.dcache_req_paddr = (io.in.satp_ppn << 12) | (io.regs.vpn1_r << 2);
      if (io.out.dcache_req_valid && io.in.dcache_req_ready) {
        io.reg_write.dcache_state_r = DCACHE_BUSY;
      }
    } else {
      io.out.dcache_resp_ready = true;
      if (io.in.dcache_resp_valid && io.out.dcache_resp_ready) {
        io.reg_write.dcache_state_r = DCACHE_IDLE;
        if (io.in.dcache_resp_miss) {
          io.reg_write.state_r = MEM_1;
        } else {
          io.reg_write.pte1_raw_r = io.in.dcache_resp_data;
          if (!handle_leaf_pte(io.in.dcache_resp_data, true)) {
            io.reg_write.state_r = CACHE_2;
          }
        }
      }
    }
    break;
  }

  case MEM_1: {
    io.reg_write.state_r = MEM_1;
    if (dcache_state == DCACHE_IDLE) {
      io.out.mem_req_valid = true;
      io.out.mem_req_paddr = (io.in.satp_ppn << 12) | (io.regs.vpn1_r << 2);
      if (io.out.mem_req_valid && io.in.mem_req_ready) {
        io.reg_write.dcache_state_r = DCACHE_BUSY;
      }
    } else {
      io.out.mem_resp_ready = true;
      if (io.in.mem_resp_valid && io.out.mem_resp_ready) {
        io.reg_write.dcache_state_r = DCACHE_IDLE;
        io.reg_write.pte1_raw_r = io.in.mem_resp_data;
        if (!handle_leaf_pte(io.in.mem_resp_data, true)) {
          io.reg_write.state_r = CACHE_2;
        }
      }
    }
    break;
  }

  case CACHE_2: {
    io.reg_write.state_r = CACHE_2;
    if (dcache_state == DCACHE_IDLE) {
      uint32_t pte1_ppn =
          ((pte_ppn1(io.regs.pte1_raw_r) << 10) | pte_ppn0(io.regs.pte1_raw_r)) &
          0xFFFFFu;
      io.out.dcache_req_valid = true;
      io.out.dcache_req_paddr = (pte1_ppn << 12) | (io.regs.vpn0_r << 2);
      if (io.out.dcache_req_valid && io.in.dcache_req_ready) {
        io.reg_write.dcache_state_r = DCACHE_BUSY;
      }
    } else {
      io.out.dcache_resp_ready = true;
      if (io.in.dcache_resp_valid && io.out.dcache_resp_ready) {
        io.reg_write.dcache_state_r = DCACHE_IDLE;
        if (io.in.dcache_resp_miss) {
          io.reg_write.state_r = MEM_2;
        } else {
          io.reg_write.pte2_raw_r = io.in.dcache_resp_data;
          (void)handle_leaf_pte(io.in.dcache_resp_data, false);
        }
      }
    }
    break;
  }

  case MEM_2: {
    io.reg_write.state_r = MEM_2;
    if (dcache_state == DCACHE_IDLE) {
      uint32_t pte1_ppn =
          ((pte_ppn1(io.regs.pte1_raw_r) << 10) | pte_ppn0(io.regs.pte1_raw_r)) &
          0xFFFFFu;
      io.out.mem_req_valid = true;
      io.out.mem_req_paddr = (pte1_ppn << 12) | (io.regs.vpn0_r << 2);
      if (io.out.mem_req_valid && io.in.mem_req_ready) {
        io.reg_write.dcache_state_r = DCACHE_BUSY;
      }
    } else {
      io.out.mem_resp_ready = true;
      if (io.in.mem_resp_valid && io.out.mem_resp_ready) {
        io.reg_write.dcache_state_r = DCACHE_IDLE;
        io.reg_write.pte2_raw_r = io.in.mem_resp_data;
        (void)handle_leaf_pte(io.in.mem_resp_data, false);
      }
    }
    break;
  }

  default:
    io.reg_write.state_r = IDLE;
    io.reg_write.dcache_state_r = DCACHE_IDLE;
    break;
  }
}

void PTWModule::seq() {
  io.regs = io.reg_write;
}

} // namespace ptw_module_n

