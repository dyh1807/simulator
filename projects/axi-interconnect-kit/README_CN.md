# AXI Interconnect Kit（中文说明）

这是从原模拟器中抽离出的独立 AXI 子系统，可单独编译运行。

## 组件组成

- AXI4 路径：`interconnect + AXI4 router + SimDDR + MMIO bus + UART16550`
- AXI3 路径：`interconnect + AXI3 router + SimDDR + MMIO bus + UART16550`
- 上游 CPU 简化主设备端口：
  - 读主设备 `4` 个（`icache` / `dcache_r` / `mmu` / `extra_r`）
  - 写主设备 `2` 个（`dcache_w` / `extra_w`）

## 命名选择：为什么叫 `interconnect`

`bridge` 通常表示点到点协议转换。  
本项目承担的是多主设备仲裁、地址路由和响应分发，更准确的名字是 `interconnect`。

## 连接拓扑（READ 路径）

```
读主设备（4个）
  M0 icache, M1 dcache_r, M2 mmu, M3 extra_r
            |
            v
   +----------------------+
   | AXI_Interconnect     |  （上游简化接口 -> AXI总线）
   +----------------------+
            |
            v
   +----------------------+
   | AXI_Router_AXI4/AXI3 |  （按地址选择目标）
   +----------------------+
        |            |
        | DDR区间    | MMIO区间
        v            v
 +-------------+  +---------------------+
 | SimDDR      |  | MMIO_Bus + UART16550|
 | (slave #0)  |  | (slave #1)          |
 +-------------+  +---------------------+
```

## 连接拓扑（WRITE 路径）

```
写主设备（2个）
  M0 dcache_w, M1 extra_w
            |
            v
   +----------------------+
   | AXI_Interconnect     |  （AW/W/B 调度与响应路由）
   +----------------------+
            |
            v
   +----------------------+
   | AXI_Router_AXI4/AXI3 |  （AW/W/B 目标选择）
   +----------------------+
        |            |
        | DDR区间    | MMIO区间
        v            v
 +-------------+  +---------------------+
 | SimDDR      |  | MMIO_Bus + UART16550|
 | (slave #0)  |  | (slave #1)          |
 +-------------+  +---------------------+
```

## 接口信号文档

详细信号列表见：

- `docs/interfaces.md`（英文）
- `docs/interfaces_CN.md`（中文）

内容包括：

- Interconnect 上游接口（`read_ports[4]`、`write_ports[2]`）
- AXI3 五通道信号（`AW/W/B/AR/R`，256-bit 数据）
- AXI4 五通道信号（`AW/W/B/AR/R`，32-bit 数据）

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

或：

```bash
make -j
```

## 测试

```bash
cd build
ctest --output-on-failure
```

当前测试项：

- `sim_ddr_test`
- `axi_interconnect_test`
- `mmio_router_axi4_test`
- `sim_ddr_axi3_test`
- `axi_interconnect_axi3_test`
- `mmio_router_axi3_test`

## Demo

```bash
./build/axi4_smoke_demo
./build/axi3_smoke_demo
```

## 回接到父仓库

父仓库可将此项目作为外部依赖，包含以下目录：

- `include/`
- `axi_interconnect/include/`
- `sim_ddr/include/`
- `mmio/include/`

并按需要链接：

- `axi_kit_axi4`
- `axi_kit_axi3`
