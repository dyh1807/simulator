# AXI/LLC 等价验证 Compare Contract

本文件定义当前 `tools/equiv/` MVP 的比较边界。

## 当前强比较项

当前 comparator 做逐行强比较，事件集合为：

- `READ_ACCEPT`
- `WRITE_ACCEPT`
- `READ_RESP`
- `WRITE_RESP`
- `MAINT_ACCEPT`
- `MODE_ACTIVE`

这意味着：

- 事件类型必须一致
- 事件顺序必须一致
- 同一事件上的 `master/id/addr/size/bypass/data hash` 必须一致

## 当前未纳入比较的项

当前 MVP 明确不比较：

- lower AXI 原始 `AR/AW/W/B/R` 轨迹
- final DDR / MMIO / mapped-window memory state
- 内部 table / queue / pending slot 状态

这些属于下一阶段扩展项。

## 当前环境约束

当前 seed 生成和比较默认建立在下面这些约束上：

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
- `tests/equiv/seeds/mode_transition_flush_write_block.json`

这两条 seed 的目标是：

- 先证明 shared-stimulus + C++ runner + RTL replay + comparator 这条链路可运行
- 并在当前共同合同子集内产出一致结果
