---
name: axi-bridge-testing
description: "Test patterns for AXI/handshake bridges: ready/valid backpressure, ID packing, beat splitting/merging, alignment, and randomized stress against a reference model."
---

# AXI/握手桥测试模板（Interconnect/Bridge）

适用场景：
- 写 interconnect/bridge 的功能测试、压力测试
- 定位 difftest 类问题（“ref inst 对，dut inst 错”经常是取指/读数据拼接、握手时序、resp 生命周期问题）

## 1. 推荐的 test harness 结构（two-phase）
每周期拆成两段：
1) **Phase 1（outputs）**：`ddr.comb_outputs()` → wire 到 `intlv.axi_io.*` → `intlv.comb_outputs()`  
2) **Phase 2（inputs）**：驱动上游 `req/ready` → `intlv.comb_inputs()` → 记录 AXI 侧握手事件 → wire 到 `ddr.io.*` → `ddr.seq()` + `intlv.seq()`

参考实现：
- `axi_interconnect/axi_interconnect_test.cpp`
- `axi_interconnect/axi_interconnect_axi3_test.cpp`

## 2. 必测点清单（按“容易出 bug”排序）
- **ready-first + latch**：下游 `arready/awready=0` 时，`arvalid/awvalid` 不得掉
- **R/B backpressure**：
  - 测试时把 `rready/bready` 提前拉低（在 `rvalid/bvalid` 出现前），避免“valid 首次出现即被消费”导致误判
  - backpressure 期间 data/ID 必须稳定
- **对齐与跨 beat**：
  - `addr` 非 beat 对齐（offset 1..31）
  - `offset + bytes > beat_bytes` 的跨 beat（至少覆盖 2 beat）
- **ID 路由**：多 master 同时请求时，resp 只能回到对应 master，且 `resp.id` 保持上游 ID
- **串行/并行假设一致**：
  - 如果 bridge 设计为“单 outstanding read/write”，测试不要假设三路请求都 accept 后再统一等 resp
  - 推荐写成“持续发请求直到 accepted，同时 resp 出现就立即消费”

## 3. 建议加入的观测数据（events）
建议在 test 里记录：
- AR/AW handshake：`addr/id/len/size/burst`
- W handshake：`data/strb/last`
- R/B handshake：`id/last/resp`

用于检查：
- burst 拆分是否正确
- ID packing/decoding 是否正确
- backpressure 下 valid/数据是否保持

## 4. byte 级 reference model（强烈推荐）
对于 `total_size<=32B` 的上游接口：
- 用 `std::vector<uint8_t> ref_mem` 维护期望内存镜像
- 写：按 `wstrb` 更新 `ref_mem[addr+i]`
- 读：组装 `bytes=total_size+1`，再打包回 256b/32b 的 `resp.data`

参考实现：
- `axi_interconnect/axi_interconnect_axi3_test.cpp`（随机压力 + byte 参考）

## 5. 随机压力（最小可用版本）
建议 500~2000 次迭代，随机：
- master（读）
- `addr`（注意边界与对齐）
- `total_size`（覆盖 1/2/4/8/16/32B）
- `wstrb`（避免全 0）
- `resp.ready` backpressure（0~2 个周期）

当发现 mismatch：
- 先打印：`addr/size/offset/beats/id/expected/got`
- 再回放：可把随机 seed 固定，定位到最小失败 case

