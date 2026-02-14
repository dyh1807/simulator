#pragma once

#include "IO.h"
class PeripheralModel;

class AbstractDcache {
public:
  virtual ~AbstractDcache() {}

  MemReqIO *lsu_req_io = nullptr;
  MemReqIO *lsu_wreq_io = nullptr;
  MemRespIO *lsu_resp_io = nullptr;
  MemReadyIO *lsu_wready_io = nullptr;
  PeripheralModel *peripheral_model = nullptr;

  virtual void init() = 0;
  virtual void comb() = 0;
  virtual void seq() = 0;
};
