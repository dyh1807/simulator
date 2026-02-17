#include <MMU.h>
#include <config.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#ifdef USE_SIM_DDR
#include <MemorySubsystem.h>
#endif

using namespace mmu_n;

namespace {

constexpr bool kDelayedLookup = (MMU_TLB_LOOKUP_LATENCY > 0);
constexpr bool kLookupFromInput = (MMU_TLB_LOOKUP_FROM_INPUT != 0);
constexpr reg16_t kLookupDelayInit =
    kDelayedLookup ? static_cast<reg16_t>(MMU_TLB_LOOKUP_LATENCY - 1) : 0;

template <size_t N> struct LookupResult {
  bool hit = false;
  wire6_t hit_index = 0;
  TLBEntry entry = {};
};

template <size_t N>
LookupResult<N> lookup_table(const std::array<TLBEntry, N> &table, wire20_t vtag,
                             wire9_t asid) {
  LookupResult<N> result;
  const wire10_t vpn1 = (vtag >> 10) & 0x3ffu;
  const wire10_t vpn0 = vtag & 0x3ffu;
  int hit_index = -1;

  for (size_t i = 0; i < N; ++i) {
    const TLBEntry &entry = table[i];
    if (!entry.pte_valid) {
      continue;
    }
    if (entry.vpn1 != vpn1 || entry.asid != asid) {
      continue;
    }
    if (!entry.megapage && entry.vpn0 != vpn0) {
      continue;
    }
    if (hit_index >= 0) {
      std::cerr << "MMU Fatal Error: multiple TLB entries matched one lookup\n";
      std::cerr << "  vtag=0x" << std::hex << vtag << " asid=0x" << asid
                << std::dec << "\n";
      std::exit(1);
    }
    hit_index = static_cast<int>(i);
  }

  if (hit_index >= 0) {
    result.hit = true;
    result.hit_index = static_cast<wire6_t>(hit_index);
    result.entry = table[static_cast<size_t>(hit_index)];
  }
  return result;
}

inline void fill_lookup_entry_fields(const TLBEntry &entry,
                                     tlb_module_n::TLB_lookup_in_t &lookup_in,
                                     bool ifu, uint32_t port = 0) {
  if (ifu) {
    lookup_in.ifu_lookup_vpn1 = static_cast<wire10_t>(entry.vpn1);
    lookup_in.ifu_lookup_vpn0 = static_cast<wire10_t>(entry.vpn0);
    lookup_in.ifu_lookup_ppn1 = static_cast<wire12_t>(entry.ppn1);
    lookup_in.ifu_lookup_ppn0 = static_cast<wire10_t>(entry.ppn0);
    lookup_in.ifu_lookup_asid = static_cast<wire9_t>(entry.asid);
    lookup_in.ifu_lookup_megapage = entry.megapage;
    lookup_in.ifu_lookup_dirty = entry.dirty;
    lookup_in.ifu_lookup_accessed = entry.accessed;
    lookup_in.ifu_lookup_global = entry.global;
    lookup_in.ifu_lookup_user = entry.user;
    lookup_in.ifu_lookup_execute = entry.execute;
    lookup_in.ifu_lookup_write = entry.write;
    lookup_in.ifu_lookup_read = entry.read;
    lookup_in.ifu_lookup_valid = entry.valid;
    lookup_in.ifu_lookup_pte_valid = entry.pte_valid;
    return;
  }

  lookup_in.lsu_lookup_vpn1[port] = static_cast<wire10_t>(entry.vpn1);
  lookup_in.lsu_lookup_vpn0[port] = static_cast<wire10_t>(entry.vpn0);
  lookup_in.lsu_lookup_ppn1[port] = static_cast<wire12_t>(entry.ppn1);
  lookup_in.lsu_lookup_ppn0[port] = static_cast<wire10_t>(entry.ppn0);
  lookup_in.lsu_lookup_asid[port] = static_cast<wire9_t>(entry.asid);
  lookup_in.lsu_lookup_megapage[port] = entry.megapage;
  lookup_in.lsu_lookup_dirty[port] = entry.dirty;
  lookup_in.lsu_lookup_accessed[port] = entry.accessed;
  lookup_in.lsu_lookup_global[port] = entry.global;
  lookup_in.lsu_lookup_user[port] = entry.user;
  lookup_in.lsu_lookup_execute[port] = entry.execute;
  lookup_in.lsu_lookup_write[port] = entry.write;
  lookup_in.lsu_lookup_read[port] = entry.read;
  lookup_in.lsu_lookup_valid[port] = entry.valid;
  lookup_in.lsu_lookup_pte_valid[port] = entry.pte_valid;
}

inline void fill_lookup_entry_fields_from_external(
    const mmu_tlb_lookup_entry_t &entry, tlb_module_n::TLB_lookup_in_t &lookup_in,
    bool ifu, uint32_t port = 0) {
  if (ifu) {
    lookup_in.ifu_lookup_vpn1 = static_cast<wire10_t>(entry.vpn1);
    lookup_in.ifu_lookup_vpn0 = static_cast<wire10_t>(entry.vpn0);
    lookup_in.ifu_lookup_ppn1 = static_cast<wire12_t>(entry.ppn1);
    lookup_in.ifu_lookup_ppn0 = static_cast<wire10_t>(entry.ppn0);
    lookup_in.ifu_lookup_asid = static_cast<wire9_t>(entry.asid);
    lookup_in.ifu_lookup_megapage = entry.megapage;
    lookup_in.ifu_lookup_dirty = entry.dirty;
    lookup_in.ifu_lookup_accessed = entry.accessed;
    lookup_in.ifu_lookup_global = entry.global;
    lookup_in.ifu_lookup_user = entry.user;
    lookup_in.ifu_lookup_execute = entry.execute;
    lookup_in.ifu_lookup_write = entry.write;
    lookup_in.ifu_lookup_read = entry.read;
    lookup_in.ifu_lookup_valid = entry.valid;
    lookup_in.ifu_lookup_pte_valid = entry.pte_valid;
    return;
  }

  lookup_in.lsu_lookup_vpn1[port] = static_cast<wire10_t>(entry.vpn1);
  lookup_in.lsu_lookup_vpn0[port] = static_cast<wire10_t>(entry.vpn0);
  lookup_in.lsu_lookup_ppn1[port] = static_cast<wire12_t>(entry.ppn1);
  lookup_in.lsu_lookup_ppn0[port] = static_cast<wire10_t>(entry.ppn0);
  lookup_in.lsu_lookup_asid[port] = static_cast<wire9_t>(entry.asid);
  lookup_in.lsu_lookup_megapage[port] = entry.megapage;
  lookup_in.lsu_lookup_dirty[port] = entry.dirty;
  lookup_in.lsu_lookup_accessed[port] = entry.accessed;
  lookup_in.lsu_lookup_global[port] = entry.global;
  lookup_in.lsu_lookup_user[port] = entry.user;
  lookup_in.lsu_lookup_execute[port] = entry.execute;
  lookup_in.lsu_lookup_write[port] = entry.write;
  lookup_in.lsu_lookup_read[port] = entry.read;
  lookup_in.lsu_lookup_valid[port] = entry.valid;
  lookup_in.lsu_lookup_pte_valid[port] = entry.pte_valid;
}

inline TLBEntry ifu_resp_to_entry(const tlb_module_n::TLB_out_t &out) {
  TLBEntry entry = {};
  entry.vpn1 = out.ifu_resp_vpn1;
  entry.vpn0 = out.ifu_resp_vpn0;
  entry.ppn1 = out.ifu_resp_ppn1;
  entry.ppn0 = out.ifu_resp_ppn0;
  entry.asid = out.ifu_resp_asid;
  entry.megapage = out.ifu_resp_megapage;
  entry.dirty = out.ifu_resp_dirty;
  entry.accessed = out.ifu_resp_accessed;
  entry.global = out.ifu_resp_global;
  entry.user = out.ifu_resp_user;
  entry.execute = out.ifu_resp_execute;
  entry.write = out.ifu_resp_write;
  entry.read = out.ifu_resp_read;
  entry.valid = out.ifu_resp_valid_bit;
  entry.pte_valid = out.ifu_resp_pte_valid;
  return entry;
}

inline TLBEntry lsu_resp_to_entry(const tlb_module_n::TLB_out_t &out,
                                  uint32_t port) {
  TLBEntry entry = {};
  entry.vpn1 = out.lsu_resp_vpn1[port];
  entry.vpn0 = out.lsu_resp_vpn0[port];
  entry.ppn1 = out.lsu_resp_ppn1[port];
  entry.ppn0 = out.lsu_resp_ppn0[port];
  entry.asid = out.lsu_resp_asid[port];
  entry.megapage = out.lsu_resp_megapage[port];
  entry.dirty = out.lsu_resp_dirty[port];
  entry.accessed = out.lsu_resp_accessed[port];
  entry.global = out.lsu_resp_global[port];
  entry.user = out.lsu_resp_user[port];
  entry.execute = out.lsu_resp_execute[port];
  entry.write = out.lsu_resp_write[port];
  entry.read = out.lsu_resp_read[port];
  entry.valid = out.lsu_resp_valid_bit[port];
  entry.pte_valid = out.lsu_resp_pte_valid[port];
  return entry;
}

template <size_t N>
void apply_tlb_table_write(std::array<TLBEntry, N> &table,
                           const tlb_module_n::TLB_table_write_t &tw) {
  if (tw.flush_valid) {
    const wire10_t flush_vpn1 = (tw.flush_vpn >> 10) & 0x3ffu;
    const wire10_t flush_vpn0 = tw.flush_vpn & 0x3ffu;
    for (size_t i = 0; i < N; ++i) {
      TLBEntry &entry = table[i];
      if (!entry.pte_valid) {
        continue;
      }
      const bool asid_match =
          (tw.flush_asid == 0) || (entry.asid == static_cast<uint32_t>(tw.flush_asid));
      const bool vpn1_match = (entry.vpn1 == flush_vpn1);
      const bool vpn0_match = entry.megapage || (entry.vpn0 == flush_vpn0);
      const bool vpn_match = (tw.flush_vpn == 0) || (vpn1_match && vpn0_match);
      if (asid_match && vpn_match) {
        entry.pte_valid = false;
      }
    }
  }

  if (!tw.we) {
    return;
  }

  const size_t index = static_cast<size_t>(tw.index);
  if (index >= N) {
    std::cerr << "MMU Fatal Error: TLB write index out of range index=" << index
              << " size=" << N << "\n";
    std::exit(1);
  }

  TLBEntry entry = {};
  entry.vpn1 = tw.vpn1;
  entry.vpn0 = tw.vpn0;
  entry.ppn1 = tw.ppn1;
  entry.ppn0 = tw.ppn0;
  entry.asid = tw.asid;
  entry.megapage = tw.megapage;
  entry.dirty = tw.dirty;
  entry.accessed = tw.accessed;
  entry.global = tw.global;
  entry.user = tw.user;
  entry.execute = tw.execute;
  entry.write = tw.write;
  entry.read = tw.read;
  entry.valid = tw.valid_bit;
  entry.pte_valid = tw.pte_valid;
  table[index] = entry;
}

inline void update_lookup_delay_counter(reg1_t prev_pending, reg1_t next_pending,
                                        reg16_t &delay_counter) {
  if (!kDelayedLookup) {
    delay_counter = 0;
    return;
  }

  if (!next_pending) {
    delay_counter = 0;
    return;
  }

  if (!prev_pending && next_pending) {
    delay_counter = kLookupDelayInit;
    return;
  }

  if (delay_counter > 0) {
    --delay_counter;
  }
}

template <size_t N>
wire6_t pick_refill_victim(const std::array<TLBEntry, N> &table, reg6_t rr_index) {
  for (size_t i = 0; i < N; ++i) {
    if (!table[i].pte_valid) {
      return static_cast<wire6_t>(i);
    }
  }
  return static_cast<wire6_t>(static_cast<uint32_t>(rr_index) % N);
}

} // namespace

