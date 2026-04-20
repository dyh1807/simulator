#include <cstdio>
#include <cstdlib>
#include <cstdint>

#define private public
#include "AXI_Interconnect.h"
#undef private
#include "axi_test_axi4_llc_env.h"
#include "case_data.h"

uint32_t *p_memory = nullptr;
long long sim_time = 0;

namespace {

using axi_interconnect::AXI_Interconnect;
using axi_interconnect::AXI_LLCConfig;
using axi_test::FakeLlcTables;

AXI_LLCConfig make_config() {
  AXI_LLCConfig cfg;
  cfg.enable = true;
  cfg.size_bytes = 8u * 1024u * 1024u;
  cfg.line_bytes = 64;
  cfg.ways = 16;
  cfg.mshr_num = 8;
  cfg.prefetch_enable = false;
  return cfg;
}

void clear_inputs(AXI_Interconnect &interconnect) {
  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; ++i) {
    interconnect.read_ports[i].req.valid = false;
    interconnect.read_ports[i].req.addr = 0;
    interconnect.read_ports[i].req.total_size = 0;
    interconnect.read_ports[i].req.id = 0;
    interconnect.read_ports[i].req.bypass = false;
    interconnect.read_ports[i].resp.ready = false;
  }
  for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; ++i) {
    interconnect.write_ports[i].req.valid = false;
    interconnect.write_ports[i].req.addr = 0;
    interconnect.write_ports[i].req.total_size = 0;
    interconnect.write_ports[i].req.id = 0;
    interconnect.write_ports[i].req.bypass = false;
    interconnect.write_ports[i].req.wdata.clear();
    interconnect.write_ports[i].req.wstrb.clear();
    interconnect.write_ports[i].resp.ready = false;
  }
}

struct HeldReadReq {
  bool active = false;
  equiv_case::ReadReq req{};
};

struct HeldWriteReq {
  bool active = false;
  equiv_case::WriteReq req{};
};

struct GeneratedMemReadLineResp {
  bool active = false;
  uint8_t rid = 0;
  uint8_t total_beats = 0;
  uint8_t beat_idx = 0;
  std::array<uint32_t, 16> line_words{};
};

template <size_t N>
void apply_line_to_samples(const uint32_t base_addr,
                           const std::array<uint32_t, 16> &line_words,
                           std::array<bool, N> &known,
                           std::array<uint32_t, N> &values) {
  for (size_t idx = 0; idx < N; ++idx) {
    const uint32_t sample_addr = equiv_case::kFinalMemSampleAddrs[idx];
    if (sample_addr < base_addr || sample_addr >= (base_addr + 64u)) {
      continue;
    }
    const uint32_t word_idx = (sample_addr - base_addr) >> 2;
    if (word_idx < line_words.size()) {
      known[idx] = true;
      values[idx] = line_words[word_idx];
    }
  }
}

template <size_t N>
void apply_write_beat_to_samples(const uint32_t beat_addr,
                                 const sim_ddr::axi_data_t &wdata,
                                 const sim_ddr::axi_strb_t &wstrb,
                                 std::array<bool, N> &known,
                                 std::array<uint32_t, N> &values) {
  for (size_t idx = 0; idx < N; ++idx) {
    const uint32_t sample_addr = equiv_case::kFinalMemSampleAddrs[idx];
    if (sample_addr < beat_addr ||
        sample_addr >= (beat_addr + sim_ddr::SIM_DDR_BEAT_BYTES)) {
      continue;
    }
    uint32_t merged = known[idx] ? values[idx] : 0u;
    const uint32_t byte_off = sample_addr - beat_addr;
    for (uint32_t b = 0; b < 4u; ++b) {
      if (!axi_compat::test_bit(wstrb, byte_off + b)) {
        continue;
      }
      const uint8_t new_byte =
          static_cast<uint8_t>(axi_compat::get_byte(wdata, byte_off + b));
      merged &= ~(0xffu << (b * 8u));
      merged |= static_cast<uint32_t>(new_byte) << (b * 8u);
    }
    known[idx] = true;
    values[idx] = merged;
  }
}

uint32_t hash_read_words(const axi_interconnect::WideReadData_t &data) {
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < 8; ++i) {
    h = ((h << 5) | (h >> 27)) ^ data.words[i] ^ static_cast<uint32_t>(i);
  }
  return h;
}

