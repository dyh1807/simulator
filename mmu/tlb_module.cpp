#include "include/tlb_module.h"

namespace tlb_module_n {

namespace {

struct FlatEntry {
  wire10_t vpn1 = 0;
  wire10_t vpn0 = 0;
  wire12_t ppn1 = 0;
  wire10_t ppn0 = 0;
  wire9_t asid = 0;
  wire1_t megapage = false;
  wire1_t dirty = false;
  wire1_t accessed = false;
  wire1_t global = false;
  wire1_t user = false;
  wire1_t execute = false;
  wire1_t write = false;
  wire1_t read = false;
  wire1_t valid = false;
  wire1_t pte_valid = false;
};

inline bool lookup_from_input_enabled() {
  return (MMU_TLB_LOOKUP_FROM_INPUT != 0);
}

inline bool delayed_lookup_enabled() { return (MMU_TLB_LOOKUP_LATENCY > 0); }

inline bool ptw_write_matches_tlb(const TLB_in_t &in) {
  if (!in.ptw_write_valid) {
    return false;
  }
  const bool is_itlb_dest =
      in.ptw_write_dest == static_cast<wire1_t>(mmu_n::DEST_ITLB);
  return in.is_itlb ? is_itlb_dest : !is_itlb_dest;
}

FlatEntry ifu_entry_from_lookup(const TLB_lookup_in_t &lookup_in) {
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
  return entry;
}

FlatEntry lsu_entry_from_lookup(const TLB_lookup_in_t &lookup_in, uint32_t port) {
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
  return entry;
}

void fill_ifu_resp(TLB_out_t &out, wire1_t resp_valid, wire1_t hit, wire6_t hit_index,
                   const FlatEntry &entry) {
  out.ifu_resp_valid = resp_valid;
  out.ifu_resp_hit = hit;
  out.ifu_resp_hit_index = hit_index;
  if (!resp_valid || !hit) {
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

void fill_lsu_resp(TLB_out_t &out, uint32_t port, wire1_t resp_valid, wire1_t hit,
                   wire6_t hit_index, const FlatEntry &entry) {
  out.lsu_resp_valid[port] = resp_valid;
  out.lsu_resp_hit[port] = hit;
  out.lsu_resp_hit_index[port] = hit_index;
  if (!resp_valid || !hit) {
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

void clear_pending_state(TLB_reg_write_t &reg_write) {
  reg_write.ifu_pending_r = false;
  reg_write.ifu_vtag_r = 0;
  reg_write.ifu_op_type_r = 0;
  for (uint32_t i = 0; i < kLsuPorts; ++i) {
    reg_write.lsu_pending_r[i] = false;
    reg_write.lsu_vtag_r[i] = 0;
    reg_write.lsu_op_type_r[i] = 0;
  }
}

} // namespace

void TLBModule::reset() { io = {}; }

void TLBModule::comb() {
  io.out = {};
  io.table_write = {};
  io.reg_write = io.regs;

  const bool lookup_from_input = lookup_from_input_enabled();
  const bool delayed_lookup = delayed_lookup_enabled();
  const bool flush = io.in.flush_valid;

  // Forward flush command to external table backend.
  io.table_write.flush_valid = io.in.flush_valid;
  io.table_write.flush_vpn = io.in.flush_vpn;
  io.table_write.flush_asid = io.in.flush_asid;

  // PTW refill command turns into a table write request.
  if (ptw_write_matches_tlb(io.in)) {
    wire6_t victim = io.lookup_in.refill_victim_valid
                         ? io.lookup_in.refill_victim_index
                         : static_cast<wire6_t>(io.regs.replace_idx_r % kTlbEntryNum);
    io.table_write.we = true;
    io.table_write.index = victim;
    io.table_write.vpn1 = io.in.ptw_write_vpn1;
    io.table_write.vpn0 = io.in.ptw_write_vpn0;
    io.table_write.ppn1 = io.in.ptw_write_ppn1;
    io.table_write.ppn0 = io.in.ptw_write_ppn0;
    io.table_write.asid = io.in.ptw_write_asid;
    io.table_write.megapage = io.in.ptw_write_megapage;
    io.table_write.dirty = io.in.ptw_write_dirty;
    io.table_write.accessed = io.in.ptw_write_accessed;
    io.table_write.global = io.in.ptw_write_global;
    io.table_write.user = io.in.ptw_write_user;
    io.table_write.execute = io.in.ptw_write_execute;
    io.table_write.write = io.in.ptw_write_write;
    io.table_write.read = io.in.ptw_write_read;
    io.table_write.valid_bit = io.in.ptw_write_valid_bit;
    io.table_write.pte_valid = io.in.ptw_write_pte_valid;
    io.reg_write.replace_idx_r =
        static_cast<reg6_t>((static_cast<uint32_t>(victim) + 1u) % kTlbEntryNum);
  }

  if (flush) {
    clear_pending_state(io.reg_write);
  }

  // IFU channel.
  io.out.ifu_req_ready = lookup_from_input ? true : !io.regs.ifu_pending_r;
  if (lookup_from_input) {
    if (!flush && io.in.ifu_req_valid) {
      FlatEntry entry = ifu_entry_from_lookup(io.lookup_in);
      fill_ifu_resp(io.out, io.lookup_in.ifu_lookup_resp_valid,
                    io.lookup_in.ifu_lookup_hit,
                    io.lookup_in.ifu_lookup_hit_index, entry);
    }
    io.reg_write.ifu_pending_r = false;
    io.reg_write.ifu_vtag_r = 0;
    io.reg_write.ifu_op_type_r = 0;
  } else if (!flush) {
    if (io.regs.ifu_pending_r) {
      if (io.lookup_in.ifu_lookup_resp_valid) {
        FlatEntry entry = ifu_entry_from_lookup(io.lookup_in);
        fill_ifu_resp(io.out, true, io.lookup_in.ifu_lookup_hit,
                      io.lookup_in.ifu_lookup_hit_index, entry);
        io.reg_write.ifu_pending_r = false;
      }
    } else if (io.in.ifu_req_valid) {
      if (!delayed_lookup && io.lookup_in.ifu_lookup_resp_valid) {
        FlatEntry entry = ifu_entry_from_lookup(io.lookup_in);
        fill_ifu_resp(io.out, true, io.lookup_in.ifu_lookup_hit,
                      io.lookup_in.ifu_lookup_hit_index, entry);
      } else {
        io.reg_write.ifu_pending_r = true;
        io.reg_write.ifu_vtag_r = io.in.ifu_req_vtag;
        io.reg_write.ifu_op_type_r = io.in.ifu_req_op_type;
      }
    }
  }

  // LSU channels.
  for (uint32_t i = 0; i < kLsuPorts; ++i) {
    io.out.lsu_req_ready[i] = lookup_from_input ? true : !io.regs.lsu_pending_r[i];
    if (lookup_from_input) {
      if (!flush && io.in.lsu_req_valid[i]) {
        FlatEntry entry = lsu_entry_from_lookup(io.lookup_in, i);
        fill_lsu_resp(io.out, i, io.lookup_in.lsu_lookup_resp_valid[i],
                      io.lookup_in.lsu_lookup_hit[i],
                      io.lookup_in.lsu_lookup_hit_index[i], entry);
      }
      io.reg_write.lsu_pending_r[i] = false;
      io.reg_write.lsu_vtag_r[i] = 0;
      io.reg_write.lsu_op_type_r[i] = 0;
      continue;
    }

    if (flush) {
      continue;
    }

    if (io.regs.lsu_pending_r[i]) {
      if (io.lookup_in.lsu_lookup_resp_valid[i]) {
        FlatEntry entry = lsu_entry_from_lookup(io.lookup_in, i);
        fill_lsu_resp(io.out, i, true, io.lookup_in.lsu_lookup_hit[i],
                      io.lookup_in.lsu_lookup_hit_index[i], entry);
        io.reg_write.lsu_pending_r[i] = false;
      }
      continue;
    }

    if (!io.in.lsu_req_valid[i]) {
      continue;
    }

    if (!delayed_lookup && io.lookup_in.lsu_lookup_resp_valid[i]) {
      FlatEntry entry = lsu_entry_from_lookup(io.lookup_in, i);
      fill_lsu_resp(io.out, i, true, io.lookup_in.lsu_lookup_hit[i],
                    io.lookup_in.lsu_lookup_hit_index[i], entry);
    } else {
      io.reg_write.lsu_pending_r[i] = true;
      io.reg_write.lsu_vtag_r[i] = io.in.lsu_req_vtag[i];
      io.reg_write.lsu_op_type_r[i] = io.in.lsu_req_op_type[i];
    }
  }
}

void TLBModule::seq() { io.regs = io.reg_write; }

} // namespace tlb_module_n