MMU::MMU() : io{}, itlb_mod{}, dtlb_mod{}, ptw_mod{}, itlb_table{}, dtlb_table{},
             itlb_ifu_lookup_delay_r(0), tlb2ptw_frontend{}, tlb2ptw_backend{},
             tlb2ptw{}, mem_sim{}, resp_ifu_r_1{}, resp_lsu_r_1{} {
  itlb_refill_idx_r = 0;
  dtlb_refill_idx_r = 0;
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    dtlb_lsu_lookup_delay_r[i] = 0;
  }
  reset();
}

void MMU::reset() {
  io = {};
  io.in.state.privilege = M_MODE;

  i_tlb = {};
  d_tlb = {};
  tlb2ptw = {};
  tlb2ptw_frontend = {};
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    tlb2ptw_backend[i] = {};
    resp_lsu_r_1[i] = {};
    io.out.mmu_lsu_resp[i] = {};
    dtlb_lsu_lookup_delay_r[i] = 0;
  }
  resp_ifu_r_1 = {};
  io.out.mmu_ifu_resp = {};

  mem_sim = {};
  itlb_ifu_lookup_delay_r = 0;
  itlb_refill_idx_r = 0;
  dtlb_refill_idx_r = 0;

  for (auto &entry : itlb_table) {
    entry = {};
  }
  for (auto &entry : dtlb_table) {
    entry = {};
  }

  itlb_mod.reset();
  dtlb_mod.reset();
  ptw_mod.reset();
}

