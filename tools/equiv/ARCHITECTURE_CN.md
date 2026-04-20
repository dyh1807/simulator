# AXI/LLC Equivalence Harness 架构说明

本文说明当前 `tools/equiv/` harness 的实际工作方式。

## 目标

这套 harness 的目标不是形式化证明，而是：

- 用同一份 stimulus 同时驱动 C++ reference 和 RTL
- 把两侧行为压缩成同一套规范化 trace
- 在当前 compare contract 定义的共同语义子集内，尽早发现 C++ / RTL 偏差

## 总体流程

```text
             +----------------------+
             |   seed JSON input    |
             |  tests/equiv/seeds   |
             +----------+-----------+
                        |
                        v
             +----------------------+
             |    gen_case.py       |
             |  shared case emit    |
             +----------+-----------+
                        |
          +-------------+-------------+
          |                           |
          v                           v
+----------------------+   +-----------------------------+
| case_data.h          |   | equiv_case.vh              |
| C++ shared stimulus  |   | RTL shared stimulus        |
+----------+-----------+   +-------------+---------------+
           |                               |
           v                               v
+----------------------+   +-----------------------------+
| equiv_case_runner    |   | tb_axi_llc_subsystem_       |
| C++ reference runner |   | equiv_replay.v              |
+----------+-----------+   +-------------+---------------+
           |                               |
           v                               v
+----------------------+   +-----------------------------+
| cpp_trace.txt        |   | rtl_trace.txt              |
| normalized events    |   | normalized events          |
+----------+-----------+   +-------------+---------------+
           \                               /
            \                             /
             v                           v
               +----------------------+
               |   compare_trace.py   |
               | canonical compare    |
               +----------+-----------+
                          |
                          v
               +----------------------+
               | PASS / first diff    |
               +----------------------+
```

## 各模块职责

### 1. `gen_case.py`

输入：

- 一份 seed JSON

输出：

- `case_data.h`
- `equiv_case.vh`
- `case_meta.json`

作用：

- 把 seed 里的事件展平成 cycle-by-cycle 的共享刺激
- 确保 C++ 和 RTL 看到的是同一份 stimulus
- 把 `final_mem_samples` / `final_mmio_samples` / `final_mapped_samples` 一起编进 case

### 2. `equiv_case_runner.cpp`

作用：

- 直接实例化并运行 C++ reference
- 根据 case 驱动上游请求、维护下游返回、记录可见事件
- 在 C++ 侧维护 sample 级 shadow state：
  - DDR
  - MMIO
  - mapped-window
- 输出规范化 trace

### 3. `tb_axi_llc_subsystem_equiv_replay.v`

作用：

- 在 RTL 上重放同一份 stimulus
- 从 DUT 的上游/下游接口采样事件
- 在 RTL 侧维护与 C++ 对应的 sample 级 final state
- 输出同样格式的规范化 trace

### 4. `compare_trace.py`

作用：

- 读取两侧 trace
- 按事件类型做 canonicalize
- 做严格的顺序比较
- 第一处不一致时给出：
  - first diff index
  - normalized 结果
  - 原始 trace 行

## 现在到底比较什么

当前强比较事件见：

- [compare_contract_CN.md](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/compare_contract_CN.md)

当前主集合是：

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

换句话说，这套 harness 现在同时覆盖：

- 上游可见行为
- 下游 issue 行为
- 最终 sample state

但它还**没有**比较：

- raw AXI 全字段
- 内部表项、队列、pending slot 的完整内部状态
- 全量 DDR / MMIO / mapped-window 全镜像

## 运行时约束检查

这套 harness 不只是盲目 compare，还会做环境约束检查。

当前已经强制：

- 同地址 `AR` 未完成前，不允许发同地址 `AW`
- 同地址 `AW` 未完成前，不允许发同地址 `AR`

一旦违反，测试直接 fail。

因此它不是“把任何 stimulus 都拿来比”，而是只在当前定义好的合同范围内做对拍。

## 默认回归与随机 smoke

### 默认 directed 回归

入口：

- [run_mvp.py](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/run_mvp.py)

特点：

- 跑固定 seed 集
- 用于锁住已知 bug 和已收敛合同

### 受约束随机 smoke

入口：

- [run_random_smoke.py](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/run_random_smoke.py)

特点：

- 生成可复现的随机 seed
- 但只在当前 compare contract 的共同合同子集内随机组合
- 当前仍然避免：
  - overlapping same-master same-ID reuse
  - 同地址 `AR/AW` overlap
  - mode2 写后继续拼接尚未冻结的后续组合语义

## 这套 harness 的能力边界

它当前能回答的是：

- 在当前共同合同子集内，C++ 和 RTL 是否给出相同外部行为
- 一些 latent bug 是否能通过 shared-stimulus 直接复现

它当前不能回答的是：

- “C++ 一定完全正确”
- “RTL 在所有输入空间都正确”
- “这等价于形式化证明”

更准确的定位是：

- 强回归
- 强对拍
- 非 formal

## 相关文件

- [README_CN.md](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/README_CN.md)
- [compare_contract_CN.md](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/compare_contract_CN.md)
- [stimulus_schema_CN.md](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/stimulus_schema_CN.md)
- [run_mvp.py](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/run_mvp.py)
- [run_random_smoke.py](/nfs_global/S/daiyihao/project/qm-rocky/dev/addr_map_dev/local_debug/simulator_main_8368196_wt/tools/equiv/run_random_smoke.py)
