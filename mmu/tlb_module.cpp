#include "include/tlb_module.h"

namespace tlb_module_n {

namespace {

struct FlatEntry {
  bool pte_valid = false;
  uint32_t vpn1 = 0;
  uint32_t vpn0 = 0;
  uint32_t ppn1 = 0;
  uint32_t ppn0 = 0;
  uint32_t asid = 0;
  bool megapage = false;
  bool dirty = false;
  bool accessed = false;
  bool global = false;
  bool user = false;
  bool execute = false;
  bool write = false;
  bool read = false;
  bool valid = false;
};

inline uint32_t clamp_lookup_latency(uint32_t v) { return (v < 1u) ? 1u : v; }

inline bool lookup_latency_enabled() { return (MMU_TLB_LOOKUP_LATENCY > 0); }

inline bool lookup_from_input_enabled() { return (MMU_TLB_LOOKUP_FROM_INPUT != 0); }

FlatEntry load_entry(const TLB_regs_t &regs, uint32_t idx) {
  FlatEntry entry;
  entry.pte_valid = regs.entry_pte_valid[idx];
  entry.vpn1 = regs.entry_vpn1[idx];
  entry.vpn0 = regs.entry_vpn0[idx];
  entry.ppn1 = regs.entry_ppn1[idx];
  entry.ppn0 = regs.entry_ppn0[idx];
  entry.asid = regs.entry_asid[idx];
  entry.megapage = regs.entry_megapage[idx];
  entry.dirty = regs.entry_dirty[idx];
  entry.accessed = regs.entry_accessed[idx];
  entry.global = regs.entry_global[idx];
  entry.user = regs.entry_user[idx];
  entry.execute = regs.entry_execute[idx];
  entry.write = regs.entry_write[idx];
  entry.read = regs.entry_read[idx];
  entry.valid = regs.entry_valid[idx];
  return entry;
}

void store_entry(TLB_reg_write_t &regs, uint32_t idx, const FlatEntry &entry) {
  regs.entry_pte_valid[idx] = entry.pte_valid;
  regs.entry_vpn1[idx] = entry.vpn1;
  regs.entry_vpn0[idx] = entry.vpn0;
  regs.entry_ppn1[idx] = entry.ppn1;
  regs.entry_ppn0[idx] = entry.ppn0;
  regs.entry_asid[idx] = entry.asid;
  regs.entry_megapage[idx] = entry.megapage;
  regs.entry_dirty[idx] = entry.dirty;
  regs.entry_accessed[idx] = entry.accessed;
  regs.entry_global[idx] = entry.global;
  regs.entry_user[idx] = entry.user;
  regs.entry_execute[idx] = entry.execute;
  regs.entry_write[idx] = entry.write;
  regs.entry_read[idx] = entry.read;
  regs.entry_valid[idx] = entry.valid;
}

FlatEntry load_ptw_entry(const TLB_in_t &in) {
  FlatEntry entry;
  entry.pte_valid = in.ptw_write_pte_valid;
  entry.vpn1 = in.ptw_write_vpn1;
  entry.vpn0 = in.ptw_write_vpn0;
  entry.ppn1 = in.ptw_write_ppn1;
  entry.ppn0 = in.ptw_write_ppn0;
  entry.asid = in.ptw_write_asid;
  entry.megapage = in.ptw_write_megapage;
  entry.dirty = in.ptw_write_dirty;
  entry.accessed = in.ptw_write_accessed;
  entry.global = in.ptw_write_global;
  entry.user = in.ptw_write_user;
  entry.execute = in.ptw_write_execute;
  entry.write = in.ptw_write_write;
  entry.read = in.ptw_write_read;
  entry.valid = in.ptw_write_valid_bit;
  return entry;
}

bool lookup_entry(const TLB_regs_t &regs, uint32_t asid, uint32_t vtag,
                  uint32_t &hit_index, FlatEntry &hit_entry) {
  uint32_t vpn1 = (vtag >> 10) & 0x3ffu;
  uint32_t vpn0 = vtag & 0x3ffu;
  int found = -1;
  for (uint32_t i = 0; i < kTlbEntryNum; ++i) {
    if (!regs.entry_pte_valid[i]) {
      continue;
    }
    if (regs.entry_vpn1[i] != vpn1) {
      continue;
    }
    if (regs.entry_asid[i] != asid) {
      continue;
    }
    bool vpn0_match = regs.entry_megapage[i] || (regs.entry_vpn0[i] == vpn0);
    if (!vpn0_match) {
      continue;
    }
    if (found >= 0) {
      // Ambiguous entries are treated as miss on this functional model path.
      return false;
    }
    found = static_cast<int>(i);
  }
  if (found < 0) {
    return false;
  }
  hit_index = static_cast<uint32_t>(found);
  hit_entry = load_entry(regs, hit_index);
  return true;
}

void fill_ifu_resp(TLB_out_t &out, bool valid, bool hit, uint32_t hit_index,
                   const FlatEntry &entry) {
  out.ifu_resp_valid = valid;
  out.ifu_resp_hit = hit;
  out.ifu_resp_hit_index = hit_index;
  if (!valid || !hit) {
    return;
  }
  out.ifu_resp_vpn1 = entry.vpn1;
  out.ifu_resp_vpn0 = entry.vpn0;
  out.ifu_resp_ppn1 = entry.ppn1;
  out.ifu_resp_ppn0 = entry.ppn0;
  out.ifu_resp_asid = entry.asid;
  out.ifu_resp_megapage = entry.megapage;
  out.ifu_resp_dirty = entry.dirty;
  out.ifu_resp_accessed = entry.accessed;
  out.ifu_resp_global = entry.global;
  out.ifu_resp_user = entry.user;
  out.ifu_resp_execute = entry.execute;
  out.ifu_resp_write = entry.write;
  out.ifu_resp_read = entry.read;
  out.ifu_resp_valid_bit = entry.valid;
  out.ifu_resp_pte_valid = entry.pte_valid;
}

void fill_lsu_resp(TLB_out_t &out, uint32_t port, bool valid, bool hit,
                   uint32_t hit_index, const FlatEntry &entry) {
  out.lsu_resp_valid[port] = valid;
  out.lsu_resp_hit[port] = hit;
  out.lsu_resp_hit_index[port] = hit_index;
  if (!valid || !hit) {
    return;
  }
  out.lsu_resp_vpn1[port] = entry.vpn1;
  out.lsu_resp_vpn0[port] = entry.vpn0;
  out.lsu_resp_ppn1[port] = entry.ppn1;
  out.lsu_resp_ppn0[port] = entry.ppn0;
  out.lsu_resp_asid[port] = entry.asid;
  out.lsu_resp_megapage[port] = entry.megapage;
  out.lsu_resp_dirty[port] = entry.dirty;
  out.lsu_resp_accessed[port] = entry.accessed;
  out.lsu_resp_global[port] = entry.global;
  out.lsu_resp_user[port] = entry.user;
  out.lsu_resp_execute[port] = entry.execute;
  out.lsu_resp_write[port] = entry.write;
  out.lsu_resp_read[port] = entry.read;
  out.lsu_resp_valid_bit[port] = entry.valid;
  out.lsu_resp_pte_valid[port] = entry.pte_valid;
}

void fill_from_lookup_ifu(const TLB_lookup_in_t &lookup_in, TLB_out_t &out) {
  FlatEntry entry;
  entry.vpn1 = lookup_in.ifu_lookup_vpn1;
  entry.vpn0 = lookup_in.ifu_lookup_vpn0;
  entry.ppn1 = lookup_in.ifu_lookup_ppn1;
  entry.ppn0 = lookup_in.ifu_lookup_ppn0;
  entry.asid = lookup_in.ifu_lookup_asid;
  entry.megapage = lookup_in.ifu_lookup_megapage;
  entry.dirty = lookup_in.ifu_lookup_dirty;
  entry.accessed = lookup_in.ifu_lookup_accessed;
  entry.global = lookup_in.ifu_lookup_global;
  entry.user = lookup_in.ifu_lookup_user;
  entry.execute = lookup_in.ifu_lookup_execute;
  entry.write = lookup_in.ifu_lookup_write;
  entry.read = lookup_in.ifu_lookup_read;
  entry.valid = lookup_in.ifu_lookup_valid;
  entry.pte_valid = lookup_in.ifu_lookup_pte_valid;
  fill_ifu_resp(out, lookup_in.ifu_lookup_resp_valid, lookup_in.ifu_lookup_hit,
                lookup_in.ifu_lookup_hit_index, entry);
}

void fill_from_lookup_lsu(const TLB_lookup_in_t &lookup_in, TLB_out_t &out,
                          uint32_t port) {
  FlatEntry entry;
  entry.vpn1 = lookup_in.lsu_lookup_vpn1[port];
  entry.vpn0 = lookup_in.lsu_lookup_vpn0[port];
  entry.ppn1 = lookup_in.lsu_lookup_ppn1[port];
  entry.ppn0 = lookup_in.lsu_lookup_ppn0[port];
  entry.asid = lookup_in.lsu_lookup_asid[port];
  entry.megapage = lookup_in.lsu_lookup_megapage[port];
  entry.dirty = lookup_in.lsu_lookup_dirty[port];
  entry.accessed = lookup_in.lsu_lookup_accessed[port];
  entry.global = lookup_in.lsu_lookup_global[port];
  entry.user = lookup_in.lsu_lookup_user[port];
  entry.execute = lookup_in.lsu_lookup_execute[port];
  entry.write = lookup_in.lsu_lookup_write[port];
  entry.read = lookup_in.lsu_lookup_read[port];
  entry.valid = lookup_in.lsu_lookup_valid[port];
  entry.pte_valid = lookup_in.lsu_lookup_pte_valid[port];
  fill_lsu_resp(out, port, lookup_in.lsu_lookup_resp_valid[port],
                lookup_in.lsu_lookup_hit[port], lookup_in.lsu_lookup_hit_index[port],
                entry);
}

} // namespace

void TLBModule::reset() {
  io = {};
}

void TLBModule::comb() {
  io.out = {};
  io.table_write = {};
  io.reg_write = io.regs;

  const bool lookup_from_input = lookup_from_input_enabled();
  const bool latency_enabled = lookup_latency_enabled() && !lookup_from_input;
  const uint32_t latency = clamp_lookup_latency(MMU_TLB_LOOKUP_LATENCY);

  // Flush handling.
  if (io.in.flush_valid) {
    uint32_t flush_vpn1 = (io.in.flush_vpn >> 10) & 0x3ffu;
    uint32_t flush_vpn0 = io.in.flush_vpn & 0x3ffu;
    for (uint32_t i = 0; i < kTlbEntryNum; ++i) {
      if (!io.regs.entry_pte_valid[i]) {
        continue;
      }
      bool asid_match = (io.in.flush_asid == 0) || (io.regs.entry_asid[i] == io.in.flush_asid);
      bool vpn1_match = (io.regs.entry_vpn1[i] == flush_vpn1);
      bool vpn0_match =
          io.regs.entry_megapage[i] || (io.regs.entry_vpn0[i] == flush_vpn0);
      bool vpn_match = (io.in.flush_vpn == 0) || (vpn1_match && vpn0_match);
      if (asid_match && vpn_match) {
        io.reg_write.entry_pte_valid[i] = false;
      }
    }
  }

  // PTW refill handling.
  bool accept_ptw_write =
      io.in.ptw_write_valid &&
      ((io.in.is_itlb && (io.in.ptw_write_dest == static_cast<wire1_t>(mmu_n::DEST_ITLB))) ||
       (!io.in.is_itlb && (io.in.ptw_write_dest == static_cast<wire1_t>(mmu_n::DEST_DTLB))));
  if (accept_ptw_write) {
    uint32_t victim = io.regs.replace_idx_r % kTlbEntryNum;
    for (uint32_t i = 0; i < kTlbEntryNum; ++i) {
      if (!io.reg_write.entry_pte_valid[i]) {
        victim = i;
        break;
      }
    }
    FlatEntry entry = load_ptw_entry(io.in);
    store_entry(io.reg_write, victim, entry);
    io.reg_write.replace_idx_r = (victim + 1u) % kTlbEntryNum;

    io.table_write.we = true;
    io.table_write.index = victim;
    io.table_write.vpn1 = entry.vpn1;
    io.table_write.vpn0 = entry.vpn0;
    io.table_write.ppn1 = entry.ppn1;
    io.table_write.ppn0 = entry.ppn0;
    io.table_write.asid = entry.asid;
    io.table_write.megapage = entry.megapage;
    io.table_write.dirty = entry.dirty;
    io.table_write.accessed = entry.accessed;
    io.table_write.global = entry.global;
    io.table_write.user = entry.user;
    io.table_write.execute = entry.execute;
    io.table_write.write = entry.write;
    io.table_write.read = entry.read;
    io.table_write.valid_bit = entry.valid;
    io.table_write.pte_valid = entry.pte_valid;
  }

  // IFU request/response.
  io.out.ifu_req_ready = !latency_enabled || !io.regs.ifu_pending_r;
  if (lookup_from_input) {
    if (io.in.ifu_req_valid) {
      fill_from_lookup_ifu(io.lookup_in, io.out);
    }
    io.reg_write.ifu_pending_r = false;
    io.reg_write.ifu_delay_r = 0;
  } else if (!latency_enabled) {
    if (io.in.ifu_req_valid) {
      uint32_t hit_index = 0;
      FlatEntry hit_entry;
      bool hit = lookup_entry(io.regs, io.in.satp_asid, io.in.ifu_req_vtag,
                              hit_index, hit_entry);
      fill_ifu_resp(io.out, true, hit, hit_index, hit_entry);
    }
    io.reg_write.ifu_pending_r = false;
    io.reg_write.ifu_delay_r = 0;
  } else {
    if (io.regs.ifu_pending_r) {
      if (io.regs.ifu_delay_r == 0) {
        uint32_t hit_index = 0;
        FlatEntry hit_entry;
        bool hit = lookup_entry(io.regs, io.in.satp_asid, io.regs.ifu_vtag_r,
                                hit_index, hit_entry);
        fill_ifu_resp(io.out, true, hit, hit_index, hit_entry);
        io.reg_write.ifu_pending_r = false;
        io.reg_write.ifu_delay_r = 0;
      } else {
        io.reg_write.ifu_pending_r = true;
        io.reg_write.ifu_delay_r = io.regs.ifu_delay_r - 1u;
      }
    } else if (io.in.ifu_req_valid) {
      if (latency <= 1u) {
        uint32_t hit_index = 0;
        FlatEntry hit_entry;
        bool hit = lookup_entry(io.regs, io.in.satp_asid, io.in.ifu_req_vtag,
                                hit_index, hit_entry);
        fill_ifu_resp(io.out, true, hit, hit_index, hit_entry);
        io.reg_write.ifu_pending_r = false;
        io.reg_write.ifu_delay_r = 0;
      } else {
        io.reg_write.ifu_pending_r = true;
        io.reg_write.ifu_delay_r = latency - 1u;
        io.reg_write.ifu_vtag_r = io.in.ifu_req_vtag;
        io.reg_write.ifu_op_type_r = io.in.ifu_req_op_type;
      }
    }
  }

  // LSU request/response.
  for (uint32_t i = 0; i < kLsuPorts; ++i) {
    io.out.lsu_req_ready[i] = !latency_enabled || !io.regs.lsu_pending_r[i];
    if (lookup_from_input) {
      if (io.in.lsu_req_valid[i]) {
        fill_from_lookup_lsu(io.lookup_in, io.out, i);
      }
      io.reg_write.lsu_pending_r[i] = false;
      io.reg_write.lsu_delay_r[i] = 0;
      continue;
    }
    if (!latency_enabled) {
      if (io.in.lsu_req_valid[i]) {
        uint32_t hit_index = 0;
        FlatEntry hit_entry;
        bool hit =
            lookup_entry(io.regs, io.in.satp_asid, io.in.lsu_req_vtag[i], hit_index,
                         hit_entry);
        fill_lsu_resp(io.out, i, true, hit, hit_index, hit_entry);
      }
      io.reg_write.lsu_pending_r[i] = false;
      io.reg_write.lsu_delay_r[i] = 0;
      continue;
    }

    if (io.regs.lsu_pending_r[i]) {
      if (io.regs.lsu_delay_r[i] == 0) {
        uint32_t hit_index = 0;
        FlatEntry hit_entry;
        bool hit = lookup_entry(io.regs, io.in.satp_asid, io.regs.lsu_vtag_r[i],
                                hit_index, hit_entry);
        fill_lsu_resp(io.out, i, true, hit, hit_index, hit_entry);
        io.reg_write.lsu_pending_r[i] = false;
        io.reg_write.lsu_delay_r[i] = 0;
      } else {
        io.reg_write.lsu_pending_r[i] = true;
        io.reg_write.lsu_delay_r[i] = io.regs.lsu_delay_r[i] - 1u;
      }
      continue;
    }

    if (!io.in.lsu_req_valid[i]) {
      continue;
    }

    if (latency <= 1u) {
      uint32_t hit_index = 0;
      FlatEntry hit_entry;
      bool hit = lookup_entry(io.regs, io.in.satp_asid, io.in.lsu_req_vtag[i],
                              hit_index, hit_entry);
      fill_lsu_resp(io.out, i, true, hit, hit_index, hit_entry);
      io.reg_write.lsu_pending_r[i] = false;
      io.reg_write.lsu_delay_r[i] = 0;
    } else {
      io.reg_write.lsu_pending_r[i] = true;
      io.reg_write.lsu_delay_r[i] = latency - 1u;
      io.reg_write.lsu_vtag_r[i] = io.in.lsu_req_vtag[i];
      io.reg_write.lsu_op_type_r[i] = io.in.lsu_req_op_type[i];
    }
  }
}

void TLBModule::seq() {
  io.regs = io.reg_write;
}

} // namespace tlb_module_n