void MMU::comb_frontend() {
  resp_ifu_r_1 = {};
  tlb2ptw_frontend = {};

  if (io.in.state.satp.mode == MODE_BARE || io.in.state.privilege == M_MODE) {
    io.out.mmu_ifu_req.ready = true;
    if (io.in.mmu_ifu_req.op_type != OP_FETCH && io.in.mmu_ifu_req.valid) {
      std::cout << "MMU comb_frontend received non-FETCH request in Bare mode. cycle: "
                << sim_time << std::endl;
      std::exit(1);
    }
    const uint32_t ptag = io.in.mmu_ifu_req.vtag;
    resp_ifu_r_1 = {
        .valid = io.in.mmu_ifu_req.valid,
        .excp = false,
        .miss = false,
        .ptag = ptag,
    };
    return;
  }

  auto &in = itlb_mod.io.in;
  in = {};
  in.is_itlb = true;
  in.satp_asid = static_cast<wire9_t>(io.in.state.satp.asid);
  in.ifu_req_valid = io.in.mmu_ifu_req.valid;
  in.ifu_req_vtag = static_cast<wire20_t>(io.in.mmu_ifu_req.vtag);
  in.ifu_req_op_type = static_cast<wire2_t>(io.in.mmu_ifu_req.op_type);
  in.flush_valid = io.in.tlb_flush.flush_valid;
  in.flush_vpn = static_cast<wire20_t>(io.in.tlb_flush.flush_vpn);
  in.flush_asid = static_cast<wire9_t>(io.in.tlb_flush.flush_asid);

  auto &lookup_in = itlb_mod.io.lookup_in;
  lookup_in = {};
  lookup_in.refill_victim_valid = io.in.itlb_lookup_in.refill_victim_valid;
  lookup_in.refill_victim_index =
      static_cast<wire6_t>(io.in.itlb_lookup_in.refill_victim_index);
  if (kLookupFromInput) {
    if (in.ifu_req_valid) {
      lookup_in.ifu_lookup_resp_valid = io.in.itlb_lookup_in.lookup_resp_valid;
      lookup_in.ifu_lookup_hit = io.in.itlb_lookup_in.lookup_hit;
      lookup_in.ifu_lookup_hit_index =
          static_cast<wire6_t>(io.in.itlb_lookup_in.lookup_hit_index);
      if (io.in.itlb_lookup_in.lookup_hit) {
        fill_lookup_entry_fields_from_external(io.in.itlb_lookup_in.lookup_entry,
                                               lookup_in, true);
      }
    }
  } else if (!kDelayedLookup) {
    if (in.ifu_req_valid) {
      auto result = lookup_table(itlb_table, in.ifu_req_vtag, in.satp_asid);
      lookup_in.ifu_lookup_resp_valid = true;
      lookup_in.ifu_lookup_hit = result.hit;
      lookup_in.ifu_lookup_hit_index = result.hit_index;
      if (result.hit) {
        fill_lookup_entry_fields(result.entry, lookup_in, true);
      }
    }
  } else if (itlb_mod.io.regs.ifu_pending_r && itlb_ifu_lookup_delay_r == 0) {
    auto result = lookup_table(itlb_table, itlb_mod.io.regs.ifu_vtag_r, in.satp_asid);
    lookup_in.ifu_lookup_resp_valid = true;
    lookup_in.ifu_lookup_hit = result.hit;
    lookup_in.ifu_lookup_hit_index = result.hit_index;
    if (result.hit) {
      fill_lookup_entry_fields(result.entry, lookup_in, true);
    }
  }

  itlb_mod.comb();

  io.out.mmu_ifu_req.ready = itlb_mod.io.out.ifu_req_ready;
  if (io.in.mmu_ifu_req.valid) {
    i_tlb.access_count++;
  }

  if (!itlb_mod.io.out.ifu_resp_valid) {
    return;
  }

  if (itlb_mod.io.out.ifu_resp_hit) {
    TLBEntry entry = ifu_resp_to_entry(itlb_mod.io.out);
    const bool page_fault = entry.is_page_fault(OP_FETCH, io.in.state.mstatus,
                                                io.in.state.sstatus,
                                                io.in.state.privilege);
    uint32_t ptag = entry.get_ppn();
    if (entry.megapage) {
      const uint32_t vpn0 = io.in.mmu_ifu_req.vtag & 0x3ffu;
      ptag = (ptag & 0xFFFFFC00u) | vpn0;
    }
    resp_ifu_r_1 = {
        .valid = true,
        .excp = page_fault,
        .miss = false,
        .ptag = ptag,
    };
    i_tlb.hit_count++;
    return;
  }

  resp_ifu_r_1 = {
      .valid = true,
      .excp = false,
      .miss = true,
      .ptag = 0,
  };

  wire20_t miss_vtag = io.in.mmu_ifu_req.vtag;
  wire2_t miss_op = static_cast<wire2_t>(io.in.mmu_ifu_req.op_type);
  if (kDelayedLookup && itlb_mod.io.regs.ifu_pending_r) {
    miss_vtag = itlb_mod.io.regs.ifu_vtag_r;
    miss_op = itlb_mod.io.regs.ifu_op_type_r;
  }

  tlb2ptw_frontend = {
      .tlb_miss = true,
      .vpn1 = static_cast<uint32_t>((miss_vtag >> 10) & 0x3ffu),
      .vpn0 = static_cast<uint32_t>(miss_vtag & 0x3ffu),
      .op_type = static_cast<MMU_OP_TYPE>(miss_op),
  };
}