uint32_t hash_write_strobe(const axi_interconnect::WideWriteStrb_t &wstrb) {
  uint32_t h = 0x13579BDFu;
  for (int i = 0; i < 16; ++i) {
    uint32_t nib = 0;
    for (int b = 0; b < 4; ++b) {
      if (wstrb.test(i * 4 + b)) {
        nib |= (1u << b);
      }
    }
    h = ((h << 3) | (h >> 29)) ^ nib ^ static_cast<uint32_t>(i);
  }
  return h;
}

uint32_t hash_axi_data(const sim_ddr::axi_data_t &data) {
  uint32_t h = 0x2468ACE1u;
  const uint32_t word_count = sim_ddr::SIM_DDR_BEAT_BYTES / 4u;
  for (uint32_t i = 0; i < word_count; ++i) {
    h = ((h << 5) | (h >> 27)) ^
        axi_compat::get_u32(data, i) ^ static_cast<uint32_t>(i);
  }
  return h;
}

uint32_t hash_axi_strb(const sim_ddr::axi_strb_t &strb) {
  uint32_t h = 0x5A5A1357u;
  for (uint32_t i = 0; i < sim_ddr::SIM_DDR_BEAT_BYTES; i += 4u) {
    uint32_t nib = 0;
    for (uint32_t b = 0; b < 4u && (i + b) < sim_ddr::SIM_DDR_BEAT_BYTES; ++b) {
      nib |= static_cast<uint32_t>(axi_compat::get_byte(strb, i + b) & 0x1u) << b;
    }
    h = ((h << 3) | (h >> 29)) ^ nib ^ i;
  }
  return h;
}

int find_oldest_pending_llc_read(const AXI_Interconnect &interconnect) {
  for (size_t idx = 0; idx < interconnect.r_pending.size(); ++idx) {
    const auto &txn = interconnect.r_pending[idx];
    if (txn.to_llc && txn.beats_done < txn.total_beats) {
      return static_cast<int>(idx);
    }
  }
  return -1;
}

void start_generated_mem_read_line_resp(
    AXI_Interconnect &interconnect, const equiv_case::Frame &frame,
    GeneratedMemReadLineResp &gen_resp) {
  if (!frame.mem_read_line_resp_valid || gen_resp.active) {
    return;
  }
  const int pending_idx = find_oldest_pending_llc_read(interconnect);
  if (pending_idx < 0) {
    return;
  }
  const auto &txn = interconnect.r_pending[static_cast<size_t>(pending_idx)];
  gen_resp.active = true;
  gen_resp.rid = txn.axi_id;
  gen_resp.total_beats = txn.total_beats;
  gen_resp.beat_idx = 0;
  gen_resp.line_words = frame.mem_read_line_resp_words;
}

void drive_generated_mem_read_line_resp(
    AXI_Interconnect &interconnect, const GeneratedMemReadLineResp &gen_resp) {
  if (!gen_resp.active) {
    return;
  }
  interconnect.axi_io.r.rvalid = true;
  interconnect.axi_io.r.rid = gen_resp.rid;
  interconnect.axi_io.r.rresp = sim_ddr::AXI_RESP_OKAY;
  interconnect.axi_io.r.rlast = (gen_resp.beat_idx + 1u) >= gen_resp.total_beats;
  interconnect.axi_io.r.rdata = 0;

  const uint32_t beat_word_count = sim_ddr::SIM_DDR_BEAT_BYTES / 4u;
  const uint32_t base = static_cast<uint32_t>(gen_resp.beat_idx) * beat_word_count;
  for (uint32_t word = 0; word < beat_word_count; ++word) {
    const uint32_t idx = base + word;
    const uint32_t value =
        idx < gen_resp.line_words.size() ? gen_resp.line_words[idx] : 0u;
    axi_compat::set_u32(interconnect.axi_io.r.rdata, word, value);
  }
}

void advance_generated_mem_read_line_resp(
    const AXI_Interconnect &interconnect, GeneratedMemReadLineResp &gen_resp) {
  if (!gen_resp.active) {
    return;
  }
  if (!(interconnect.axi_io.r.rvalid && interconnect.axi_io.r.rready)) {
    return;
  }
  if ((gen_resp.beat_idx + 1u) >= gen_resp.total_beats) {
    gen_resp = {};
    return;
  }
  gen_resp.beat_idx++;
}

