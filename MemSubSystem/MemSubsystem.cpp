#include "MemSubsystem.h"
#include "SimpleCache.h"
#include "config.h"
#include "icache/GenericTable.h"
#include <memory>

#if __has_include("UART16550_Device.h") && \
    __has_include("AXI_Interconnect.h") && \
    __has_include("AXI_Interconnect_AXI3.h") && \
    __has_include("AXI_Router_AXI4.h") && \
    __has_include("AXI_Router_AXI3.h") && \
    __has_include("MMIO_Bus_AXI4.h") && \
    __has_include("MMIO_Bus_AXI3.h") && \
    __has_include("SimDDR.h") && \
    __has_include("SimDDR_AXI3.h")
#define AXI_KIT_HEADERS_AVAILABLE 1
#include "UART16550_Device.h"
#if CONFIG_AXI_PROTOCOL == 4
#include "AXI_Interconnect.h"
#include "AXI_Router_AXI4.h"
#include "MMIO_Bus_AXI4.h"
#include "SimDDR.h"
namespace {
using InterconnectImpl = axi_interconnect::AXI_Interconnect;
using DdrImpl = sim_ddr::SimDDR;
using RouterImpl = axi_interconnect::AXI_Router_AXI4;
using MmioImpl = mmio::MMIO_Bus_AXI4;
} // namespace
#elif CONFIG_AXI_PROTOCOL == 3
#include "AXI_Interconnect_AXI3.h"
#include "AXI_Router_AXI3.h"
#include "MMIO_Bus_AXI3.h"
#include "SimDDR_AXI3.h"
namespace {
using InterconnectImpl = axi_interconnect::AXI_Interconnect_AXI3;
using DdrImpl = sim_ddr_axi3::SimDDR_AXI3;
using RouterImpl = axi_interconnect::AXI_Router_AXI3;
using MmioImpl = mmio::MMIO_Bus_AXI3;
} // namespace
#else
#error "Unsupported CONFIG_AXI_PROTOCOL value"
#endif
#else
#define AXI_KIT_HEADERS_AVAILABLE 0
#if CONFIG_ICACHE_USE_AXI_MEM_PORT
#error "CONFIG_ICACHE_USE_AXI_MEM_PORT requires axi-interconnect-kit"
#endif
#endif

#if AXI_KIT_HEADERS_AVAILABLE && CONFIG_ICACHE_USE_AXI_MEM_PORT
#define AXI_KIT_RUNTIME_ENABLED 1
#else
#define AXI_KIT_RUNTIME_ENABLED 0
#endif

#if AXI_KIT_RUNTIME_ENABLED
namespace {

struct AxiLlcTableRuntime {
  DynamicGenericTable<SramTablePolicy> data;
  DynamicGenericTable<SramTablePolicy> meta;
  DynamicGenericTable<SramTablePolicy> repl;
  axi_interconnect::AXI_LLC_LookupIn_t lookup_in{};
  axi_interconnect::AXI_LLCConfig config{};
  bool enabled = false;

  static DynamicTableConfig make_table_config(uint32_t rows, uint32_t row_bytes,
                                              uint32_t latency) {
    DynamicTableConfig cfg;
    cfg.rows = rows;
    cfg.chunks = row_bytes;
    cfg.chunk_bits = 8;
    cfg.timing.fixed_latency = latency == 0 ? 1 : latency;
    cfg.timing.random_delay = false;
    return cfg;
  }

  static DynamicTableReadReq make_read_req(
      const axi_interconnect::AXI_LLC_TableReq_t &req) {
    DynamicTableReadReq read_req;
    read_req.enable = req.enable && !req.write;
    read_req.address = req.index;
    return read_req;
  }

  static DynamicTableWriteReq make_write_req(
      const axi_interconnect::AXI_LLC_TableReq_t &req, uint32_t row_bytes,
      uint32_t unit_bytes) {
    DynamicTableWriteReq write_req;
    write_req.enable = req.enable && req.write;
    write_req.address = req.index;
    write_req.payload.reset(row_bytes);
    write_req.chunk_enable.assign(row_bytes, 0);
    if (!write_req.enable) {
      return write_req;
    }
    const size_t base = unit_bytes == 0 ? 0 : static_cast<size_t>(req.way) * unit_bytes;
    const size_t copy_bytes =
        std::min(req.payload.size(), row_bytes > base ? row_bytes - base : 0u);
    if (copy_bytes == 0) {
      return write_req;
    }
    std::memcpy(write_req.payload.data() + base, req.payload.data(), copy_bytes);
    const size_t en_bytes = std::min(req.byte_enable.size(), copy_bytes);
    for (size_t i = 0; i < en_bytes; ++i) {
      write_req.chunk_enable[base + i] = req.byte_enable[i];
    }
    return write_req;
  }

