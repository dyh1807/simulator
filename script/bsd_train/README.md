# `pi/po` 动态库导出工具说明

本文说明当前目录下两份脚本的用途，以及其它项目如何使用生成出来的 `pi/po -> po` 动态库。

## 1. 工具文件

当前方案只依赖两份脚本：

- [`script/bsd_train/gen_pi_po.py`](./gen_pi_po.py)
- [`script/bsd_train/gen_io_generator_so.py`](./gen_io_generator_so.py)

其中：

### `gen_pi_po.py`
负责从模块的 generalized-IO 结构体定义中，自动生成：
- `PI_WIDTH`
- `PO_WIDTH`
- `pack_pi()` / `unpack_pi()`
- `pack_po()` / `unpack_po()`
- `eval_comb()`

这是一个**通用结构展开脚本**，不直接绑定某个具体模块。

### `gen_io_generator_so.py`
负责根据模块绑定关系和配置：
- 调用 `gen_pi_po.py` 生成 `pi/po` 头
- 生成动态库包装代码
- 可选直接编译出 `.so`

它是当前面向训练/拟合流程的主入口脚本。

## 2. 当前默认配置文件

默认配置文件：

- [`script/bsd_train/configs/icache_v1_io_generator.json`](./configs/icache_v1_io_generator.json)

这份配置文件当前绑定的是：
- `icache_module_n::ICache`
- `PI = in + regs + lookup_in`
- `PO = out + reg_write + table_write`

默认还打开了：
- `PO slice = [0, 2)`
- 最小 totalization guard：
  - `regs.state`
  - `regs.mem_axi_state`

## 3. 最常用的用法

直接执行：

```bash
python3 script/bsd_train/gen_io_generator_so.py --config script/bsd_train/configs/icache_v1_io_generator.json
```

这条命令会在配置文件指定的 `out_dir` 中生成：

- `*_pi_po.h`
- `*_api.h`
- `*_api.cpp`
- `lib*.so`

以当前默认配置为例，输出目录是：

```bash
/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default
```

对应产物是：

- `icache_v1_pi_po.h`
- `icache_v1_api.h`
- `icache_v1_api.cpp`
- `libicache_v1.so`

## 4. 其它项目接入时，到底需要什么

### 结论
**其它项目在运行时需要：**
- `.so`

**其它项目在编译时需要：**
- `*_api.h`

**其它项目一般不需要：**
- `*_pi_po.h`
- `*_api.cpp`

### 原因

#### `lib*.so`
这是实际的动态库实现，里面包含：
- 模块实例
- `pi -> generalized io -> comb -> po` 的评估逻辑
- 多线程 `thread_local` 上下文
- totalization guard

#### `*_api.h`
这是**给其它项目 include 的唯一头文件**。
它只暴露：
- `PI_WIDTH`
- `PO_WIDTH`
- `PO_WIDTH_FULL`
- `io_generator_outer()`
- `io_generator_outer_range()`

这个头文件**不会 include 模拟器内部头文件**，因此不会把当前模拟器里的宏和类型定义带到其它项目里。

#### `*_pi_po.h`
这是库内部生成代码要用的头。
它会 include 当前模拟器里的模块头，例如：
- `front-end/icache/include/icache_module.h`

因此：
- 它主要给库自身编译使用
- **不建议其它项目直接 include**

#### `*_api.cpp`
这是库包装实现源码。
它通常只用于：
- 生成 `.so`
- 调试 wrapper 逻辑

其它项目正常接入时不需要它。

## 5. 给其它项目使用的最小依赖集合

假设生成目录是：

```bash
OUT=/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default
```

那么其它项目只需要：

### 编译时
- `-I$OUT`
- include: `#include "icache_v1_api.h"`

### 链接时
- `-L$OUT -licache_v1`
- 建议加：`-Wl,-rpath,$OUT`

也就是：
- **头文件：只用 `icache_v1_api.h`**
- **库文件：只用 `libicache_v1.so`**

## 6. 其它项目的编译命令示例

以当前目录下通过 `another_proj` 验证过的最小接入为例：

