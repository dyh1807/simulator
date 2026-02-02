# icache_module_v2（非阻塞 I$）设计方案（rev2）

本方案用于新增 `icache_module_v2.cpp/.h`，在保持现有 `icache_module.cpp/.h` 的基本参数与“查表两种实现方式（register / sram 延迟模型）”不变的前提下，引入：
- 非阻塞 miss（MSHR 可配置）与预取机制；
- 可配置替换算法（新增 PLRU，且不把 PLRU 写死为 8-way）；
- 访存接口同时支持“非 AXI（TrueICacheTop）”与“AXI（SimDDRICacheTop + interconnect）”；
- 提供 `mem_req_id/mem_resp_id`（按你的反馈）。

> Skills 对齐说明：严格遵循 `simulator-arch` 的 comb/seq 分离与 `two-phase-debug` 的 ready/valid/backpressure 规则，并保留 `icache_v2` 中关于 refetch/DRAIN 的避坑点；但 **不遵循** `icache_v2` 文档里“必须阻塞式流水线”的约束，因为本任务目标就是非阻塞 I$。

---

## 0. 关键系统约束（必须兼容）

### 0.1 前端 FIFO/PTAB 约束：默认必须 **in-order 对外完成**
- `PTAB` 是 FIFO（`front-end/fifo/PTAB.cpp`），BPU 只在 `icache_read_ready=1` 时推进 `pc_reg` 并写入 PTAB；
- instFIFO 只在 `icache_read_complete=1` 时写入 fetch_group。

因此，v2 默认保证：
- **IFU 可见的响应顺序 = IFU 请求握手成功的顺序**（否则要给 PTAB/instFIFO 加 tag 并重排，改动面过大，非本次范围）。

### 0.2 MMU IFU 端口：接受 “TLB miss 仍可能阻塞”
现有 IFU→MMU 没有 id，响应是固定 1 拍延迟的 ready/valid 组合；TLB miss 需要重放直到命中。

本方案接受：
- **TLB miss 仍可作为硬阻塞点**（即某个 ROB 头部请求翻译未完成时，保持 in-order 不对外完成）。

但按你的反馈需要注意：
- **refetch 到来时，不应被旧的 TLB miss “长期阻塞”**：v2 会在 refetch 时清空/取消旧请求的等待状态，并开始受理新 PC（只要资源允许且 MMU ready 允许）。

### 0.3 AXI interconnect 的现实并行度限制（写清预期）
虽然 `axi_interconnect::ReadMasterPort_t` 带 `req.id/resp.id`，但当前实现：
- AXI3 interconnect（`AXI_Interconnect_AXI3.cpp`）read 通道整体近似 **单 outstanding**（`r_active || r_resp_valid` 时不再接收新 read）；
- legacy AXI4 interconnect 也基本限制每个 master 1 outstanding。

因此：
- 在 **AXI 模式**下，MSHR>1 的主要收益来自 **miss 合并/队列化/预取占位**；要真正多 outstanding，需要后续扩展 interconnect（可另开任务）。

---

## 1. 参数化原则（尽量避免硬编码）

### 1.1 配置方式
建议同时支持：
- **宏默认值**（编译期）；
- **初始化参数覆盖**（运行期/构造函数）。

建议新增：
```cpp
struct ICacheV2Config {
  uint32_t line_bytes;          // 默认 ICACHE_LINE_SIZE（但 AXI 侧目前固定 32B/256-bit，改它需同步改 interconnect）
  uint32_t page_offset_bits;    // 默认 12（4KB 页）
  uint32_t ways;                // 可配置（不固定 8）
  uint32_t mshr_num;            // 可配置
  uint32_t rob_depth;           // 可配置（in-order commit）
  bool prefetch_enable;
  uint32_t prefetch_distance;   // next-line=1
  uint32_t prefetch_reserve;    // 给 demand 预留的 MSHR 个数（避免预取挤占）
  enum ReplPolicy { RR, RANDOM, PLRU } repl;
};
```

### 1.2 推导参数（不写死 set_num/word_num/offset_bits）
- `offset_bits = log2(line_bytes)`
- `word_num = line_bytes / 4`
- `index_bits = page_offset_bits - offset_bits`
- `set_num = 1 << index_bits`

> 注：该推导保持了现有“index 取 VA 页内位”的策略，避免虚实别名与页内索引不一致的问题。

---

## 2. v2 外部接口（提供 id）

