#include "AXI_Interconnect.h"
#include "SimDDR.h"
#include <cstdio>

namespace {
constexpr uint32_t kDemoMemWords = 0x100000;
uint32_t g_demo_mem[kDemoMemWords] = {};
} // namespace

uint32_t *p_memory = g_demo_mem;
long long sim_time = 0;

namespace {

void wire_ddr_to_intlv(axi_interconnect::AXI_Interconnect &intlv,
                       sim_ddr::SimDDR &ddr) {
  intlv.axi_io.ar.arready = ddr.io.ar.arready;
  intlv.axi_io.r.rvalid = ddr.io.r.rvalid;
  intlv.axi_io.r.rid = ddr.io.r.rid;
  intlv.axi_io.r.rdata = ddr.io.r.rdata;
  intlv.axi_io.r.rlast = ddr.io.r.rlast;
  intlv.axi_io.r.rresp = ddr.io.r.rresp;
  intlv.axi_io.aw.awready = ddr.io.aw.awready;
  intlv.axi_io.w.wready = ddr.io.w.wready;
  intlv.axi_io.b.bvalid = ddr.io.b.bvalid;
  intlv.axi_io.b.bid = ddr.io.b.bid;
  intlv.axi_io.b.bresp = ddr.io.b.bresp;
}

void wire_intlv_to_ddr(axi_interconnect::AXI_Interconnect &intlv,
                       sim_ddr::SimDDR &ddr) {
  ddr.io.ar.arvalid = intlv.axi_io.ar.arvalid;
  ddr.io.ar.araddr = intlv.axi_io.ar.araddr;
  ddr.io.ar.arid = intlv.axi_io.ar.arid;
  ddr.io.ar.arlen = intlv.axi_io.ar.arlen;
  ddr.io.ar.arsize = intlv.axi_io.ar.arsize;
  ddr.io.ar.arburst = intlv.axi_io.ar.arburst;

  ddr.io.aw.awvalid = intlv.axi_io.aw.awvalid;
  ddr.io.aw.awaddr = intlv.axi_io.aw.awaddr;
  ddr.io.aw.awid = intlv.axi_io.aw.awid;
  ddr.io.aw.awlen = intlv.axi_io.aw.awlen;
  ddr.io.aw.awsize = intlv.axi_io.aw.awsize;
  ddr.io.aw.awburst = intlv.axi_io.aw.awburst;

  ddr.io.w.wvalid = intlv.axi_io.w.wvalid;
  ddr.io.w.wdata = intlv.axi_io.w.wdata;
  ddr.io.w.wstrb = intlv.axi_io.w.wstrb;
  ddr.io.w.wlast = intlv.axi_io.w.wlast;

  ddr.io.r.rready = intlv.axi_io.r.rready;
  ddr.io.b.bready = intlv.axi_io.b.bready;
}

void clear_master_inputs(axi_interconnect::AXI_Interconnect &intlv) {
  for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
    intlv.read_ports[i].req.valid = false;
    intlv.read_ports[i].resp.ready = false;
  }
  for (int i = 0; i < axi_interconnect::NUM_WRITE_MASTERS; i++) {
    intlv.write_ports[i].req.valid = false;
    intlv.write_ports[i].resp.ready = false;
  }
}

} // namespace

int main() {
  axi_interconnect::AXI_Interconnect intlv;
  sim_ddr::SimDDR ddr;
  intlv.init();
  ddr.init();

  auto &port = intlv.read_ports[axi_interconnect::MASTER_ICACHE];
  bool accepted = false;
  bool ar_issued = false;
  bool responded = false;

  for (int cycle = 0; cycle < 4000 && !responded; cycle++) {
    clear_master_inputs(intlv);
    if (!ar_issued) {
      port.req.valid = true;
      port.req.addr = 0x1000;
      port.req.total_size = 31;
      port.req.id = 1;
    }
    port.resp.ready = true;

    ddr.comb_outputs();
    wire_ddr_to_intlv(intlv, ddr);
    intlv.comb_outputs();
    intlv.comb_inputs();
    wire_intlv_to_ddr(intlv, ddr);
    ddr.comb_inputs();

    accepted = accepted || (port.req.valid && port.req.ready);
    ar_issued = ar_issued || (intlv.axi_io.ar.arvalid && intlv.axi_io.ar.arready);
    responded = port.resp.valid;

    ddr.seq();
    intlv.seq();
  }

  if (!accepted || !ar_issued || !responded) {
    std::printf("AXI4 smoke demo failed: accepted=%d ar_issued=%d responded=%d\n",
                accepted ? 1 : 0, ar_issued ? 1 : 0, responded ? 1 : 0);
    return 1;
  }

  std::printf("AXI4 smoke demo passed\n");
  return 0;
}
