# AXI Interconnect LLC Design

## 1. 当前代码约束

### 1.1 interconnect 现状

- `AXI_Interconnect` 当前本质上是一个上游简化端口到下游 AXI 的桥，负责仲裁、ready-first 握手、AXI `AR/AW` 锁存和响应分发，本身不维护 cache 状态。
- 通用上游接口容量是 `read_ports[4] / write_ports[2]`。
- 当前准备采用的端口映射是：读侧 `icache / dcache_r / uncore_lsu_r / extra_r`，写侧 `dcache_w / uncore_lsu_w`。
- 上游读响应最大可返回 `256B`，但上游写负载目前只有 `256-bit = 32B`。
- 当前读路径允许不同 master 并行挂起，但同一个 read master 在已有 pending 响应未消费前不能再发第二个请求；写路径更严格，基本是全局单 active write。

对应代码：

- `simulator/axi-interconnect-kit/axi_interconnect/include/AXI_Interconnect_IO.h`
- `simulator/axi-interconnect-kit/axi_interconnect/AXI_Interconnect.cpp`

### 1.1.1 当前 non-blocking 边界

当前 interconnect 不是“每个 master 完全非阻塞”的实现：

- read side:
  - 不同 master 之间可以并行挂起
  - 同一个 master 会被 `has_pending` 检查挡住，直到前一个响应被真正消费
- write side:
  - 目前基本只有一个全局 active write

因此，当前语义不是“只在请求发起握手时阻塞”，而是“同一 master 在前一个事务返回并消费完成前持续阻塞”。
如果 LLC 想真正提升性能，必须把“请求接收”和“响应返回”用可配置的 `MSHR` 解耦。

### 1.2 MemSubsystem 接线现状

- 当前 AXI-kit 只真实接了 `icache` 读口。
- `dcache` 仍是 `SimpleCache`，直接访问 `p_memory` / `peripheral_model`，没有经过 `axi-interconnect`。
- `MemSubsystem::comb()` 里除 `MASTER_ICACHE` 外，其它 `read_ports` 和所有 `write_ports` 都被强制清零。

对应代码：

- `simulator/MemSubSystem/MemSubsystem.cpp`
- `simulator/MemSubSystem/SimpleCache.cpp`

### 1.3 可复用的 icache 外置 table 机制

- `icache` 已经把 `data/tag/valid` 表抽成了 `GenericTable`。
- `GenericTable` 同时支持 `RegfileTablePolicy` 和 `SramTablePolicy`，天然适合复用到 LLC 的 tag/data/meta/repl table。
- 但当前 `GenericTable` 的 geometry 是模板常量，payload 存储也以 `32-bit word` 为基本粒度；这对“WAY/SET 运行时可配置”和“大容量 LLC 的紧凑 meta/data 组织”都不够理想。

对应代码：

- `simulator/front-end/icache/GenericTable.h`
- `simulator/front-end/icache/icache.cpp`

### 1.4 对 GenericTable 的前置调整建议

推荐先升级 `GenericTable`，再在其上开发 LLC，而不是为 LLC 单独再造一套 table：

- geometry 支持运行时配置：
  - `rows`
  - `chunks_per_row`
  - `chunk_bytes` 或 `chunk_bits`
- payload 改成 byte-granular 存储，而不是固定按 `uint32_t` 对齐
- timing config 保持现有 `Regfile/Sram` 模式，但参数改成运行时可配

为了不破坏现有 `icache`，建议做成：

- 保留当前模板 API 作为兼容 facade
- 底层新增统一的动态存储后端

这样 LLC 可以直接建立在“更新后的 GenericTable”之上，同时 `icache` 也能渐进迁移。

## 2. 设计目标

目标是在 `axi-interconnect` 层引入一个共享的 `unified LLC`：