  void configure(const axi_interconnect::AXI_LLCConfig &cfg) {
    config = cfg;
    enabled = cfg.enable && cfg.valid();
    lookup_in = {};
    if (!enabled) {
      return;
    }
    const uint32_t sets = cfg.set_count();
    data.configure(make_table_config(sets, cfg.ways * cfg.line_bytes,
                                     cfg.lookup_latency));
    meta.configure(make_table_config(
        sets, cfg.ways * axi_interconnect::AXI_LLC_META_ENTRY_BYTES,
        cfg.lookup_latency));
    repl.configure(make_table_config(sets, axi_interconnect::AXI_LLC_REPL_BYTES,
                                     cfg.lookup_latency));
    data.reset();
    meta.reset();
    repl.reset();
  }

  void comb_outputs() {
    lookup_in = {};
    if (!enabled) {
      return;
    }
    DynamicTableReadResp data_resp, meta_resp, repl_resp;
    data.comb({}, data_resp);
    meta.comb({}, meta_resp);
    repl.comb({}, repl_resp);
    lookup_in.data_valid = data_resp.valid;
    lookup_in.meta_valid = meta_resp.valid;
    lookup_in.repl_valid = repl_resp.valid;
    lookup_in.data.bytes = data_resp.payload.bytes;
    lookup_in.meta.bytes = meta_resp.payload.bytes;
    lookup_in.repl.bytes = repl_resp.payload.bytes;
  }

  void seq(const axi_interconnect::AXI_LLC_TableOut_t &table_out) {
    if (!enabled) {
      return;
    }
    if (table_out.invalidate_all) {
      data.reset();
      meta.reset();
      repl.reset();
    }
    const auto data_read = make_read_req(table_out.data);
    const auto meta_read = make_read_req(table_out.meta);
    const auto repl_read = make_read_req(table_out.repl);
    const auto data_write =
        make_write_req(table_out.data, config.ways * config.line_bytes,
                       config.line_bytes);
    const auto meta_write = make_write_req(
        table_out.meta,
        config.ways * axi_interconnect::AXI_LLC_META_ENTRY_BYTES,
        axi_interconnect::AXI_LLC_META_ENTRY_BYTES);
    const auto repl_write =
        make_write_req(table_out.repl, axi_interconnect::AXI_LLC_REPL_BYTES, 0);
    data.seq(data_read, data_write);
    meta.seq(meta_read, meta_write);
    repl.seq(repl_read, repl_write);
  }
};

} // namespace

struct AxiKitRuntime {
  InterconnectImpl interconnect;
  DdrImpl ddr;
  RouterImpl router;
  MmioImpl mmio;
  mmio::UART16550_Device uart0{0x10000000u};
  AxiLlcTableRuntime llc_tables;
};
#else
struct AxiKitRuntime {};
#endif

class MemSubsystemPtwMemPortAdapter : public PtwMemPort {
public:
  MemSubsystemPtwMemPortAdapter(MemSubsystem *owner, MemSubsystem::PtwClient c)
      : owner(owner), client(c) {}

  bool send_read_req(uint32_t paddr) override {
    return owner->ptw_mem_send_read_req(client, paddr);
  }
  bool resp_valid() const override { return owner->ptw_mem_resp_valid(client); }
  uint32_t resp_data() const override { return owner->ptw_mem_resp_data(client); }
  void consume_resp() override { owner->ptw_mem_consume_resp(client); }

private:
  MemSubsystem *owner = nullptr;
  MemSubsystem::PtwClient client = MemSubsystem::PtwClient::DTLB;
};

class MemSubsystemPtwWalkPortAdapter : public PtwWalkPort {
public:
  MemSubsystemPtwWalkPortAdapter(MemSubsystem *owner, MemSubsystem::PtwClient c)
      : owner(owner), client(c) {}

