---
name: two-phase-debug
description: "Debug checklist for two-phase comb_outputs/comb_inputs + seq timing issues in this simulator (ready-first handshake, latches, response lifetime)."
---

# Two-phase 时序/握手 Debug 清单

适用场景：
- 开启/替换 SimDDR 后 difftest 出错、卡死、性能突变
- ready/valid 相关的“偶现”问题（通常是 two-phase 连接顺序或寄存器生命周期问题）

## 1. 先确认执行顺序是否正确
典型顺序（每周期）：
1) `mem_subsystem.comb_outputs()`（清 upstream 输入、ddr->intlv wiring、intlv 输出）
2) `cpu.cycle()`（master 驱动 req/resp.ready）
3) `mem_subsystem.comb_inputs()`（intlv 处理 req、驱动 ddr 输入）
4) `mem_subsystem.seq()`（所有寄存器更新）

如果你把 comb_outputs/comb_inputs 调用顺序弄反，最常见现象是：
- master 永远看不到 `req.ready` 或 `resp.valid`（错一个周期）
- latch/backpressure 逻辑表现异常

## 2. ready-first 常见坑
症状：master 只拉 `valid` 一个周期就掉（或被清空），但 interconnect 还没把 AR/AW 发出去。
修复思路：
- `req.ready` 用寄存器打一拍（ready-first），并在 comb_inputs 中优先完成“上一拍 ready 的握手”
- 测试里要允许“ready 先出现 → 下一拍才真正发 AXI AR/AW”

## 3. latch/backpressure 常见坑
症状：下游 `*_ready=0` 时，`*_valid` 掉了，导致 AXI 侧违反协议/丢请求。
修复思路：
- AR/AW 输出加 latch：当 `valid && !ready` 时，把 `addr/len/id/...` 锁存到寄存器
- latch 存在时：永远输出 latch 的值并保持 `valid=1`，直到 ready

## 4. “同周期产生又同周期清掉”的坑
症状：上游 `resp.ready=1`，bridge 在某周期生成 `resp.valid=1`，但 `seq()` 里同周期就清掉，导致上游看不到 resp。
修复思路：
- 在 `seq()` 入口处 snapshot `resp_valid_curr`，只对“上一周期可见的 resp_valid”处理握手清除

## 5. 定位手段（推荐最短路径）
- 先跑单测：`sim_ddr_test` / `axi_interconnect_test` / `*_axi3_test`
- 需要更多信息时：
  - 在 interconnect 输出指令/数据给上游前增加断言/打印（例如比较 `dut_mem[pc]` vs `resp.data[0]`）
  - 记录 AXI 侧 AR/AW/W/R/B 的 handshake events（地址、ID、len、last）

