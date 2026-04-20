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
- `FINAL_MEM`
- `FINAL_MMIO`
- `FINAL_MAPPED`

当前不比较：

- raw `axi_id/size/burst/strb hash`

当前已经支持 **final DDR sample state** compare：

- seed 可以通过 `final_mem_samples` 指定一组 DDR 地址
- runner / RTL replay 会根据 `mem_read_line_resp` 与下游 `AW/W` 握手维护 shadow memory
- 仿真结束时会输出 `FINAL_MEM`

当前也已经支持 **final mapped-window sample state** compare：

- seed 可以通过 `final_mapped_samples` 指定一组 mode2 local-window 地址
- harness 会在收尾时直接读取 resident generic store / valid RAM 的最终值
- 仿真结束时输出：
  - `FINAL_MAPPED`

这些能力当前主要用于：

- `mode1_bypass_rw`
- `mode1_bypass_read_id_reuse_serial`
- `mode1_fill_then_bypass_hit`
- `mode1_mmio_write`
- `mode1_mmio_write_id_reuse_serial`
- `mode2_aligned_write`
- `mode2_window_local_write`

当前也已经支持 **final MMIO sample state** compare：

- seed 可以通过 `final_mmio_samples` 指定一组 MMIO 地址
- harness 会根据下游 `AW/W` 握手维护 MMIO sample shadow state
- 仿真结束时输出：
  - `FINAL_MMIO`

当前 stimulus 额外支持：

- `warmup_cycles`
- `hold_until_accept`
- `mem_read_line_resp`
- `final_mem_samples`
- `final_mmio_samples`
- `final_mapped_samples`

其中 `warmup_cycles` 用来吸收 RTL 顶层 reset 后的 invalidate sweep / reconfig busy 初始化窗口。

当前 replay harness 还会在 C++ runner 和 RTL bench 两侧同时执行一条运行时约束检查：

- DDR 侧同地址 `AR` 未完成前，不允许再发同地址 `AW`
- DDR 侧同地址 `AW` 未完成前，不允许再发同地址 `AR`

一旦违反，这条 seed 会直接失败，而不是继续做 trace compare。

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

也可以跑一批受当前 compare contract 约束的小规模随机 smoke：

```bash
python3 tools/equiv/run_random_smoke.py --count 8 --root-seed 20260420
```

这条脚本会：

- 生成一组可复现的随机 seed
- 约束在当前共同合同子集内
- 复用 `run_mvp.py` 做 C++ / RTL 对拍

生成出来的随机 seed 与 manifest 会写到：

- `tools/equiv/out/random_smoke/`

当前默认通过的 seed：

- `tests/equiv/seeds/mode1_bypass_rw.json`
- `tests/equiv/seeds/mode1_bypass_read_id_reuse_serial.json`
- `tests/equiv/seeds/invalidate_line_idle_accept.json`
- `tests/equiv/seeds/mode1_fill_then_bypass_hit.json`
- `tests/equiv/seeds/mode1_mmio_write.json`
- `tests/equiv/seeds/mode1_mmio_write_id_reuse_serial.json`
- `tests/equiv/seeds/mode_transition_flush_write_block.json`
- `tests/equiv/seeds/mode2_aligned_write.json`
- `tests/equiv/seeds/mode2_window_local_write.json`

当前还显式覆盖了共同合同内的 **serial ID reuse**：

- bypass read：同 master、同 `id`，第一笔 `READ_RESP` 完成后再发第二笔
- MMIO write：同 master、同 `id`，第一笔 `WRITE_RESP/B` 完成后再发第二笔

仍然没有纳入默认 PASS 集的，是 **overlapping same-master same-ID reuse**。

随机 smoke 当前也沿用同样限制：

- 允许 **serial** ID reuse
- 不生成 overlapping same-master same-ID reuse
- 不生成同地址 `AR/AW` overlap
- 只拼接当前已经冻结到共同合同内的操作模板
- 如需包含 mode2 写模板，只会放在 seed 尾部，不再在它后面继续拼接新 op

当前还有一条**策略化** seed：

- `tests/equiv/seeds/invalidate_all_idle_accept.json`

这条用例当前暴露的是已确认的 policy 差异：

- `invalidate_all_idle_accept`
  - C++ 现在会对持续拉高的 `invalidate_all` 只给一次 `MAINT_ACCEPT`
  - RTL 在同样 stimulus 下仍没有对应 `MAINT_ACCEPT`
  - 这说明当前剩下的是 **accept policy / sweep timing** 差异，不再是 pulse/level 或重复 accept 的 bug
  - 该 seed 在 JSON 内通过 `compare_policy.ignore_maint_accept_ops=["invalidate_all"]`
    显式把这项 accept pulse 排除在共同合同外

`mode1_fill_then_bypass_hit` 现在已经进入默认 PASS 集。为了让 cacheable fill 的下游返回在 C++/RTL 两侧都走“共同抽象”，harness 额外提供了：

- `mem_read_line_resp`

它表示“给当前最老的 pending LLC read 返回一整条 line 数据”，由 C++ runner 和 RTL replay bench 各自按本模型的 AXI beat 宽度自动展开成合法的下游 `R` 流，而不是直接把 raw `axi_r` beat 跨模型复用。

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
