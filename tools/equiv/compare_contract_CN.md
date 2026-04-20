# AXI/LLC 等价验证 Compare Contract

本文件定义当前 `tools/equiv/` MVP 的比较边界。

## 当前强比较项

当前 comparator 做**同序列、按事件类型规范化后的强比较**，事件集合为：

- `READ_ACCEPT`
- `WRITE_ACCEPT`
- `READ_RESP`
- `WRITE_RESP`
- `MAINT_ACCEPT`
- `AXI_AR_HS`
- `AXI_AW_HS`
- `AXI_W_HS`
- `FINAL_MEM`
- `FINAL_MMIO`
- `FINAL_MAPPED`
- `MODE_ACTIVE`

这意味着：

- 事件类型必须一致
- 事件顺序必须一致
- 同一事件上的**规范化字段**必须一致

其中 `MAINT_ACCEPT` 当前只针对 **stimulus 显式请求** 的 maintenance：

- `invalidate_all_valid`
- `invalidate_line_valid`

内部 mode-transition flush 不计入 `MAINT_ACCEPT` compare。

## 当前未纳入比较的项

当前 MVP 明确不比较：

- lower AXI 原始 `id/size/burst/strb hash`
- 内部 table / queue / pending slot 状态

当前已经支持 **final DDR sample state** compare：

- seed 可通过 `final_mem_samples` 指定一组 DDR 地址
- harness 会根据：
  - `mem_read_line_resp`
  - `AXI_AW_HS`
  - `AXI_W_HS`
  维护 sample 级 shadow memory
- 仿真结束时输出：
  - `FINAL_MEM addr=... known=... val=...`

因此，仍未纳入比较的是：

- 非 sample 化的全 DDR 镜像状态

当前也已经支持 **final mapped-window sample state** compare：

- seed 可通过 `final_mapped_samples` 指定一组 mode2 local-window 地址
- harness 会在收尾时直接读取最终 resident store / valid RAM 状态
- 仿真结束时输出：
  - `FINAL_MAPPED addr=... known=... val=...`

当前也已经支持 **final MMIO sample state** compare：

- seed 可通过 `final_mmio_samples` 指定一组 MMIO 地址
- harness 会根据下游 `AW/W` 握手维护 sample 级 MMIO shadow state
- 仿真结束时输出：
  - `FINAL_MMIO addr=... known=... val=...`

因此，仍未纳入比较的是：

- 非 sample 化的全 mapped-window / DDR / MMIO 镜像状态

## 当前环境约束

当前 seed 生成和比较默认建立在下面这些约束上：

- 顶层事件前允许存在 `warmup_cycles`
- 不生成 overlapping same-master same-write-ID case
- 不生成依赖 `invalidate_line` accept policy 差异的 case
- 不生成依赖 parent host queued lookup hidden contract 的 case
- cacheable line fill 优先使用 `mem_read_line_resp`，而不是跨模型直接复用 raw `axi_r` beat

换句话说，当前 MVP 先验证：

- shared stimulus 在共同合同子集内，C++ reference 与 RTL 是否产出一致的上游可见行为

而不是一次覆盖所有边角合同。

## 已知合同差异的处理策略

### `invalidate_line`

当前已知：

- C++ 更接近 `line-local` accept
- RTL 当前更接近 `full compat-local drain`

在 compare harness 里，这一项暂时通过 **不生成相关冲突场景** 规避。
后续如果要把这类 case 纳入，需要显式引入 policy 参数，例如：

- `invalidate_line_accept_policy = line_local`
- `invalidate_line_accept_policy = full_drain`

### same-master same-write-ID reuse

当前这项政策还没有冻结成共同语义。
在 compare harness 里，暂时通过 stimulus 约束避免生成对应 case。

不过，当前已经把**串行复用**纳入共同合同：

- 同 master、同 `id`
- 第一笔事务已经完成并从上游可见接口退休
- 然后再发第二笔事务

当前默认 PASS 集已经覆盖：

- `mode1_bypass_read_id_reuse_serial`
- `mode1_mmio_write_id_reuse_serial`

仍然排除在共同合同外的是：

- overlapping same-master same-read-ID reuse
- overlapping same-master same-write-ID reuse

## 当前通过的 MVP seed

- `tests/equiv/seeds/mode1_bypass_rw.json`
- `tests/equiv/seeds/mode1_bypass_read_id_reuse_serial.json`
- `tests/equiv/seeds/invalidate_line_idle_accept.json`
- `tests/equiv/seeds/mode1_fill_then_bypass_hit.json`
- `tests/equiv/seeds/mode1_mmio_write.json`
- `tests/equiv/seeds/mode1_mmio_write_id_reuse_serial.json`
- `tests/equiv/seeds/mode_transition_flush_write_block.json`
- `tests/equiv/seeds/mode2_aligned_write.json`
- `tests/equiv/seeds/mode2_window_local_write.json`