- 位于上游 master 端口与下游 AXI/DDR/MMIO 之间。
- 逻辑上作为 `icache` 的 L2、未来 `dcache L2` 的下级共享 cache。
- 采用 `PIPT`。
- 默认 `NINE`，不做包含性维护，不向上层发回 invalidation。
- 支持 `uncache/MMIO` 请求绕过 LLC。
- WAY/SET/容量应支持动态配置，`8MB / 16MB` 只作为默认 preset，不是唯一选项。
- 查表方式复用 `icache` 的外置 table + SRAM latency 风格。
- 代码组织上先新增 LLC 类文件，再在 `AXI_Interconnect` / `AXI_Interconnect_AXI3` 中实例化。
- miss 处理必须从第一版开始按“可扩展 MSHR”建模，而不是写死单 miss 全阻塞状态机。

## 3. 推荐总体架构

### 3.1 分层方式

推荐把 interconnect 从“纯桥”扩成三层：

1. 上游请求捕获/仲裁层
2. LLC 核心层
3. 下游协议桥层（AXI4 或 AXI3）

建议新增文件：

- `simulator/axi-interconnect-kit/axi_interconnect/include/AXI_LLC.h`
- `simulator/axi-interconnect-kit/axi_interconnect/AXI_LLC.cpp`
- `simulator/axi-interconnect-kit/axi_interconnect/include/AXI_LLC_Config.h`

不建议把 LLC 直接散写进 `AXI_Interconnect.cpp`，否则 AXI4/AXI3 会复制两套 miss/refill/evict 状态机，后续维护代价很高。

### 3.2 类职责

`AXI_LLC` 只做协议无关的 cache 行为：

- lookup / hit / miss 判定
- refill / eviction / writeback 状态机
- meta/data/replacement table 读写
- 上游 master 响应生成
- bypass 决策

`AXI_Interconnect` / `AXI_Interconnect_AXI3` 保留：

- 上游端口 ready/valid 语义
- 下游 AXI 通道握手
- LLC miss/writeback 到 AXI 事务的翻译

这要求在 interconnect 和 LLC 之间定义一组内部协议无关端口，例如：

- `LlcUpstreamReq`
- `LlcUpstreamResp`
- `LlcDownstreamReadReq/Resp`
- `LlcDownstreamWriteReq/Resp`

### 3.3 generalized-io 组织方式

LLC 建议严格参考 `icache_module` 的 generalized-io 风格，按 `3` 组输入和 `3` 组输出组织，而不是按 `req`/`mem`/`table` 平铺。

更准确的划分应为：

- Inputs
  - `ext_in`
    - 外部输入
    - 包含上游 master 输入和下游 memory 返回输入
  - `regs`
    - LLC 内部固有寄存器
  - `lookup_in`
    - 查表反馈输入
- Outputs
  - `ext_out`
    - 对外输出
    - 包含上游 master 输出和下游 memory 请求输出
  - `reg_write`
    - 内部固有寄存器下一步更新值
    - 类型与 `regs` 相同
  - `table_out`
    - 查表请求/写表控制输出

因此，`Req/Mem` 的 `in/out` 不应继续拆成彼此平级的多个一级字段；更好的方式是先按“外部接口边界”包装成 `ext_in/ext_out`，再在内部按 upstream/downstream 分子 struct。

建议 IO 形态：

```cpp
struct AXI_LLC_ExtIn_t {
  LlcUpstreamIn_t upstream;
  LlcMemIn_t mem;
};

struct AXI_LLC_ExtOut_t {
  LlcUpstreamOut_t upstream;
  LlcMemOut_t mem;
};

struct AXI_LLC_IO_t {
  AXI_LLC_ExtIn_t ext_in;
  LlcRegs_t regs;
  LlcLookupIn_t lookup_in;

  AXI_LLC_ExtOut_t ext_out;
  LlcRegs_t reg_write;
  LlcTableOut_t table_out;
};
```

其中：

- `ext_in.upstream`
  - 各 master 的 `req.valid/addr/id/size/bypass`
  - 各 master 对响应的 `resp.ready`
- `ext_in.mem`
  - refill / writeback 返回
  - 下游 ready / resp / data