### 2.1 模块 IO（建议新定义 `icache_module_v2.h`，不改 v1 IO）
- IFU 输入：`pc / ifu_req_valid / refetch`
- IFU 输出：`ifu_req_ready / ifu_resp_valid / rd_data[word_num] / ifu_page_fault`
- MMU 输入：`ppn / ppn_valid / page_fault`
- MMU 输出：`ppn_ready`（沿用现有：仅在需要翻译时拉高）

**访存接口（新增 id）：**
- `mem_req_valid/ready/addr/id`
- `mem_resp_valid/ready/data[word_num]/id`

> AXI 模式下：`id` 直接映射到 `mem_subsystem().icache_port().req.id/resp.id`（4-bit）。
>
> 非 AXI 模式下：TrueICacheTop 的“多 outstanding 内存队列”也携带 `id`，可乱序返回；v2 用 `id->MSHR` 映射完成回填匹配。

### 2.2 ID 位宽与 MSHR_NUM 的关系
- interconnect upstream `req.id/resp.id` 是 `wire4_t`（4-bit，0..15）。
- v2 内部 `mshr_num` 可以 >16，但 **同一时刻实际发出去的 memory transaction 数量不能超过可编码的 id 数**。
- 在当前 interconnect “单 outstanding”约束下，实际 in-flight=1，所以即便 `mshr_num>16` 也可工作：只需保证发出去的那一笔有唯一 id，并记录 `id->mshr_idx` 映射。

建议实现策略：
- 维护 `txid` 分配器（0..15 循环），仅对 **已发出但未回**的事务占用；
- MSHR entry 里记录 `txid`（或独立 mapping table `txid -> mshr_idx`）。

---

## 3. 微架构总览：ROB + 查表流水 + MSHR + Prefetch

### 3.1 Fetch-ROB（in-order commit）
目的：允许 hit-under-miss，但对外完成保持 in-order。

每个 ROB slot 建议字段：
- `valid, pc, epoch`
- `state`: EMPTY / LOOKUP / WAIT_TLB / WAIT_MSHR / READY / KILLED
- `page_fault`
- `line_data[word_num]`（READY 时有效）
- `line_addr`（物理对齐 32B）
- `mshr_idx`（WAIT_MSHR 时）

对外 `ifu_resp_valid` 仅来自 **ROB head**：
- head READY：输出 data/page_fault 并 pop
- head WAIT：`ifu_resp_valid=0`

### 3.2 查表两种模式（保留 v1 思路）

#### (1) Register 查表（`ICACHE_USE_SRAM_MODEL=0`）
- Stage1：组合读出 `index` 对应 set 的 `tags/data/valid`
- Stage2：下一拍用 `ppn` 比对，产出 hit/miss/page_fault，并写回 ROB slot

#### (2) SRAM 延迟模型（`ICACHE_USE_SRAM_MODEL=1`）
- 复用 v1 的 `sram_pending/delay/index/seed`，但注意 v2 可能需要 **支持并行多个 lookup**：
  - 最小实现：仍限制“同一周期最多启动 1 个 SRAM lookup”（简单可靠）；
  - 后续可扩展：多 bank 或多端口 SRAM（超出本次）。
- 关键要求：
  - `sram_ready` 类信号必须保持直到被 stage2 消费（避免 backpressure 丢数据）
  - 禁止 comb 里改 `_r`（严格 comb/seq 分离）

### 3.3 MSHR（miss 合并 + 回填唤醒）
MSHR entry 建议字段：
- `valid, epoch`
- `line_addr`（物理对齐）
- `state`: ALLOC / SENT / WAIT_RESP
- `is_prefetch`
- `waiters`：ROB slot 的 bitset 或小队列（用于 merge）
- `txid`（若已发出 mem_req）

规则：
- miss -> 查 MSHR：
  - 命中已有 entry：merge waiter
  - 否则分配 FREE entry（若无 FREE：ROB head 可能反压，避免无限堆积）

### 3.4 Prefetch（默认 next-line，参数化）
默认实现：
- demand miss 分配成功后，尝试分配 prefetch entry：`line_addr + prefetch_distance * line_bytes`
- 抑制：
  - 已在 cache / 已在 MSHR
  - `mshr_free <= prefetch_reserve`（给 demand 预留）

Prefetch 的回填：
- 回填到 cache，但默认不直接唤醒 demand（除非有人 waiter 绑定）
- 替换算法更新可配置：prefetch fill 是否更新为 MRU（可选项，降低污染）

