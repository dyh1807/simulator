# AXI Interconnect Kit Interface Specification

This document lists:

1. Interconnect upstream interfaces
2. AXI3 channel signals
3. AXI4 channel signals

## 1) Interconnect Upstream Interfaces

Defined in `axi_interconnect/include/AXI_Interconnect_IO.h`.

### 1.1 Read Master Request (`ReadMasterReq_t`)

| Signal | Width | Direction (vs interconnect) | Meaning |
|---|---:|---|---|
| `valid` | 1 | input | Request is valid |
| `ready` | 1 | output | Interconnect accepts request |
| `addr` | 32 | input | Byte address |
| `total_size` | 5 | input | Transfer bytes minus 1 (`0=1B`, `31=32B`) |
| `id` | 4 | input | Upstream transaction ID |

### 1.2 Read Master Response (`ReadMasterResp_t`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `valid` | 1 | output | Response valid |
| `ready` | 1 | input | Master accepts response |
| `data` | 256 (`8x32`) | output | Wide read response payload |
| `id` | 4 | output | Upstream ID echoed back |

### 1.3 Write Master Request (`WriteMasterReq_t`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `valid` | 1 | input | Request valid |
| `ready` | 1 | output | Interconnect accepts request |
| `addr` | 32 | input | Byte address |
| `wdata` | 256 (`8x32`) | input | Wide write payload |
| `wstrb` | 32 | input | Byte strobes |
| `total_size` | 5 | input | Transfer bytes minus 1 |
| `id` | 4 | input | Upstream transaction ID |

### 1.4 Write Master Response (`WriteMasterResp_t`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `valid` | 1 | output | Response valid |
| `ready` | 1 | input | Master accepts response |
| `id` | 4 | output | Upstream ID echoed back |
| `resp` | 2 | output | AXI response code |

## 2) AXI3 Channel Signals

Defined in `sim_ddr/include/SimDDR_AXI3_IO.h`.

AXI3 in this project uses:

- `ID`: 22 bits (stored in `wire32_t`)
- Data: `256-bit` (`8 x 32-bit words`)

### 2.1 Write Address Channel (`AW`)

| Signal | Width | Direction (master->slave) | Meaning |
|---|---:|---|---|
| `awvalid` | 1 | M->S | Address valid |
| `awready` | 1 | S->M | Address ready |
| `awid` | 22 (in 32) | M->S | Write transaction ID |
| `awaddr` | 32 | M->S | Byte address |
| `awlen` | 4 | M->S | Burst length minus 1 |
| `awsize` | 3 | M->S | `log2(bytes_per_beat)` |
| `awburst` | 2 | M->S | Burst type |

### 2.2 Write Data Channel (`W`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `wvalid` | 1 | M->S | Data valid |
| `wready` | 1 | S->M | Data ready |
| `wid` | 22 (in 32) | M->S | AXI3 write ID |
| `wdata` | 256 | M->S | Write payload |
| `wstrb` | 32 | M->S | Byte enable bits |
| `wlast` | 1 | M->S | Last beat |

### 2.3 Write Response Channel (`B`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `bvalid` | 1 | S->M | Response valid |
| `bready` | 1 | M->S | Response ready |
| `bid` | 22 (in 32) | S->M | Response ID |
| `bresp` | 2 | S->M | Response code |

### 2.4 Read Address Channel (`AR`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `arvalid` | 1 | M->S | Address valid |
| `arready` | 1 | S->M | Address ready |
| `arid` | 22 (in 32) | M->S | Read transaction ID |
| `araddr` | 32 | M->S | Byte address |
| `arlen` | 4 | M->S | Burst length minus 1 |
| `arsize` | 3 | M->S | `log2(bytes_per_beat)` |
| `arburst` | 2 | M->S | Burst type |

### 2.5 Read Data Channel (`R`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `rvalid` | 1 | S->M | Data valid |
| `rready` | 1 | M->S | Data ready |
| `rid` | 22 (in 32) | S->M | Response ID |
| `rdata` | 256 | S->M | Read payload |
| `rresp` | 2 | S->M | Response code |
| `rlast` | 1 | S->M | Last beat |

## 3) AXI4 Channel Signals

Defined in `sim_ddr/include/SimDDR_IO.h`.

AXI4 in this project uses:

- `ID`: 4 bits
- Data: `32-bit`

### 3.1 Write Address Channel (`AW`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `awvalid` | 1 | M->S | Address valid |
| `awready` | 1 | S->M | Address ready |
| `awid` | 4 | M->S | Write transaction ID |
| `awaddr` | 32 | M->S | Byte address |
| `awlen` | 8 | M->S | Burst length minus 1 |
| `awsize` | 3 | M->S | `log2(bytes_per_beat)` |
| `awburst` | 2 | M->S | Burst type |

### 3.2 Write Data Channel (`W`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `wvalid` | 1 | M->S | Data valid |
| `wready` | 1 | S->M | Data ready |
| `wdata` | 32 | M->S | Write payload |
| `wstrb` | 4 | M->S | Byte enable bits |
| `wlast` | 1 | M->S | Last beat |

### 3.3 Write Response Channel (`B`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `bvalid` | 1 | S->M | Response valid |
| `bready` | 1 | M->S | Response ready |
| `bid` | 4 | S->M | Response ID |
| `bresp` | 2 | S->M | Response code |

### 3.4 Read Address Channel (`AR`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `arvalid` | 1 | M->S | Address valid |
| `arready` | 1 | S->M | Address ready |
| `arid` | 4 | M->S | Read transaction ID |
| `araddr` | 32 | M->S | Byte address |
| `arlen` | 8 | M->S | Burst length minus 1 |
| `arsize` | 3 | M->S | `log2(bytes_per_beat)` |
| `arburst` | 2 | M->S | Burst type |

### 3.5 Read Data Channel (`R`)

| Signal | Width | Direction | Meaning |
|---|---:|---|---|
| `rvalid` | 1 | S->M | Data valid |
| `rready` | 1 | M->S | Data ready |
| `rid` | 4 | S->M | Response ID |
| `rdata` | 32 | S->M | Read payload |
| `rresp` | 2 | S->M | Response code |
| `rlast` | 1 | S->M | Last beat |