- `ext_out.upstream`
  - 各 master 的 `req.ready`
  - 各 master 的 `resp.valid/data/id`
- `ext_out.mem`
  - 向下游发的 refill / writeback 请求

这种包装方式的好处是：

- 和 `icache_module` 的 `in / regs / lookup_in / out / reg_write / table_write` 一一对应
- `Req/Mem` 的方向归属更清楚，不会在 IO 顶层散成太多字段
- 未来扩多 `MSHR`、多 memory pipeline 或多个外部端口时，`ext_in/ext_out` 的层次更稳定

## 4. LLC 组织方式

### 4.1 行大小

建议 LLC line size 固定为 `64B`，与当前 `ICACHE_LINE_SIZE` / `DCACHE_LINE_SIZE` 对齐。

原因：

- 当前全局配置已经是 `64B` line。
- `icache` refill 粒度就是整行。
- 后续 `dcache` 若接入 LLC，也不需要做子行拼接。

应加入静态约束：

- `AXI_LLC_LINE_SIZE == ICACHE_LINE_SIZE`
- `AXI_LLC_LINE_SIZE == DCACHE_LINE_SIZE`
- `AXI_LLC_LINE_SIZE` 为 2 的幂

### 4.2 容量和相联度

推荐 LLC geometry 通过运行时 `AXI_LLCConfig` 注入：

- `size_bytes`
- `line_bytes`
- `ways`
- `mshr_num`

其中：

- `sets = size_bytes / (line_bytes * ways)`
- `ways`、`sets`、`line_bytes` 都必须是 2 的幂

推荐默认值：

- `AXI_LLC_SIZE_BYTES = 8 * 1024 * 1024`
- `AXI_LLC_WAYS = 16`
- `AXI_LLC_LINE_SIZE = 64`
- `AXI_LLC_MSHR_NUM = 4`

派生结果：

- `8MB, 16-way, 64B line`: `8192 sets`
- `16MB, 16-way, 64B line`: `16384 sets`

为什么默认仍推荐 `16-way`：

- 容量较大时能显著降低 set 数，减少外置 table 的 row 数。
- 相比 `8-way`，大共享 LLC 的冲突 miss 更温和。
- `tree-PLRU` 代价仍可控，16-way 只需 `15` 个状态 bit / set。

### 4.3 基于更新后 GenericTable 的 table 切分

在“升级后的 GenericTable”上，LLC 不建议机械复刻 `icache` 的 `data/tag/valid` 三分法；更适合：

1. `DataTable`
2. `MetaTable`
3. `ReplTable`

其中：

- `DataTable`
  - 每个 row 是一个 set
  - 每个 chunk 是一个 way
  - 每个 chunk 宽度是 `64B = 512b`
- `MetaTable`
  - 每个 row 是一个 set
  - 每个 chunk 是一个 way
  - 每个 chunk 建议固定 `32b`
  - 打包：`valid/dirty/tag/状态保留位`
- `ReplTable`
  - 每个 row 是一个 set
  - 一个 chunk 即可
  - `16-way tree-PLRU` 只需 `15b`，可放进 `32b`

在升级后的 byte-granular table 上，这样切分的原因不再是“绕开 32-bit 对齐限制”，而是：

- data/meta/replacement 生命周期本来就不同
- lookup latency 可以分别配置
- 写回时更容易只改 meta 或 replacement，不必带上整块 data payload

### 4.4 元数据编码

建议 `MetaTable` 的每 way 采用单 `uint32_t`：

- `bit[0]`: `valid`
- `bit[1]`: `dirty`
- `bit[2]`: `prefetch/reserved`
- `bit[3]`: `bypass_fill/reserved`
- `bit[31:4]`: `tag`

对当前 `32-bit PA + 64B line + 16-way + 16MB` 的几何，tag 位数足够放入 `bit[31:4]`。

## 5. 包含策略：推荐 NINE

推荐 LLC 采用 `NINE`，理由如下：

