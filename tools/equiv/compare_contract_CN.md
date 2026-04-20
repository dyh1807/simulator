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
- final DDR / MMIO / mapped-window memory state
- 内部 table / queue / pending slot 状态

这些属于下一阶段扩展项。

## 当前环境约束

当前 seed 生成和比较默认建立在下面这些约束上：

- 顶层事件前允许存在 `warmup_cycles`
- 不生成 overlapping same-master same-write-ID case
- 不生成依赖 `invalidate_line` accept policy 差异的 case
- 不生成依赖 parent host queued lookup hidden contract 的 case

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

## 当前通过的 MVP seed

- `tests/equiv/seeds/mode1_bypass_rw.json`
- `tests/equiv/seeds/invalidate_line_idle_accept.json`
- `tests/equiv/seeds/mode_transition_flush_write_block.json`
- `tests/equiv/seeds/mode2_aligned_write.json`

这些 seed 的目标是：

- 先证明 shared-stimulus + C++ runner + RTL replay + comparator 这条链路可运行
- 并在当前共同合同子集内产出一致结果

## 当前探索性用例

- `tests/equiv/seeds/mode1_fill_then_bypass_hit.json`

这条用例目前**不纳入默认 PASS 集**。它正在暴露一条新的差异：

- RTL 已经表现出 `cacheable fill -> first READ_RESP -> same-line bypass hit`
- C++ reference 当前在相同时序下没有产出第一次 `READ_RESP`

后续需要先把这条差异定位清楚，再决定是修 reference、修 RTL，还是进一步收紧 stimulus / compare contract。

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

换句话说，当前 MVP 已经把下游 AXI issue 纳入 compare，但还没有把所有 raw AXI 字段都冻结成强等价项。
