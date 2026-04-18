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

void apply_frame(AXI_Interconnect &interconnect, FakeLlcTables &tables,
                 const equiv_case::Frame &frame) {
  clear_inputs(interconnect);

  interconnect.mode = frame.mode_req;
  interconnect.llc_mapped_offset = frame.llc_mapped_offset_req;
  interconnect.set_llc_invalidate_all(frame.invalidate_all_valid);
  interconnect.set_llc_invalidate_line(frame.invalidate_line_valid,
                                       frame.invalidate_line_addr);

  for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
    const auto &src = frame.read_req[m];
    auto &dst = interconnect.read_ports[m];
    dst.req.valid = src.valid;
    dst.req.addr = src.addr;
    dst.req.total_size = src.size;
    dst.req.id = src.id;
    dst.req.bypass = src.bypass;
    dst.resp.ready = ((frame.read_resp_ready_mask >> m) & 0x1u) != 0u;
  }

  for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
    const auto &src = frame.write_req[m];
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

  uint8_t prev_mode = 0xFF;
  uint32_t prev_offset = 0xFFFFFFFFu;

  for (int cycle = 0; cycle < equiv_case::kNumCycles; ++cycle) {
    const auto &frame = equiv_case::kFrames[cycle];
    const int trace_cycle = cycle + 1;
    apply_frame(interconnect, tables, frame);
    interconnect.comb_outputs();
    interconnect.comb_inputs();

    for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
      if (interconnect.read_req_accepted[m]) {
        const auto &rq = frame.read_req[m];
        std::fprintf(fp,
                     "%d READ_ACCEPT m=%d id=%u addr=0x%08x size=%u bypass=%u\n",
                     trace_cycle, m, static_cast<unsigned>(rq.id), rq.addr,
                     static_cast<unsigned>(rq.size),
                     static_cast<unsigned>(rq.bypass));
      }
    }
    for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
      if (interconnect.write_req_accepted[m]) {
        const auto &rq = frame.write_req[m];
        std::fprintf(
            fp,
            "%d WRITE_ACCEPT m=%d id=%u addr=0x%08x size=%u bypass=%u data0=0x%08x strbhash=0x%08x\n",
            trace_cycle, m, static_cast<unsigned>(rq.id), rq.addr,
            static_cast<unsigned>(rq.size),
            static_cast<unsigned>(rq.bypass), rq.wdata_words[0],
            hash_write_strobe(interconnect.write_ports[m].req.wstrb));
      }
    }
    for (int m = 0; m < axi_interconnect::NUM_READ_MASTERS; ++m) {
      auto &resp = interconnect.read_ports[m].resp;
      if (resp.valid && interconnect.read_ports[m].resp.ready) {
        std::fprintf(
            fp,
            "%d READ_RESP m=%d id=%u hash=0x%08x d0=0x%08x d1=0x%08x\n",
            trace_cycle, m, static_cast<unsigned>(resp.id),
            hash_read_words(resp.data), resp.data[0], resp.data[1]);
      }
    }
    for (int m = 0; m < axi_interconnect::NUM_WRITE_MASTERS; ++m) {
      auto &resp = interconnect.write_ports[m].resp;
      if (resp.valid && interconnect.write_ports[m].resp.ready) {
        std::fprintf(fp, "%d WRITE_RESP m=%d id=%u code=%u\n", trace_cycle, m,
                     static_cast<unsigned>(resp.id),
                     static_cast<unsigned>(resp.resp));
      }
    }
    if (interconnect.llc_invalidate_line_accepted()) {
      std::fprintf(fp, "%d MAINT_ACCEPT op=invalidate_line addr=0x%08x\n",
                   trace_cycle, frame.invalidate_line_addr);
    }
    if (interconnect.llc_invalidate_all_accepted()) {
      std::fprintf(fp, "%d MAINT_ACCEPT op=invalidate_all\n", trace_cycle);
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
    interconnect.seq();
    ++sim_time;
  }

  std::fclose(fp);
  return 0;
}