- 当前系统没有上层 cache invalidation / snoop 通路。
- `icache` 现在是私有上层，未来 `dcache` 即使引入私有 L2，也没有现成的 back-invalidate 接口。
- `inclusive` 需要 LLC 驱逐时向 L1/L2 发失效，这会迫使前端/LSU/cache 顶层接口一起改。
- `exclusive` 需要上下层 owner 迁移和 victim 交换，复杂度明显高于本项目当前 interconnect 结构。

因此首版应定义为：

- 上层命中与 LLC 是否保有该行无强约束。
- LLC 驱逐不通知上层。
- 上层 miss 时若 LLC hit，则向上返回 data，但不要求上层目录与 LLC 保持包含关系。

这对单核、无 DMA、无外部一致性代理的当前模拟器是可接受的。

## 6. unified cache 实现方案

推荐 LLC 为统一的 `unified cache`，统一服务：

- `MASTER_ICACHE`
- `MASTER_DCACHE_R`
- `MASTER_UNCORE_LSU_R`
- `MASTER_EXTRA_R`
- `MASTER_DCACHE_W`
- `MASTER_UNCORE_LSU_W`

原因：

- interconnect 本身就是共享点，放 unified LLC 最自然。
- 相比做 I/D 分离 LLC，不需要两套 data/meta/replacement table。
- 对未来 D-side L2/L3 场景，shared LLC 更接近真实 SoC。

### 6.1 当前可落地范围

当前代码里真正接到 interconnect 的只有 `icache` 读口，所以：

- 首阶段实现 LLC 时，功能上先覆盖 `icache read miss -> LLC -> DDR`
- `dcache` / `uncore` / `extra` 接口先保留，但不必在第一步全部联通

## 7. uncache 请求不走 LLC 的方案

这个需求必须分两类处理：

### 7.1 MMIO 地址天然 bypass

所有 `is_mmio_addr(addr)` 命中的请求都必须绕过 LLC。

现有辅助函数已存在：

- `simulator/axi-interconnect-kit/include/axi_mmio_map.h`

### 7.2 cache-inhibited DDR 地址 / 特殊 uncached 请求

仅靠地址范围不够，因为未来可能有 DDR 内可访问但不希望进入 LLC 的 uncached 区域。
推荐上游接口只保留一个显式属性位：

- `req.bypass`

读写请求都应有。

建议规则：

- `is_mmio_addr(addr)` -> 强制 bypass
- `req.bypass` -> bypass
- 否则进入 LLC

### 7.3 接口兼容建议

当前 `ReadMasterReq_t` / `WriteMasterReq_t` 没有这个字段。推荐做法：

- 给结构体新增字段，默认值初始化为 `false`
- 现有 `icache` 调用点不需要修改行为
- 后续 `dcache` / `uncore lsu` 接入时再真正驱动这个字段

如果暂时不想立刻改所有 master，`uncore lsu` 口也可以在接入层先默认置 `bypass=true`，但长期仍推荐把这个语义做成显式字段。

## 8. PIPT 方案

LLC 应明确采用 `PIPT`：

- index/tag 全部来自 physical address
- LLC 不接 virtual address
- 不承担 synonym / alias 处理

这在当前工程是合理的，因为 interconnect 下游本来就面对 DDR/MMIO 物理空间；`icache` 通过 miss path 发给 interconnect 的地址也应被视为物理地址。

结论：

- LLC 不需要 VIPT/VIVT 的别名处理逻辑。
- `NINE + PIPT` 是当前工程改动最小且最稳妥的组合。

## 9. 读写策略

### 9.1 读 miss

读 miss 流程建议：

1. 仲裁选中一个上游请求
2. 读 `MetaTable + DataTable + ReplTable`
3. `hit`:
   - 直接返回上游
   - 更新 `PLRU`
4. `miss`:
   - 若有 invalid way，直接占用
   - 否则按 `PLRU` 选 victim
   - victim dirty 时先 writeback
   - 再向下游发 refill
   - refill 完成后写入 `DataTable/MetaTable/ReplTable`
   - 向上游返回 data

### 9.2 写策略

