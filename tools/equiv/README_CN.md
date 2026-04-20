# AXI/LLC C++ vs RTL 等价验证 MVP

本目录放：

- shared stimulus 生成器
- C++ reference runner
- RTL replay/trace 运行脚本
- trace comparator
- compare contract 文档

当前目标是做一个可运行的 MVP，而不是一次做完完整 fuzz 框架。

## 当前范围

MVP 当前覆盖：

- shared stimulus
- C++ runner
- RTL replay TB
- normalized trace compare

第一阶段比较：

- `READ_ACCEPT`
- `WRITE_ACCEPT`
- `READ_RESP`
- `WRITE_RESP`
- `MAINT_ACCEPT`
- `AXI_AR_HS`
- `AXI_AW_HS`
- `AXI_W_HS`
- `MODE_ACTIVE`

当前不比较：

- raw `axi_id/size/burst/strb hash`
- final DDR/MMIO state

这些会在后续阶段扩展。

当前 stimulus 额外支持：

- `warmup_cycles`
- `hold_until_accept`

其中 `warmup_cycles` 用来吸收 RTL 顶层 reset 后的 invalidate sweep / reconfig busy 初始化窗口。

## 用法

从父仓库根目录运行：

```bash
python3 tools/equiv/run_mvp.py
```

可指定 seed：

```bash
python3 tools/equiv/run_mvp.py --seed tests/equiv/seeds/mode1_bypass_rw.json
```

默认 RTL 在 `eda-03` 上用 VCS 重放。

当前默认通过的 seed：

- `tests/equiv/seeds/mode1_bypass_rw.json`
- `tests/equiv/seeds/invalidate_line_idle_accept.json`
- `tests/equiv/seeds/mode_transition_flush_write_block.json`
- `tests/equiv/seeds/mode2_aligned_write.json`

当前还有一条**探索性** seed：

- `tests/equiv/seeds/mode1_fill_then_bypass_hit.json`

这条用例当前会暴露一条新的 C++/RTL 行为差异：

- RTL 在 cacheable fill 完成后会先返回第一次 `READ_RESP`，随后同 line 的 bypass read 走 resident hit
- C++ reference 当前没有在同样时序下给出第一次 `READ_RESP`

合同边界见：

- `tools/equiv/compare_contract_CN.md`

如需用独立的 submodule worktree 做 C++/RTL 双侧构建，可显式指定：

```bash
python3 tools/equiv/run_mvp.py \
  --submodule-root /path/to/axi-interconnect-kit-worktree
```

## 目录说明

- `cpp/`
  C++ reference runner
- `generated/`
  由 seed 自动生成的 `case_data.h` / `equiv_case.vh`
- `out/`
  每个 seed 的 build / trace / compare 结果
