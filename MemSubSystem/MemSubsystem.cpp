#include "MemSubsystem.h"
#include "SimpleCache.h"
#include <memory>

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

MemSubsystem::MemSubsystem(SimContext *ctx) : ctx(ctx) {
  dcache = std::make_unique<SimpleCache>(ctx);
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

  dcache->init();
}

void MemSubsystem::on_commit_store(uint32_t paddr, uint32_t data) {
  peripheral.on_commit_store(paddr, data);
}

void MemSubsystem::comb() {
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
  refresh_ptw_client_outputs();
}

void MemSubsystem::seq() {
  dcache->seq();
}