推荐 LLC 默认采用：

- `write-back`
- `write-allocate`
- `write-no-allocate` 仅用于 bypass/uncached 请求

原因：

- 大容量 LLC 的价值主要体现在吸收写工作集和减少 DDR 带宽。
- `write-through` 会让未来 dcache/l2 接入后的收益明显下降。

cacheable store 行为：

- write hit:
  - 更新命中行 bytes
  - 置 dirty
  - 更新 replacement 状态
- write miss:
  - 首版建议 `write-allocate + read-for-ownership`
  - refill 回来后 merge `wstrb`
  - 返回写响应

### 9.3 部分写和当前接口限制

这是实现可行性的核心约束：

- 当前 `WriteMasterReq_t::wdata` 只有 `32B`
- 而 LLC line 建议为 `64B`

这意味着：

- 未来若 `dcache L2` 想把整条 `64B` victim line 一次性写回 LLC，现有写口不够用
- 只能拆成两个 `32B` 写，或者扩展上游写口

因此推荐现在就把写口设计成与读口对称的“可变宽到 `256B`”：

- 新增 `MAX_WRITE_TRANSACTION_BYTES`
- `WideWriteData_t` 和 `WideWriteStrobe_t`
- `WriteMasterReq_t.total_size` 提升到和 read 一样的 byte-count 语义

如果只做当前 `icache` 场景，可以先不改；但如果目标包含“未来 dcache L2 之下的 LLC/L3”，这个接口扩展基本是必需项。

## 10. 替换策略

推荐首版使用 `tree-PLRU`：

- 行为稳定，可重复
- 对 `8-way / 16-way` 都容易编码
- 每个 set 的状态量小，适合 `ReplTable`

替换顺序建议：

1. 先找 invalid way
2. 无 invalid 时用 `tree-PLRU`
3. 每次 hit / fill 后都更新 `PLRU`

随机替换可以作为 debug fallback，但不建议作为默认。

## 11. 时序模型与状态机

### 11.1 与现有 comb/seq 风格对齐

LLC 应遵循模拟器既有规则：

- `comb_*()` 只读当前寄存器，产生 `_next`
- `seq()` 只提交 `_next`

不建议写成“大量直接改当前状态”的行为式代码。

### 11.2 推荐 MSHR 组织

首版就应引入可配置 `MSHR` 数组，而不是单 miss engine 写死后再推倒重来：

- `AXI_LLC_MSHR_NUM` 可配置，默认建议 `4`
- 每个 `MSHR` 绑定一个 `master_id + orig_id + line_addr`
- 同一 cache line 的后续 miss 可 merge 到已有 `MSHR`
- 响应返回时按 `master_id + orig_id` 精确路由

建议的 `MissEntryState`：

- `FREE`
- `LOOKUP_ALLOCATED`
- `MISS_SELECTED`
- `WRITEBACK_REQ`
- `WRITEBACK_WAIT`
- `REFILL_REQ`
- `REFILL_WAIT`
- `REFILL_COMMIT`
- `RESP_READY`

### 11.3 全局调度器与结构冲突

即使支持多 `MSHR`，首版仍可以只有少量共享资源：

- `1` 个 lookup issue scheduler
- `1` 个 refill/writeback issue scheduler
- `1` 个 table commit scheduler

这样做的结果是：

- 可以在前一个 miss 尚未返回时继续接受下一个请求
- 但真正的下游发射和 table commit 仍可能串行

这比“全阻塞单 miss”前进一步，且状态机结构天然可扩展。

### 11.4 对当前 interconnect 阻塞行为的解决方案

目标语义应改成：

- 某个请求一旦拿到 `MSHR`，就算“已接收”
- 只要 `MSHR` 还有空位，同一个 master 可以继续送后续请求
- `req.ready` 是否拉高由 `MSHR` 余量、同 line merge 条件、表端口冲突共同决定

因此，新的 interconnect/LLC 设计里应去掉“同 master 只有一个 pending 才能继续接收”的硬约束，替换为：