```bash
g++ another_proj/BSD.cpp -o /nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default/bsd_icache_probe \
  -O2 -std=c++11 -fopenmp \
  -I./another_proj/io_generator/simulator_include \
  -I./another_proj/io_generator \
  -DDYNAMIC_HEADER='"/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default/icache_v1_api.h"' \
  -DDYNAMIC_THREADS=128 \
  -L/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default \
  -licache_v1 \
  -Wl,-rpath,/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default
```

如果你的项目不是 `another_proj`，而是普通 C/C++ 工程，通常只需要类似：

```bash
g++ your_main.cpp -o your_prog \
  -O2 -std=c++17 \
  -I/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default \
  -L/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default \
  -licache_v1 \
  -Wl,-rpath,/nfs_global/S/daiyihao/tmp/icache_pi_po_dl_default
```

代码里 include：

```cpp
#include "icache_v1_api.h"
```

然后直接调用：

```cpp
bool pi[PI_WIDTH];
bool po[PO_WIDTH];
io_generator_outer(pi, po);
```

或者调用范围接口：

```cpp
bool pi[PI_WIDTH];
bool po_slice[2];
io_generator_outer_range(pi, po_slice, 0, 2);
```

## 7. `PO_WIDTH` 与 `PO_WIDTH_FULL`

当前导出接口会同时给出两个宽度：

- `PO_WIDTH_FULL`
  - 模块完整输出宽度
- `PO_WIDTH`
  - 当前导出给外部项目的输出宽度

如果配置里设置了：
- `export_po_begin`
- `export_po_width`

那么：
- `PO_WIDTH_FULL` 仍然表示完整输出宽度
- `PO_WIDTH` 表示当前导出的 slice 宽度

例如当前默认配置：
- `PO_WIDTH_FULL = 1819`
- `PO_WIDTH = 2`

这适合：
- 先小范围快速验证
- 再按 slice 分布式训练/拟合

## 8. totalization 的位置

当前 totalization **只在导出 wrapper 中生效**，不修改主模拟器的语义。

也就是说：
- 主模拟器的 `icache_module` 仍保持原本行为
- 导出给训练/拟合流程的 `.so` 会对极少量非法输入做保护

当前默认 guard 是：
- `regs.state` 只允许 `0..2`
- `regs.mem_axi_state` 只允许 `0..1`

如果超出范围：
- 当前导出 wrapper 直接返回默认 `PO`（全 0）

这样做的目标是：
- 让导出接口变成 totalized 的确定函数
- 但不污染主模拟器逻辑

## 9. 配置文件字段说明

常用字段：

- `header`
  - generalized-IO 结构定义所在头文件
- `include_path`
  - 生成的 `*_pi_po.h` 中使用的 include 路径
- `namespace`
  - 模块命名空间
- `out_namespace`
  - 生成的 `pi/po` helper 命名空间
- `wrapper_type`
  - generalized-IO 总结构，例如 `ICache_IO_t`
- `pi_field`
  - PI 侧字段绑定
- `po_field`
  - PO 侧字段绑定
- `module_type`
  - 模块类名
- `module_header`
  - 模块头文件
- `module_source`
  - 编译动态库需要的 `.cpp` 源文件列表
- `guard_range`
  - totalization 用的最小范围约束
- `api_namespace`
  - 动态库包装代码自己的命名空间
- `function_name`
  - 导出给训练器调用的函数名
- `export_po_begin`
  - 输出 slice 起点
- `export_po_width`
  - 输出 slice 宽度
- `out_dir`
  - 生成文件和 `.so` 输出目录
- `include_dir`
  - 编译 `.so` 时的 include 路径

## 10. 推荐工作流

推荐顺序：

1. 修改配置文件
2. 执行：

```bash
python3 script/bsd_train/gen_io_generator_so.py --config script/bsd_train/configs/icache_v1_io_generator.json
```

3. 在其它项目里 include `*_api.h`
4. 链接 `lib*.so`
5. 先用小 `PO slice` 验证
6. 再放大 `PO_WIDTH` 或切更多 slice

## 11. 当前已验证通过的场景

当前这套工具已经验证过：

1. 生成 `.so`
2. `another_proj` 最小接入编译
3. `another_proj` 128 线程短运行
4. `PO slice = 2` 生效

因此当前结论是：
- 这套“配置文件 + 两脚本 + 动态库”的方案已经可用
- 可以替代之前那个外置临时仓库方案