void MMU::comb_backend() {
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    resp_lsu_r_1[i] = {};
    tlb2ptw_backend[i] = {};
  }

  if (io.in.state.satp.mode == MODE_BARE || io.in.state.privilege == M_MODE) {
    for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
      io.out.mmu_lsu_req[i].ready = true;
      if (io.in.mmu_lsu_req[i].valid && io.in.mmu_lsu_req[i].op_type != OP_LOAD &&
          io.in.mmu_lsu_req[i].op_type != OP_STORE) {
        std::cout << "MMU comb_backend received invalid LSU op in Bare mode. cycle: "
                  << sim_time << std::endl;
        std::exit(1);
      }
      resp_lsu_r_1[i] = {
          .valid = io.in.mmu_lsu_req[i].valid,
          .excp = false,
          .miss = false,
          .ptag = io.in.mmu_lsu_req[i].vtag,
      };
    }
    return;
  }

  auto &in = dtlb_mod.io.in;
  in = {};
  in.is_itlb = false;
  in.satp_asid = static_cast<wire9_t>(io.in.state.satp.asid);
  in.flush_valid = io.in.tlb_flush.flush_valid;
  in.flush_vpn = static_cast<wire20_t>(io.in.tlb_flush.flush_vpn);
  in.flush_asid = static_cast<wire9_t>(io.in.tlb_flush.flush_asid);
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    in.lsu_req_valid[i] = io.in.mmu_lsu_req[i].valid;
    in.lsu_req_vtag[i] = static_cast<wire20_t>(io.in.mmu_lsu_req[i].vtag);
    in.lsu_req_op_type[i] = static_cast<wire2_t>(io.in.mmu_lsu_req[i].op_type);
  }

  auto &lookup_in = dtlb_mod.io.lookup_in;
  lookup_in = {};
  lookup_in.refill_victim_valid = io.in.dtlb_lookup_in.refill_victim_valid;
  lookup_in.refill_victim_index =
      static_cast<wire6_t>(io.in.dtlb_lookup_in.refill_victim_index);
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    if (kLookupFromInput) {
      if (!in.lsu_req_valid[i]) {
        continue;
      }
      lookup_in.lsu_lookup_resp_valid[i] = io.in.dtlb_lookup_in.lookup_resp_valid[i];
      lookup_in.lsu_lookup_hit[i] = io.in.dtlb_lookup_in.lookup_hit[i];
      lookup_in.lsu_lookup_hit_index[i] =
          static_cast<wire6_t>(io.in.dtlb_lookup_in.lookup_hit_index[i]);
      if (io.in.dtlb_lookup_in.lookup_hit[i]) {
        fill_lookup_entry_fields_from_external(io.in.dtlb_lookup_in.lookup_entry[i],
                                               lookup_in, false, i);
      }
      continue;
    }

    if (!kDelayedLookup) {
      if (!in.lsu_req_valid[i]) {
        continue;
      }
      auto result = lookup_table(dtlb_table, in.lsu_req_vtag[i], in.satp_asid);
      lookup_in.lsu_lookup_resp_valid[i] = true;
      lookup_in.lsu_lookup_hit[i] = result.hit;
      lookup_in.lsu_lookup_hit_index[i] = result.hit_index;
      if (result.hit) {
        fill_lookup_entry_fields(result.entry, lookup_in, false, i);
      }
      continue;
    }

    if (dtlb_mod.io.regs.lsu_pending_r[i] && dtlb_lsu_lookup_delay_r[i] == 0) {
      auto result = lookup_table(dtlb_table, dtlb_mod.io.regs.lsu_vtag_r[i], in.satp_asid);
      lookup_in.lsu_lookup_resp_valid[i] = true;
      lookup_in.lsu_lookup_hit[i] = result.hit;
      lookup_in.lsu_lookup_hit_index[i] = result.hit_index;
      if (result.hit) {
        fill_lookup_entry_fields(result.entry, lookup_in, false, i);
      }
    }
  }

  dtlb_mod.comb();

  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    io.out.mmu_lsu_req[i].ready = dtlb_mod.io.out.lsu_req_ready[i];
    if (io.in.mmu_lsu_req[i].valid) {
      d_tlb.access_count++;
    }

    if (!dtlb_mod.io.out.lsu_resp_valid[i]) {
      continue;
    }

    if (dtlb_mod.io.out.lsu_resp_hit[i]) {
      TLBEntry entry = lsu_resp_to_entry(dtlb_mod.io.out, i);

      wire2_t op_type = static_cast<wire2_t>(io.in.mmu_lsu_req[i].op_type);
      wire20_t req_vtag = static_cast<wire20_t>(io.in.mmu_lsu_req[i].vtag);
      if (kDelayedLookup && dtlb_mod.io.regs.lsu_pending_r[i]) {
        op_type = dtlb_mod.io.regs.lsu_op_type_r[i];
        req_vtag = dtlb_mod.io.regs.lsu_vtag_r[i];
      }

      const bool page_fault = entry.is_page_fault(op_type, io.in.state.mstatus,
                                                  io.in.state.sstatus,
                                                  io.in.state.privilege);
      uint32_t ptag = entry.get_ppn();
      if (entry.megapage) {
        const uint32_t vpn0 = req_vtag & 0x3ffu;
        ptag = (ptag & 0xFFFFFC00u) | vpn0;
      }
      resp_lsu_r_1[i] = {
          .valid = true,
          .excp = page_fault,
          .miss = false,
          .ptag = ptag,
      };
      d_tlb.hit_count++;
      continue;
    }

    resp_lsu_r_1[i] = {
        .valid = true,
        .excp = false,
        .miss = true,
        .ptag = 0,
    };

    wire20_t miss_vtag = static_cast<wire20_t>(io.in.mmu_lsu_req[i].vtag);
    wire2_t miss_op = static_cast<wire2_t>(io.in.mmu_lsu_req[i].op_type);
    if (kDelayedLookup && dtlb_mod.io.regs.lsu_pending_r[i]) {
      miss_vtag = dtlb_mod.io.regs.lsu_vtag_r[i];
      miss_op = dtlb_mod.io.regs.lsu_op_type_r[i];
    }

    tlb2ptw_backend[i] = {
        .tlb_miss = true,
        .vpn1 = static_cast<uint32_t>((miss_vtag >> 10) & 0x3ffu),
        .vpn0 = static_cast<uint32_t>(miss_vtag & 0x3ffu),
        .op_type = static_cast<MMU_OP_TYPE>(miss_op),
    };
  }
}