- `per-master outstanding credit`
- `global MSHR budget`
- `AXI ID budget`

三者共同裁决。

## 12. 与 AXI4 / AXI3 的集成方式

### 12.1 AXI4

AXI4 下游 beat 为 `32-bit`，`64B` line refill 需要 `16` beats。

优点：

- 现有 `AXI_Interconnect` 已有多 beat 聚合到上游宽响应的逻辑，可复用其 pending/data 收集思路。

代价：

- LLC miss engine 需要更长的 refill/writeback 周期。

### 12.2 AXI3

AXI3 下游 beat 为 `256-bit`，`64B` line 只需 `2` beats。

优点：

- 更接近 cache line 传输粒度

代价：

- 现有 `AXI_Interconnect_AXI3` 有自己的 packed ID 和单 active read/write 约束，需要单独做胶水

### 12.3 推荐实现顺序

1. 先实现协议无关 `AXI_LLC`
2. 先接通 AXI4
3. 再把 AXI3 胶水补齐

原因不是 AXI3 做不了，而是当前主模拟器默认配置是 `CONFIG_AXI_PROTOCOL = 4`，且先把 AXI4 上的 non-blocking + MSHR 结构打稳更现实。

## 13. 配置项建议

建议在 `simulator/include/config.h` 提供默认值，在运行时再由 `AXI_LLCConfig` 注入实际 geometry：

```cpp
#ifndef CONFIG_AXI_LLC_ENABLE
#define CONFIG_AXI_LLC_ENABLE 0
#endif

constexpr uint32_t AXI_LLC_DEFAULT_SIZE_BYTES = 8u * 1024u * 1024u;
constexpr uint32_t AXI_LLC_DEFAULT_LINE_SIZE = 64u;
constexpr uint32_t AXI_LLC_DEFAULT_WAYS = 16u;
constexpr uint32_t AXI_LLC_DEFAULT_MSHR_NUM = 4u;
constexpr uint32_t AXI_LLC_DEFAULT_LOOKUP_LATENCY = 4u;
constexpr uint32_t AXI_LLC_REPL_POLICY = 1; // 0=random, 1=tree-plru
constexpr bool AXI_LLC_WRITE_ALLOCATE = true;
constexpr bool AXI_LLC_BYPASS_MMIO = true;
constexpr bool AXI_LLC_USE_NINE = true;
```

运行时配置对象建议为：

```cpp
struct AXI_LLCConfig {
  uint32_t size_bytes = AXI_LLC_DEFAULT_SIZE_BYTES;
  uint32_t line_bytes = AXI_LLC_DEFAULT_LINE_SIZE;
  uint32_t ways = AXI_LLC_DEFAULT_WAYS;
  uint32_t mshr_num = AXI_LLC_DEFAULT_MSHR_NUM;
  uint32_t lookup_latency = AXI_LLC_DEFAULT_LOOKUP_LATENCY;
  uint32_t data_sram_latency = 4u;
  uint32_t meta_sram_latency = 2u;
  uint32_t repl_sram_latency = 1u;
};
```

如需复用 `GenericTable` 风格，还建议：

```cpp
constexpr uint32_t AXI_LLC_DATA_SRAM_LATENCY = 4u;
constexpr uint32_t AXI_LLC_META_SRAM_LATENCY = 2u;
constexpr uint32_t AXI_LLC_REPL_SRAM_LATENCY = 1u;
```

## 14. 代码落点建议

### 14.1 新类

`AXI_LLC` 建议包含：

- `AXI_LLCConfig`
- 配置与派生常量
- `MetaTable/DataTable/ReplTable`
- `MSHR` 数组与 merge/allocate 逻辑
- 上游请求锁存
- miss/writeback/refill 调度器
- 按 master 路由的响应寄存器

### 14.2 interconnect 改造点

`AXI_Interconnect` / `AXI_Interconnect_AXI3` 需要：

