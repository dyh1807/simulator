# AXI Interconnect Kit

Standalone AXI subsystem extracted from the simulator:
- AXI4 interconnect + SimDDR
- AXI3 interconnect + AXI3 router + SimDDR + MMIO/UART
- CPU-side simplified master ports (`4` read masters + `2` write masters)

## Why `interconnect` naming

`bridge` is usually point-to-point protocol conversion.
This module does multi-master arbitration, routing, and response demux, so `interconnect` is the more accurate name.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Or:

```bash
make -j
```

## Tests

```bash
cd build
ctest --output-on-failure
```

## Demos

```bash
./build/axi4_smoke_demo
./build/axi3_smoke_demo
```

## Integration

The parent simulator integrates this project by compiling sources under:
- `projects/axi-interconnect-kit/axi_interconnect`
- `projects/axi-interconnect-kit/sim_ddr`
- `projects/axi-interconnect-kit/mmio`

and adding include dirs under:
- `projects/axi-interconnect-kit/include`
- `projects/axi-interconnect-kit/*/include`