void MMU::comb_arbiter() {
  tlb2ptw = {};
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    if (tlb2ptw_backend[i].tlb_miss) {
      tlb2ptw = tlb2ptw_backend[i];
      return;
    }
  }
  if (tlb2ptw_frontend.tlb_miss) {
    tlb2ptw = tlb2ptw_frontend;
  }
}

extern uint32_t *p_memory;
extern long long sim_time;

void MMU::comb_memory() {
#ifdef USE_SIM_DDR
  auto &port = mem_subsystem().mmu_port();
  io.in.mmu_mem_req.ready = port.req.ready;
  io.in.mmu_mem_resp.valid = port.resp.valid;
  io.in.mmu_mem_resp.data = port.resp.data[0];
#else
  io.in.mmu_mem_req.ready = !mem_sim.busy;
  io.in.mmu_mem_resp.valid = (mem_sim.busy && mem_sim.count >= PTW_MEM_LATENCY);
  io.in.mmu_mem_resp.data = mem_sim.data;
#endif
}

void MMU::comb_ptw() {
#ifndef CONFIG_CACHE
  io.in.mmu_dcache_req.ready = true;
  io.in.mmu_dcache_resp.valid = true;
  io.in.mmu_dcache_resp.miss = true;
#endif

  comb_arbiter();
  comb_memory();

  auto &ptw_in = ptw_mod.io.in;
  ptw_in = {};
  ptw_in.tlb_miss = tlb2ptw.tlb_miss;
  ptw_in.tlb_vpn1 = static_cast<wire10_t>(tlb2ptw.vpn1);
  ptw_in.tlb_vpn0 = static_cast<wire10_t>(tlb2ptw.vpn0);
  ptw_in.tlb_op_type = static_cast<wire2_t>(tlb2ptw.op_type);
  ptw_in.satp_ppn = static_cast<wire22_t>(io.in.state.satp.ppn);
  ptw_in.satp_asid = static_cast<wire9_t>(io.in.state.satp.asid);
  ptw_in.mstatus = io.in.state.mstatus;
  ptw_in.sstatus = io.in.state.sstatus;
  ptw_in.privilege = static_cast<wire2_t>(io.in.state.privilege);
  ptw_in.dcache_req_ready = io.in.mmu_dcache_req.ready;
  ptw_in.dcache_resp_valid = io.in.mmu_dcache_resp.valid;
  ptw_in.dcache_resp_miss = io.in.mmu_dcache_resp.miss;
  ptw_in.dcache_resp_data = io.in.mmu_dcache_resp.data;
  ptw_in.mem_req_ready = io.in.mmu_mem_req.ready;
  ptw_in.mem_resp_valid = io.in.mmu_mem_resp.valid;
  ptw_in.mem_resp_data = io.in.mmu_mem_resp.data;

  ptw_mod.comb();

  io.out.mmu_dcache_req.valid = ptw_mod.io.out.dcache_req_valid;
  io.out.mmu_dcache_req.paddr = ptw_mod.io.out.dcache_req_paddr;
  io.out.mmu_dcache_resp.ready = ptw_mod.io.out.dcache_resp_ready;
  io.out.mmu_mem_req.valid = ptw_mod.io.out.mem_req_valid;
  io.out.mmu_mem_req.paddr = ptw_mod.io.out.mem_req_paddr;
  io.out.mmu_mem_resp.ready = ptw_mod.io.out.mem_resp_ready;

#ifdef USE_SIM_DDR
  auto &port = mem_subsystem().mmu_port();
  port.req.valid = io.out.mmu_mem_req.valid;
  port.req.addr = io.out.mmu_mem_req.paddr;
  port.req.total_size = 3; // 4B PTE
  port.req.id = 0;
  port.resp.ready = io.out.mmu_mem_resp.ready;
#endif

}