  bool send_walk_req(const PtwWalkReq &req) override {
    return owner->ptw_walk_send_req(client, req);
  }
  bool resp_valid() const override { return owner->ptw_walk_resp_valid(client); }
  PtwWalkResp resp() const override { return owner->ptw_walk_resp(client); }
  void consume_resp() override { owner->ptw_walk_consume_resp(client); }
  void flush_client() override { owner->ptw_walk_flush(client); }

private:
  MemSubsystem *owner = nullptr;
  MemSubsystem::PtwClient client = MemSubsystem::PtwClient::DTLB;
};

MemPtwBlock::Client MemSubsystem::to_block_client(PtwClient c) {
  return (c == MemSubsystem::PtwClient::DTLB) ? MemPtwBlock::Client::DTLB
                                               : MemPtwBlock::Client::ITLB;
}

void MemSubsystem::refresh_ptw_client_outputs() {
  for (size_t i = 0; i < kPtwClientCount; i++) {
    PtwClient client = static_cast<PtwClient>(i);
    ptw_mem_resp_ios[i].valid =
        ptw_block.client_resp_valid(to_block_client(client));
    ptw_mem_resp_ios[i].data =
        ptw_block.client_resp_data(to_block_client(client));
    ptw_walk_resp_ios[i].valid =
        ptw_block.walk_client_resp_valid(to_block_client(client));
    ptw_walk_resp_ios[i].resp = ptw_block.walk_client_resp(to_block_client(client));
  }
}

bool MemSubsystem::ptw_mem_send_read_req(PtwClient client, uint32_t paddr) {
  bool fire = ptw_block.client_send_read_req(to_block_client(client), paddr);
  refresh_ptw_client_outputs();
  return fire;
}

bool MemSubsystem::ptw_mem_resp_valid(PtwClient client) const {
  return ptw_mem_resp_ios[ptw_client_idx(client)].valid;
}

uint32_t MemSubsystem::ptw_mem_resp_data(PtwClient client) const {
  return ptw_mem_resp_ios[ptw_client_idx(client)].data;
}

void MemSubsystem::ptw_mem_consume_resp(PtwClient client) {
  ptw_block.client_consume_resp(to_block_client(client));
  refresh_ptw_client_outputs();
}

bool MemSubsystem::ptw_walk_send_req(PtwClient client, const PtwWalkReq &req) {
  bool fire = ptw_block.walk_client_send_req(to_block_client(client), req);
  refresh_ptw_client_outputs();
  return fire;
}

bool MemSubsystem::ptw_walk_resp_valid(PtwClient client) const {
  return ptw_walk_resp_ios[ptw_client_idx(client)].valid;
}

PtwWalkResp MemSubsystem::ptw_walk_resp(PtwClient client) const {
  return ptw_walk_resp_ios[ptw_client_idx(client)].resp;
}

void MemSubsystem::ptw_walk_consume_resp(PtwClient client) {
  ptw_block.walk_client_consume_resp(to_block_client(client));
  refresh_ptw_client_outputs();
}

void MemSubsystem::ptw_walk_flush(PtwClient client) {
  ptw_block.walk_client_flush(to_block_client(client));
  refresh_ptw_client_outputs();
}

axi_interconnect::ReadMasterPort_t *MemSubsystem::icache_read_port() {
#if AXI_KIT_RUNTIME_ENABLED
  if (axi_kit_runtime == nullptr) {
    return nullptr;
  }
  return &axi_kit_runtime->interconnect.read_ports[axi_interconnect::MASTER_ICACHE];
#else
  return nullptr;
#endif
}

MemSubsystem::MemSubsystem(SimContext *ctx) : ctx(ctx) {
  dcache = std::make_unique<SimpleCache>(ctx);
#if AXI_KIT_RUNTIME_ENABLED
  axi_kit_runtime = std::make_unique<AxiKitRuntime>();
#endif
  ptw_block.bind_context(ctx);
  dtlb_ptw_port_inst =
      std::make_unique<MemSubsystemPtwMemPortAdapter>(this, PtwClient::DTLB);
  itlb_ptw_port_inst =
      std::make_unique<MemSubsystemPtwMemPortAdapter>(this, PtwClient::ITLB);
  dtlb_walk_port_inst =
      std::make_unique<MemSubsystemPtwWalkPortAdapter>(this, PtwClient::DTLB);
  itlb_walk_port_inst =
      std::make_unique<MemSubsystemPtwWalkPortAdapter>(this, PtwClient::ITLB);
  dtlb_ptw_port = dtlb_ptw_port_inst.get();
  itlb_ptw_port = itlb_ptw_port_inst.get();
  dtlb_walk_port = dtlb_walk_port_inst.get();
  itlb_walk_port = itlb_walk_port_inst.get();
}