1. 持有 `AXI_LLC llc_`
2. 在 `init()` 时初始化 LLC
3. `comb_outputs()`：
   - 优先输出 LLC 命中/完成响应
   - 再输出 bypass 或 miss engine 对下游的请求 ready
4. `comb_inputs()`：
   - 先把上游请求送进 LLC 决策
   - LLC 决定是 hit / bypass / refill / writeback
   - 只有 bypass 或 LLC miss/writeback 时才真正驱动下游 AXI
5. `seq()`：
   - 先更新 LLC 状态
   - 再更新 AXI 通道状态

### 14.3 MemSubsystem 改造点

首阶段可以不改 `MemSubsystem` 总体结构，只需要：

- 继续把 `MASTER_ICACHE` 接进 interconnect
- 其它 master 保持 idle

第二阶段若要让 `dcache` 也走 LLC，需要先把 `SimpleCache` 替换为 AXI-backed dcache，或在 `MemSubsystem` 里新增一层 AXI 适配。

## 15. 可行性评估

### 15.1 高可行部分

以下部分可行性高，且可以较小步提交：

- 先升级 `GenericTable`，支持 byte-granular payload 和运行时 geometry
- 在 `axi-interconnect` 中新增独立 `AXI_LLC` 类文件
- 做 `icache` 只读场景的 unified LLC
- 使用 `PIPT + NINE + 64B line`
- 用“更新后的 GenericTable”做外置 `data/meta/repl` 表
- 支持 `8MB / 16MB` 默认 preset
- 支持任意合法 `ways/sets/size` 组合的动态配置
- MMIO bypass

### 15.2 中等风险部分

- AXI3 胶水同步支持
- `SramTablePolicy` 下 LLC 多表 lookup 时序对齐
- dirty victim writeback 和 refill 的 pipeline 排序

这些都能做，但需要完整单元测试覆盖。

### 15.3 真正的结构性阻塞点

最大的结构性问题不是 LLC 本身，而是上游写接口宽度：

- 当前 `WriteMasterReq_t` 最大仅 `32B`
- 未来如果要支持 `dcache L2 -> LLC` 的整行写回，这个接口明显偏窄

因此：

- 如果目标只是“先让 icache 走一个共享 LLC”，完全可行
- 如果目标包含“未来把 dcache 的 L2/L3 也规整接到这个 LLC”，推荐尽早扩展写接口宽度

## 16. 分阶段实施建议

### Phase A: 先打底座

- 升级 `GenericTable`
- 建 `AXI_LLC_IO_t` generalized-io 结构
- 加 `AXI_LLCConfig` 和可配置 `MSHR`

### Phase B: 先打通 icache LLC

- 新增 `AXI_LLC` 类文件
- AXI4 路径先接 LLC
- 只支持 cacheable read、MMIO bypass
- `dcache/uncore/extra/write` 保持现状或 bypass

预期收益：

- `icache miss` 不再总是直达 DDR
- 可以较早验证大容量 LLC + SRAM lookup 机制

### Phase C: 完整化 miss/writeback 引擎

- 补 dirty victim writeback
- 补 AXI3 接入
- 补统计计数器和 debug trace

### Phase D: 面向未来 dcache L2/L3

- 扩上游 write port 宽度
- 增加 `bypass` 属性位
- 让 dcache 读写请求也经过 interconnect/LLC

## 17. 结论

结论如下：

- 在 `axi-interconnect` 层加入一个共享 LLC 是可行的。
- 推荐方案是：`PIPT + NINE + unified cache + 64B line + 动态 geometry + 更新后的 GenericTable + tree-PLRU + write-back/write-allocate`。
- `8MB / 16MB` 只是默认 preset，不应限制成仅这两档。
- 代码结构上应先做独立 `AXI_LLC` 类，再由 `AXI_Interconnect` / `AXI_Interconnect_AXI3` 实例化。
- 当前最重要的实现边界是两点：一是先把 `GenericTable` 做成可支撑动态 geometry 的通用底座；二是若要把未来 `dcache L2` 的整行回写也纳入 LLC，需要扩展现有写主设备接口宽度。