这些 seed 的目标是：

- 先证明 shared-stimulus + C++ runner + RTL replay + comparator 这条链路可运行
- 并在当前共同合同子集内产出一致结果

## 当前探索性用例

- `tests/equiv/seeds/invalidate_all_idle_accept.json`

这些用例目前**不纳入默认 PASS 集**。它们正在暴露新的差异：

- `invalidate_all_idle_accept`
  - C++ 在持续拉高请求下现在只给一次 `MAINT_ACCEPT`
  - RTL 当前在同样 stimulus 下仍没有对应 `MAINT_ACCEPT`

这条差异现在已经收敛成 **maintenance accept policy** 问题，不再是 harness 对 pulse/level 语义处理不当。

## Compare policy 参数

当某条 seed 需要显式表达“双方都允许，但 accept/latency 政策不同”的差异时，可以在 seed JSON 中提供：

- `compare_policy.ignore_maint_accept_ops`

当前已使用的例子：

- `invalidate_all_idle_accept.json`
  - `ignore_maint_accept_ops = ["invalidate_all"]`

这表示：

- 该 seed 仍然用于验证其它上游/下游事件顺序
- 但不会把 `invalidate_all` 的 `MAINT_ACCEPT` 脉冲是否出现、何时出现，当成共同合同的一部分

这样做的前提是：这条差异已经被确认是**accept policy 差异**，而不是功能 bug。

## `mem_read_line_resp` 抽象

`mem_read_line_resp` 表示：

- 对“当前最老的 pending LLC read”返回一整条 line 的数据

它不会在 seed 中显式编码 raw `ARLEN/ARSIZE/RID/RLAST/RDATA` 的 beat 细节，而是由两侧 runner 各自展开：

- C++ runner：按 C++ interconnect 当前的下游 beat 宽度生成合法 `R` 流
- RTL replay bench：按 DUT 当前实际握手到的 `ARLEN/ARSIZE` 生成合法 `R` 流

引入这个抽象的原因是：

- C++ 当前的下游读 beat 粒度与 RTL 不同
- 直接共享 raw `axi_r` beat 会把“返回建模差异”误报成“缓存语义差异”

在 `mode1_fill_then_bypass_hit` 中，这个抽象已经证明能把同 line resident-fill / bypass-hit 场景稳定压进共同合同子集。

## 下一阶段计划约束

### DDR 侧 `AR/AW` 顺序约束

后续 shared-stimulus / compare contract 需要显式纳入下面这些约束，且 C++ / RTL 两边都应遵守：

- 上游发出某地址 `AR` 之后，在收到对应 `R` 完成前，不应再对同地址发 `AW`
- 反过来同理：某地址 `AW` 发出后，在收到对应 `B` 完成前，不应再对同地址发 `AR`
- 不允许依赖这种顺序：
  - `AR` 与 `AW`（顺序任意）都发出
  - 先收到 `B`
  - 清本地 buffer / owner
  - 之后才收到对应 `R`

原因不是 AXI 协议禁止，而是当前 DDR / ref-model / RTL 的一致性合同并没有冻结“未完成读写重叠时，同地址可见的是旧值还是新值”。

这类场景未来要么：

- 在 harness 里作为显式禁止约束
- 要么升级成共享内存一致性模型后，再正式纳入 compare

当前状态：

- 这组约束已经在 C++ runner 和 RTL replay bench 中作为**运行时 validator** 落地
- 一旦出现同地址 `AR/AW` overlap，harness 会直接 fail

## 当前规范化规则

为避免把不同抽象层的表示差异误判成语义差异，当前 comparator 使用以下规则：

- `MODE_ACTIVE`
  比较 `mode/offset`
- `READ_ACCEPT`
  比较 `master/id/addr/size/bypass`
- `WRITE_ACCEPT`
  比较 `master/id/addr/size/bypass/data0`
- `READ_RESP`
  当前只比较 `master/id/d0`
- `WRITE_RESP`
  比较 `master/id/code`
- `MAINT_ACCEPT`
  比较 `op/addr`
- `AXI_AR_HS`
  当前只比较 `addr`
- `AXI_AW_HS`
  当前只比较 `addr`
- `AXI_W_HS`
  当前只比较 `d0/last`
- `FINAL_MEM`
  比较 `addr/known/val`
- `FINAL_MMIO`
  比较 `addr/known/val`
- `FINAL_MAPPED`
  比较 `addr/known/val`

换句话说，当前 MVP 已经把下游 AXI issue 纳入 compare，但还没有把所有 raw AXI 字段都冻结成强等价项。