MemSubsystem::~MemSubsystem() = default;

void MemSubsystem::init() {
  Assert(lsu_req_io != nullptr && "MemSubsystem: lsu_req_io is not connected");
  Assert(lsu_wreq_io != nullptr &&
         "MemSubsystem: lsu_wreq_io is not connected");
  Assert(lsu_resp_io != nullptr &&
         "MemSubsystem: lsu_resp_io is not connected");
  Assert(lsu_wready_io != nullptr &&
         "MemSubsystem: lsu_wready_io is not connected");
  Assert(csr != nullptr && "MemSubsystem: csr is not connected");
  Assert(memory != nullptr && "MemSubsystem: memory is not connected");

  peripheral.csr = csr;
  peripheral.memory = memory;
  peripheral.init();

#if AXI_KIT_RUNTIME_ENABLED
  axi_interconnect::AXI_LLCConfig llc_cfg;
  llc_cfg.enable = CONFIG_AXI_PROTOCOL == 4 && CONFIG_AXI_LLC_ENABLE;
  llc_cfg.size_bytes = AXI_LLC_DEFAULT_SIZE_BYTES;
  llc_cfg.line_bytes = AXI_LLC_DEFAULT_LINE_SIZE;
  llc_cfg.ways = AXI_LLC_DEFAULT_WAYS;
  llc_cfg.mshr_num = AXI_LLC_DEFAULT_MSHR_NUM;
  llc_cfg.lookup_latency = AXI_LLC_DEFAULT_LOOKUP_LATENCY;
  axi_kit_runtime->interconnect.set_llc_config(llc_cfg);
  axi_kit_runtime->llc_tables.configure(llc_cfg);
  axi_kit_runtime->interconnect.init();
  axi_kit_runtime->router.init();
  axi_kit_runtime->mmio.init();
  axi_kit_runtime->mmio.add_device(0x10000000u, 0x1000u, &axi_kit_runtime->uart0);
  axi_kit_runtime->ddr.init();
#endif

  dcache->lsu_req_io = &dcache_req_mux;
  dcache->lsu_wreq_io = &dcache_wreq_mux;
  dcache->lsu_resp_io = &dcache_resp_raw;
  dcache->lsu_wready_io = &dcache_wready_raw;
  dcache->peripheral_model = &peripheral;

  ptw_block.init();
  resp_route_block.init();
  ptw_mem_resp_ios = {};
  ptw_walk_resp_ios = {};
  refresh_ptw_client_outputs();

  dcache_req_mux = {};
  dcache_wreq_mux = {};
  dcache_resp_raw = {};
  dcache_wready_raw = {};
  *lsu_resp_io = {};
  *lsu_wready_io = {};

#if AXI_KIT_RUNTIME_ENABLED
  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    auto &port = axi_kit_runtime->interconnect.read_ports[i];
    port.req.valid = false;
    port.req.addr = 0;
    port.req.total_size = 0;
    port.req.id = 0;
    port.req.bypass = false;
    port.resp.ready = false;
  }
  for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; i++) {
    auto &port = axi_kit_runtime->interconnect.write_ports[i];
    port.req.valid = false;
    port.req.addr = 0;
    port.req.wdata.clear();
    port.req.wstrb = 0;
    port.req.total_size = 0;
    port.req.id = 0;
    port.req.bypass = false;
    port.resp.ready = false;
  }
#endif

  dcache->init();
}

void MemSubsystem::on_commit_store(uint32_t paddr, uint32_t data, uint8_t func3) {
  peripheral.on_commit_store(paddr, data, func3);
}