---

## 4. 替换策略（新增 PLRU，且 ways 可配）

### 4.1 策略选择
支持 `RR/RANDOM/PLRU`，通过：
- macro 默认（如 `ICACHE_V2_REPL_POLICY`）
- 或构造参数 `config.repl`

共同规则：**invalid way 优先**（不污染 PLRU/RR 状态也可，或把它视作一次访问更新，二者可配）

### 4.2 通用 Tree-PLRU（要求 ways 不写死）
Tree-PLRU 的天然约束是：`ways` 最好为 **2 的幂**（2/4/8/16...）。

建议约束：
- 若 `repl=PLRU`：要求 `ways` 是 2 的幂，否则：
  - (A) 运行时报错/assert（更安全）
  - 或 (B) 自动降级为 RR（更“可跑”）

实现要点（通用 N-way）：
- 每个 set 保存 `ways-1` 个 bit，表示一棵满二叉树的内部节点；
- victim 选择：从 root 出发按 bit 指示一路走到 leaf，得到 way；
- update（hit/insert）：沿访问路径把经过节点 bit 改为“另一侧为 LRU”（即访问左子树则 bit=1，访问右子树则 bit=0）。

---

## 5. refetch 处理：保证“不被旧 TLB miss 长期阻塞”

### 5.1 epoch + kill 机制
维护 `cur_epoch`：
- `refetch=1` 的周期：`cur_epoch++`，并：
  - 清空 ROB（slot->KILLED/EMPTY）
  - 清空 prefetch 队列
  - 清除 “等待旧 TLB 翻译/重放” 的阻塞标志（重要）

对 MSHR：
- 可选择两种策略（建议默认 B）：
  - (A) refetch 直接清空 MSHR：实现简单，但会丢带宽与潜在命中
  - (B) 保留 MSHR，但用 epoch kill：响应回来若 epoch 不匹配则 drop 并释放 entry（避免污染/错配）

### 5.2 与 MMU 的配合
因为 IFU→MMU 没有 id：
- refetch 周期对 mmu_resp 直接忽略（现有 top 已这么做：`ppn_valid = mmu_resp.valid && !refetch && !miss`）
- refetch 之后的新请求将重新走翻译；v2 不会因为旧 slot 的 WAIT_TLB 而继续拉低 `ifu_req_ready`

> 仍可能阻塞的唯一情况：MMU 自身 `ready=0`（例如 PTW busy 且实现确实会拉低 ready）。v2 只能尊重这个 backpressure，但不会“额外制造阻塞”。

---

## 6. 访存接口两类（都带 id）

### 6.1 非 AXI（TrueICacheTop 风格，建议升级为可多 outstanding）
建议把 TrueICacheTop 的内存模型升级为：
- `mem_req_ready` 常为 1（或可配置带宽/限流）
- 接受 `mem_req_valid && mem_req_ready`：push 一个 pending txn（包含 `addr/id/ready_cycle`）
- 当 `sim_time` 到 `ready_cycle`：对该 txn 拉高 `mem_resp_valid` 并返回 data + id（可乱序）
- v2 用 `id` 匹配到 MSHR，完成回填与唤醒

### 6.2 AXI（SimDDRICacheTop + MemorySubsystem）
- 继续通过 `mem_subsystem().icache_port()` 连接：
  - `port.req.valid/ready/addr/total_size/id`
  - `port.resp.valid/ready/data/id`
- 当前 interconnect 近似单 outstanding：v2 的 scheduler 每周期最多发 1 个，且 in-flight 不超过 interconnect 能力。

---

## 7. 验证计划（按 skills 执行）

### 7.1 两阶段/握手自检（`two-phase-debug`）
- mem_req latch：`valid && !ready` 时 addr/id/len 必须保持
- resp 清除时序：seq 入口 snapshot `resp_valid_curr`，避免“同周期产生又同周期清掉”
- refetch：不在 top 层硬清 mem_busy/latency_cnt，靠 epoch/DRAIN 自己 drain

### 7.2 程序回归（`verify-simulator`）
- `./a.out baremetal/new_dhrystone/dhrystone.bin`
- `./a.out baremetal/new_coremark/coremark.bin`
- `timeout 60s ./a.out baremetal/linux.bin`

建议加 watchdog（例如 500 周期 head 未完成直接打印并退出）用于快速定位死锁。

