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
- `MODE_ACTIVE`

当前不比较：

- raw `axi_id`
- lower AXI 全路径 normalized trace
- final DDR/MMIO state

这些会在后续阶段扩展。

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
- `tests/equiv/seeds/mode_transition_flush_write_block.json`

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