void MemSubsystem::comb() {
#if AXI_KIT_RUNTIME_ENABLED
  auto &interconnect = axi_kit_runtime->interconnect;
  auto &ddr = axi_kit_runtime->ddr;
  auto &router = axi_kit_runtime->router;
  auto &mmio = axi_kit_runtime->mmio;
  auto &llc_tables = axi_kit_runtime->llc_tables;

  llc_tables.comb_outputs();
  interconnect.set_llc_lookup_in(llc_tables.lookup_in);

  // AXI-kit phase-1 combinational outputs.
  ddr.comb_outputs();
  mmio.comb_outputs();
  router.comb_outputs(interconnect.axi_io, ddr.io, mmio.io);
  interconnect.comb_outputs();
#endif

  // 子模块按组合逻辑顺序推进。
  ptw_block.comb_select_walk_owner();

  // Default outputs every cycle.
  *lsu_resp_io = {};
  dcache_req_mux = {};
  dcache_wreq_mux = {};

  // Pass write channel (currently only LSU issues writes).
  dcache_wreq_mux = *lsu_wreq_io;

  uint32_t ptw_walk_read_addr = 0;
  bool issue_ptw_walk_read = ptw_block.walk_read_req(ptw_walk_read_addr);

  bool has_ptw_dtlb = ptw_block.has_pending_mem_req(MemPtwBlock::Client::DTLB);
  bool has_ptw_itlb = ptw_block.has_pending_mem_req(MemPtwBlock::Client::ITLB);
  uint32_t ptw_dtlb_addr = has_ptw_dtlb
                               ? ptw_block.pending_mem_addr(
                                     MemPtwBlock::Client::DTLB)
                               : 0;
  uint32_t ptw_itlb_addr = has_ptw_itlb
                               ? ptw_block.pending_mem_addr(
                                     MemPtwBlock::Client::ITLB)
                               : 0;

  auto arb_ret = read_arb_block.arbitrate(
      lsu_req_io, issue_ptw_walk_read, ptw_walk_read_addr, has_ptw_dtlb,
      ptw_dtlb_addr, has_ptw_itlb, ptw_itlb_addr);
  ptw_block.count_wait_cycles();

  if (arb_ret.granted) {
    dcache_req_mux = arb_ret.req;
  }

  if (arb_ret.owner == MemReadArbBlock::Owner::LSU) {
    resp_route_block.enqueue_owner(MemRespRouteBlock::Owner::LSU);
  } else if (arb_ret.owner == MemReadArbBlock::Owner::PTW_WALK) {
    resp_route_block.enqueue_owner(MemRespRouteBlock::Owner::PTW_WALK);
    ptw_block.on_walk_read_granted();
  } else if (arb_ret.owner == MemReadArbBlock::Owner::PTW_DTLB) {
    ptw_block.on_mem_read_granted(MemPtwBlock::Client::DTLB);
    resp_route_block.enqueue_owner(MemRespRouteBlock::Owner::PTW_DTLB);
  } else if (arb_ret.owner == MemReadArbBlock::Owner::PTW_ITLB) {
    ptw_block.on_mem_read_granted(MemPtwBlock::Client::ITLB);
    resp_route_block.enqueue_owner(MemRespRouteBlock::Owner::PTW_ITLB);
  }

  dcache->comb();

  // Write ready backpressure directly reflects DCache.
  *lsu_wready_io = dcache_wready_raw;

  (void)resp_route_block.route_resp(dcache_resp_raw, lsu_resp_io, &ptw_block);

  // Stage-1 AXI wiring: connect ICache read master, keep all others idle.
#if AXI_KIT_RUNTIME_ENABLED
  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    if (i == axi_interconnect::MASTER_ICACHE) {
      continue;
    }
    auto &port = interconnect.read_ports[i];
    port.req.valid = false;
    port.req.addr = 0;
    port.req.total_size = 0;
    port.req.id = 0;
    port.req.bypass = false;
    port.resp.ready = false;
  }
  for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; i++) {
    auto &port = interconnect.write_ports[i];
    port.req.valid = false;
    port.req.addr = 0;
    port.req.wdata.clear();
    port.req.wstrb = 0;
    port.req.total_size = 0;
    port.req.id = 0;
    port.req.bypass = false;
    port.resp.ready = false;
  }

  // AXI-kit phase-2 combinational inputs.
  interconnect.comb_inputs();
  router.comb_inputs(interconnect.axi_io, ddr.io, mmio.io);
  ddr.comb_inputs();
  mmio.comb_inputs();
#endif

  refresh_ptw_client_outputs();
}

void MemSubsystem::seq() {
  dcache->seq();
#if AXI_KIT_RUNTIME_ENABLED
  axi_kit_runtime->ddr.seq();
  axi_kit_runtime->mmio.seq();
  axi_kit_runtime->router.seq(axi_kit_runtime->interconnect.axi_io, axi_kit_runtime->ddr.io,
                          axi_kit_runtime->mmio.io);
  axi_kit_runtime->interconnect.seq();
  axi_kit_runtime->llc_tables.seq(
      axi_kit_runtime->interconnect.get_llc_table_out());
#endif
}