void MMU::seq() {
  io.out.mmu_ifu_resp = resp_ifu_r_1;
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    io.out.mmu_lsu_resp[i] = resp_lsu_r_1[i];
  }

#ifndef USE_SIM_DDR
  if (!mem_sim.busy) {
    if (io.out.mmu_mem_req.valid) {
      mem_sim.busy = true;
      mem_sim.count = 0;
      mem_sim.addr = io.out.mmu_mem_req.paddr;
      mem_sim.data = p_memory[mem_sim.addr >> 2];
    }
  } else {
    if (mem_sim.count < PTW_MEM_LATENCY) {
      mem_sim.count++;
    } else if (io.out.mmu_mem_resp.ready) {
      mem_sim.busy = false;
    }
  }
#endif

  const reg1_t itlb_prev_pending = itlb_mod.io.regs.ifu_pending_r;
  const reg1_t itlb_next_pending = itlb_mod.io.reg_write.ifu_pending_r;
  reg1_t dtlb_prev_pending[MAX_LSU_REQ_NUM] = {false};
  reg1_t dtlb_next_pending[MAX_LSU_REQ_NUM] = {false};
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    dtlb_prev_pending[i] = dtlb_mod.io.regs.lsu_pending_r[i];
    dtlb_next_pending[i] = dtlb_mod.io.reg_write.lsu_pending_r[i];
  }

  itlb_mod.seq();
  dtlb_mod.seq();
  ptw_mod.seq();

  update_lookup_delay_counter(itlb_prev_pending, itlb_next_pending,
                              itlb_ifu_lookup_delay_r);
  for (int i = 0; i < MAX_LSU_REQ_NUM; ++i) {
    update_lookup_delay_counter(dtlb_prev_pending[i], dtlb_next_pending[i],
                                dtlb_lsu_lookup_delay_r[i]);
  }

  apply_tlb_table_write(itlb_table, itlb_mod.io.table_write);
  apply_tlb_table_write(dtlb_table, dtlb_mod.io.table_write);

  if (ptw_mod.io.table_write.we) {
    tlb_module_n::TLB_table_write_t tw = {};
    tw.we = true;
    tw.vpn1 = ptw_mod.io.table_write.vpn1;
    tw.vpn0 = ptw_mod.io.table_write.vpn0;
    tw.ppn1 = ptw_mod.io.table_write.ppn1;
    tw.ppn0 = ptw_mod.io.table_write.ppn0;
    tw.asid = ptw_mod.io.table_write.asid;
    tw.megapage = ptw_mod.io.table_write.megapage;
    tw.dirty = ptw_mod.io.table_write.dirty;
    tw.accessed = ptw_mod.io.table_write.accessed;
    tw.global = ptw_mod.io.table_write.global;
    tw.user = ptw_mod.io.table_write.user;
    tw.execute = ptw_mod.io.table_write.execute;
    tw.write = ptw_mod.io.table_write.write;
    tw.read = ptw_mod.io.table_write.read;
    tw.valid_bit = ptw_mod.io.table_write.valid_bit;
    tw.pte_valid = ptw_mod.io.table_write.pte_valid;

    const bool to_itlb =
        ptw_mod.io.table_write.dest == static_cast<wire1_t>(DEST_ITLB);
    if (to_itlb) {
      tw.index = pick_refill_victim(itlb_table, itlb_refill_idx_r);
      apply_tlb_table_write(itlb_table, tw);
      itlb_refill_idx_r = static_cast<reg6_t>(
          (static_cast<uint32_t>(tw.index) + 1u) % ITLB_SIZE);
    } else {
      tw.index = pick_refill_victim(dtlb_table, dtlb_refill_idx_r);
      apply_tlb_table_write(dtlb_table, tw);
      dtlb_refill_idx_r = static_cast<reg6_t>(
          (static_cast<uint32_t>(tw.index) + 1u) % DTLB_SIZE);
    }
  }
}