void apply_frame(AXI_Interconnect &interconnect, FakeLlcTables &tables,
                 const equiv_case::Frame &frame,
                 HeldReadReq held_read[axi_interconnect::NUM_READ_MASTERS],
                 HeldWriteReq held_write[axi_interconnect::NUM_WRITE_MASTERS]) {
  clear_inputs(interconnect);

  interconnect.mode = frame.mode_req;
  interconnect.llc_mapped_offset = frame.llc_mapped_offset_req;
  interconnect.set_llc_invalidate_all(frame.invalidate_all_valid);
  interconnect.set_llc_invalidate_line(frame.invalidate_line_valid,
                                       frame.invalidate_line_addr);

  for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
    if (frame.read_req[m].valid && frame.read_req[m].hold_until_accept &&
        !held_read[m].active) {
      held_read[m].active = true;
      held_read[m].req = frame.read_req[m];
    }
    const auto &src = held_read[m].active ? held_read[m].req : frame.read_req[m];
    auto &dst = interconnect.read_ports[m];
    dst.req.valid = src.valid;
    dst.req.addr = src.addr;
    dst.req.total_size = src.size;
    dst.req.id = src.id;
    dst.req.bypass = src.bypass;
    dst.resp.ready = ((frame.read_resp_ready_mask >> m) & 0x1u) != 0u;
  }

  for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
    if (frame.write_req[m].valid && frame.write_req[m].hold_until_accept &&
        !held_write[m].active) {
      held_write[m].active = true;
      held_write[m].req = frame.write_req[m];
    }
    const auto &src = held_write[m].active ? held_write[m].req : frame.write_req[m];
    auto &dst = interconnect.write_ports[m];
    dst.req.valid = src.valid;
    dst.req.addr = src.addr;
    dst.req.total_size = src.size;
    dst.req.id = src.id;
    dst.req.bypass = src.bypass;
    dst.req.wdata.clear();
    dst.req.wstrb.clear();
    for (int i = 0; i < 16; ++i) {
      dst.req.wdata[i] = src.wdata_words[i];
    }
    for (int i = 0; i < 64; ++i) {
      dst.req.wstrb.set(i, src.wstrb_bytes[i] != 0);
    }
    dst.resp.ready = ((frame.write_resp_ready_mask >> m) & 0x1u) != 0u;
  }

  tables.comb_outputs();
  interconnect.set_llc_lookup_in(tables.lookup_in);

  interconnect.axi_io.ar.arready = frame.axi_arready;
  interconnect.axi_io.aw.awready = frame.axi_awready;
  interconnect.axi_io.w.wready = frame.axi_wready;
  interconnect.axi_io.b.bvalid = frame.axi_bvalid;
  interconnect.axi_io.b.bid = frame.axi_bid;
  interconnect.axi_io.b.bresp = frame.axi_bresp;
  interconnect.axi_io.r.rvalid = frame.axi_rvalid;
  interconnect.axi_io.r.rid = frame.axi_rid;
  interconnect.axi_io.r.rresp = frame.axi_rresp;
  interconnect.axi_io.r.rlast = frame.axi_rlast;
  interconnect.axi_io.r.rdata = 0;
  for (int i = 0; i < 8; ++i) {
    axi_compat::set_u32(interconnect.axi_io.r.rdata, static_cast<uint32_t>(i),
                        frame.axi_rdata_words[i]);
  }
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <trace_file>\n", argv[0]);
    return 2;
  }

  FILE *fp = std::fopen(argv[1], "w");
  if (!fp) {
    std::perror("fopen trace");
    return 2;
  }

  AXI_Interconnect interconnect;
  FakeLlcTables tables;
  auto cfg = make_config();
  interconnect.set_llc_config(cfg);
  interconnect.init();
  tables.init(cfg);
  HeldReadReq held_read[axi_interconnect::NUM_READ_MASTERS];
  HeldWriteReq held_write[axi_interconnect::NUM_WRITE_MASTERS];
  GeneratedMemReadLineResp gen_mem_read_resp;
  std::array<bool, equiv_case::kNumFinalMemSamples> final_mem_known{};
  std::array<uint32_t, equiv_case::kNumFinalMemSamples> final_mem_values{};
  bool final_mem_write_pending = false;
  uint32_t final_mem_write_addr = 0;
  uint8_t final_mem_write_beat_idx = 0;
  bool prev_read_resp_valid[axi_interconnect::NUM_READ_MASTERS] = {};
  uint8_t prev_read_resp_id[axi_interconnect::NUM_READ_MASTERS] = {};
  uint32_t prev_read_resp_d0[axi_interconnect::NUM_READ_MASTERS] = {};
  bool prev_write_resp_valid[axi_interconnect::NUM_WRITE_MASTERS] = {};
  uint8_t prev_write_resp_id[axi_interconnect::NUM_WRITE_MASTERS] = {};
  uint8_t prev_write_resp_code[axi_interconnect::NUM_WRITE_MASTERS] = {};

  uint8_t prev_mode = 0xFF;
  uint32_t prev_offset = 0xFFFFFFFFu;

  for (int cycle = 0; cycle < equiv_case::kNumCycles; ++cycle) {
    const auto &frame = equiv_case::kFrames[cycle];
    const int trace_cycle = cycle + 1;
    apply_frame(interconnect, tables, frame, held_read, held_write);
    start_generated_mem_read_line_resp(interconnect, frame, gen_mem_read_resp);
    if (gen_mem_read_resp.active && frame.mem_read_line_resp_valid) {
      const int pending_idx = find_oldest_pending_llc_read(interconnect);
      if (pending_idx >= 0) {
        const auto &txn = interconnect.r_pending[static_cast<size_t>(pending_idx)];
        apply_line_to_samples(txn.addr, gen_mem_read_resp.line_words,
                              final_mem_known, final_mem_values);
      }
    }
    drive_generated_mem_read_line_resp(interconnect, gen_mem_read_resp);
    interconnect.comb_outputs();
    interconnect.comb_inputs();

    for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
      if (interconnect.read_req_accepted[m]) {
        const auto &rq = interconnect.read_ports[m].req;
        std::fprintf(fp,
                     "%d READ_ACCEPT m=%d id=%u addr=0x%08x size=%u bypass=%u\n",
                     trace_cycle, m, static_cast<unsigned>(rq.id), rq.addr,
                     static_cast<unsigned>(rq.total_size),
                     static_cast<unsigned>(rq.bypass));
      }
      if (held_read[m].active && interconnect.read_req_accepted[m]) {
        held_read[m].active = false;
      }
    }
    for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
      if (interconnect.write_req_accepted[m]) {
        const auto &rq = interconnect.write_ports[m].req;
        std::fprintf(
            fp,
            "%d WRITE_ACCEPT m=%d id=%u addr=0x%08x size=%u bypass=%u data0=0x%08x strbhash=0x%08x\n",
            trace_cycle, m, static_cast<unsigned>(rq.id), rq.addr,
            static_cast<unsigned>(rq.total_size),
            static_cast<unsigned>(rq.bypass), rq.wdata[0],
            hash_write_strobe(rq.wstrb));
      }
      if (held_write[m].active && interconnect.write_req_accepted[m]) {
        held_write[m].active = false;
      }
    }
    for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
      auto &resp = interconnect.read_ports[m].resp;
      const bool new_visible_resp =
          !prev_read_resp_valid[m] || prev_read_resp_id[m] != resp.id ||
          prev_read_resp_d0[m] != resp.data[0];
      if (resp.valid && interconnect.read_ports[m].resp.ready &&
          new_visible_resp) {
        std::fprintf(
            fp,
            "%d READ_RESP m=%d id=%u hash=0x%08x d0=0x%08x d1=0x%08x\n",
            trace_cycle, m, static_cast<unsigned>(resp.id),
            hash_read_words(resp.data), resp.data[0], resp.data[1]);
      }
      prev_read_resp_valid[m] = resp.valid;
      prev_read_resp_id[m] = resp.id;
      prev_read_resp_d0[m] = resp.data[0];
    }
    for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
      auto &resp = interconnect.write_ports[m].resp;
      const bool new_visible_resp =
          !prev_write_resp_valid[m] || prev_write_resp_id[m] != resp.id ||
          prev_write_resp_code[m] != resp.resp;
      if (resp.valid && interconnect.write_ports[m].resp.ready &&
          new_visible_resp) {
        std::fprintf(fp, "%d WRITE_RESP m=%d id=%u code=%u\n", trace_cycle, m,
                     static_cast<unsigned>(resp.id),
                     static_cast<unsigned>(resp.resp));
      }
      prev_write_resp_valid[m] = resp.valid;
      prev_write_resp_id[m] = resp.id;
      prev_write_resp_code[m] = resp.resp;
    }
    if (frame.invalidate_line_valid &&
        interconnect.llc_invalidate_line_accepted()) {
      std::fprintf(fp, "%d MAINT_ACCEPT op=invalidate_line addr=0x%08x\n",
                   trace_cycle, frame.invalidate_line_addr);
    }
    if (frame.invalidate_all_valid && interconnect.llc_invalidate_all_accepted()) {
      std::fprintf(fp, "%d MAINT_ACCEPT op=invalidate_all\n", trace_cycle);
    }
    if (interconnect.axi_io.ar.arvalid && interconnect.axi_io.ar.arready) {
      std::fprintf(fp,
                   "%d AXI_AR_HS id=%u addr=0x%08x len=%u size=%u burst=%u\n",
                   trace_cycle,
                   static_cast<unsigned>(interconnect.axi_io.ar.arid),
                   static_cast<unsigned>(interconnect.axi_io.ar.araddr),
                   static_cast<unsigned>(interconnect.axi_io.ar.arlen),
                   static_cast<unsigned>(interconnect.axi_io.ar.arsize),
                   static_cast<unsigned>(interconnect.axi_io.ar.arburst));
    }
    if (interconnect.axi_io.aw.awvalid && interconnect.axi_io.aw.awready) {
      final_mem_write_pending = true;
      final_mem_write_addr = interconnect.axi_io.aw.awaddr;
      final_mem_write_beat_idx = 0;
      std::fprintf(fp,
                   "%d AXI_AW_HS id=%u addr=0x%08x len=%u size=%u burst=%u\n",
                   trace_cycle,
                   static_cast<unsigned>(interconnect.axi_io.aw.awid),
                   static_cast<unsigned>(interconnect.axi_io.aw.awaddr),
                   static_cast<unsigned>(interconnect.axi_io.aw.awlen),
                   static_cast<unsigned>(interconnect.axi_io.aw.awsize),
                   static_cast<unsigned>(interconnect.axi_io.aw.awburst));
    }
    if (interconnect.axi_io.w.wvalid && interconnect.axi_io.w.wready) {
      if (final_mem_write_pending) {
        const uint32_t beat_addr =
            final_mem_write_addr +
            static_cast<uint32_t>(final_mem_write_beat_idx) * sim_ddr::SIM_DDR_BEAT_BYTES;
        apply_write_beat_to_samples(beat_addr, interconnect.axi_io.w.wdata,
                                    interconnect.axi_io.w.wstrb,
                                    final_mem_known, final_mem_values);
        if (interconnect.axi_io.w.wlast) {
          final_mem_write_pending = false;
          final_mem_write_beat_idx = 0;
        } else {
          final_mem_write_beat_idx++;
        }
      }
      std::fprintf(fp,
                   "%d AXI_W_HS hash=0x%08x d0=0x%08x strbhash=0x%08x last=%u\n",
                   trace_cycle,
                   hash_axi_data(interconnect.axi_io.w.wdata),
                   axi_compat::get_u32(interconnect.axi_io.w.wdata, 0),
                   hash_axi_strb(interconnect.axi_io.w.wstrb),
                   static_cast<unsigned>(interconnect.axi_io.w.wlast));
    }
    if (interconnect.runtime_mode_ != prev_mode ||
        interconnect.llc_mapped_offset_ != prev_offset) {
      std::fprintf(fp, "%d MODE_ACTIVE mode=%u offset=0x%08x\n", trace_cycle,
                   static_cast<unsigned>(interconnect.runtime_mode_),
                   interconnect.llc_mapped_offset_);
      prev_mode = interconnect.runtime_mode_;
      prev_offset = interconnect.llc_mapped_offset_;
    }

    tables.seq(interconnect.get_llc_table_out());
    advance_generated_mem_read_line_resp(interconnect, gen_mem_read_resp);
    interconnect.seq();
    ++sim_time;
  }

  for (int idx = 0; idx < equiv_case::kNumFinalMemSamples; ++idx) {
    std::fprintf(fp, "%d FINAL_MEM addr=0x%08x known=%u val=0x%08x\n",
                 equiv_case::kNumCycles + 1,
                 equiv_case::kFinalMemSampleAddrs[idx],
                 final_mem_known[idx] ? 1u : 0u,
                 final_mem_values[idx]);
  }

  std::fclose(fp);
  return 0;
}
