# AXI Interconnect Kit

Standalone AXI subsystem extracted from the simulator and buildable on its own.

## Components

- AXI4 path: interconnect + AXI4 router + SimDDR + MMIO bus + UART16550 device
- AXI3 path: interconnect + AXI3 router + SimDDR + MMIO bus + UART16550 device
- Upstream simplified CPU-side master ports:
  - `4` read masters (`icache`, `dcache_r`, `mmu`, `extra_r`)
  - `2` write masters (`dcache_w`, `extra_w`)

## Naming Choice: `interconnect` vs `bridge`

`bridge` is usually point-to-point protocol conversion.  
This project does multi-master arbitration, address routing, and response demux, so
`interconnect` is the accurate name.

## Topology (READ path)

```
Read Masters (4 ports)
  M0 icache, M1 dcache_r, M2 mmu, M3 extra_r
            |
            v
   +----------------------+
   | AXI_Interconnect     |  (upstream simplified req/resp -> AXI4/AXI3)
   +----------------------+
            |
            v
   +----------------------+
   | AXI_Router_AXI4/AXI3 |  (address decode)
   +----------------------+
        |            |
        | DDR range  | MMIO range
        v            v
 +-------------+  +---------------------+
 | SimDDR      |  | MMIO_Bus + UART16550|
 | (slave #0)  |  | (slave #1)          |
 +-------------+  +---------------------+
```

## Topology (WRITE path)

```
Write Masters (2 ports)
  M0 dcache_w, M1 extra_w
            |
            v
   +----------------------+
   | AXI_Interconnect     |  (AW/W/B scheduling + response routing)
   +----------------------+
            |
            v
   +----------------------+
   | AXI_Router_AXI4/AXI3 |  (AW/W/B path selection)
   +----------------------+
        |            |
        | DDR range  | MMIO range
        v            v
 +-------------+  +---------------------+
 | SimDDR      |  | MMIO_Bus + UART16550|
 | (slave #0)  |  | (slave #1)          |
 +-------------+  +---------------------+
```

## Interface Signals

Detailed signal lists are in:

- `docs/interfaces.md`
  - Interconnect upstream ports (`read_ports[4]`, `write_ports[2]`)
  - AXI3 channels (`AW/W/B/AR/R`, 256-bit data bus flavor)
  - AXI4 channels (`AW/W/B/AR/R`, 32-bit data bus flavor)

Chinese version:

- `docs/interfaces_CN.md`

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

or:

```bash
make -j
```

## Tests

```bash
cd build
ctest --output-on-failure
```

Current test binaries:

- `sim_ddr_test`
- `axi_interconnect_test`
- `mmio_router_axi4_test`
- `sim_ddr_axi3_test`
- `axi_interconnect_axi3_test`
- `mmio_router_axi3_test`

## Demos

```bash
./build/axi4_smoke_demo
./build/axi3_smoke_demo
```

## Integration Back to Parent Simulator

Parent project can consume this repository as an external dependency and include:

- `include/`
- `axi_interconnect/include/`
- `sim_ddr/include/`
- `mmio/include/`

and link either:

- `axi_kit_axi4`
- `axi_kit_axi3`
