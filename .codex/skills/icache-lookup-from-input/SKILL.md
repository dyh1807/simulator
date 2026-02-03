---
name: icache-lookup-from-input
description: "ICache V1/V2: how register/SRAM lookup works after the lookup() split, and how to drive the lookup set view via lookup_from_input + lookup_set_{data,tag,valid} inputs."
---

# ICache lookup split + lookup_from_input (V1/V2)

Use this skill when you need to:
- Refactor/extend the ICache “stage1 lookup” path without touching stage2 hit/miss logic too much
- Switch between **register-style** lookup and **SRAM-style** lookup (latency model)
- Feed the lookup “set view” (tag/data/valid) from outside the icache via module inputs
- Validate correctness in the simulator across V1/V2 and reg/SRAM lookup modes

## Interfaces / knobs

### Build-time macros (both V1/V2)
- `ICACHE_USE_SRAM_MODEL`:
  - `0` = register lookup (combinational read from arrays)
  - `1` = SRAM lookup model (single outstanding lookup + latency)
- `ICACHE_SRAM_FIXED_LATENCY` (cycles, `1` means next-cycle ready; `0` is clamped to `1`)
- `ICACHE_SRAM_RANDOM_DELAY`, `ICACHE_SRAM_RANDOM_MIN`, `ICACHE_SRAM_RANDOM_MAX`

### Makefile hook
Use `EXTRA_CXXFLAGS` to inject macros without overriding the default `CXXFLAGS`:
```bash
make -j USE_SIM_DDR=1 USE_ICACHE_V2=1 EXTRA_CXXFLAGS='-DICACHE_USE_SRAM_MODEL=1 -DICACHE_SRAM_FIXED_LATENCY=4'
```

### lookup_from_input (data source select)
Both V1/V2 `*_in_t` add:
- `lookup_from_input` (bool)
- `lookup_set_data[WAYS][LINE_WORDS]`
- `lookup_set_tag[WAYS]`
- `lookup_set_valid[WAYS]`

Semantics:
- `lookup_from_input=0` (default): stage1 reads icache internal arrays
- `lookup_from_input=1`: stage1 reads the “set view” from the input fields above

Notes:
- V1 ways is compile-time `ICACHE_V1_WAYS`
- V2 input arrays are sized by compile-time `ICACHE_V2_WAYS`; when `lookup_from_input=1`, **require `cfg.ways <= ICACHE_V2_WAYS`**

## Implementation pattern (what to keep stable)

### 1) Split stage1 into 2 layers

**Layer A: set read (`lookup_read_set`)**
- Only responsibility: move `{data, tag, valid}` of the chosen set into stage1→stage2 wires/registers.
- This function is where `lookup_from_input` is handled:
  - internal source: read `cache_*[set][way]...`
  - external source: read `io.in.lookup_set_*[way]...`

**Layer B: control (`lookup`)**
- Only responsibility: handle:
  - IFU handshake (accept new request vs stall)
  - refetch/flush semantics for stage1
  - SRAM latency scheduling (`sram_pending`, delay counter, etc.) when enabled
- Calls `lookup_read_set()` with the right “read index” (pc index or pending index).

Stage2 hit/miss logic should only look at the **registered set view** from stage1.

### 2) Register lookup mode (`ICACHE_USE_SRAM_MODEL=0`)
Typical behavior:
- Read the set view for `pc_index` every comb.
- Gate `valid` bits with `ifu_req_valid` if the original design relied on this to prevent “ghost hits”.
- `ifu_req_ready` usually tracks whether stage2 is ready to accept the next lookup.

### 3) SRAM lookup mode (`ICACHE_USE_SRAM_MODEL=1`)
Typical behavior:
- Allow **at most one outstanding SRAM lookup**:
  - `sram_pending_r=1` means a previous lookup is waiting for SRAM latency to mature.
  - `sram_delay_r` counts down.
- `ifu_req_ready` should deassert when `sram_pending_r=1` (no new lookup can be accepted).
- When the latency matures (`sram_load_fire`), latch the set view into stage2 regs and assert stage1-valid for stage2.

## How to use `lookup_from_input` correctly

`lookup_from_input=1` is intended for cases where the set view is produced outside the icache module:
- an SRAM macro model that returns `{tag/data/valid}` after N cycles
- a wrapper that mirrors icache state and wants to “inject” the read result into stage1

Requirements:
- Before calling `icache_hw.comb()` each cycle, set:
  - `icache_hw.io.in.lookup_from_input = true`
  - and fill `lookup_set_{data,tag,valid}` for the set that stage1 will read **in that cycle**
- The icache still maintains internal state for fills/replacements; if the external producer is not a loopback, it must stay coherent with icache state (or the design will intentionally diverge).

### Temporary loopback for verification
When validating the split, you can loop internal arrays back into the input to prove equivalence:
- Use the helper `export_lookup_set_for_pc(pc, out_data, out_tag, out_valid)` (added to both V1/V2)
- Feed those exported arrays into `io.in.lookup_set_*`, then set `lookup_from_input=1`
- After validation, delete the loopback feed code in the top wrapper

## Simulator validation (quick recipe)

### Internal source (lookup_from_input=0)
Register lookup:
```bash
make clean && make -j USE_SIM_DDR=1 USE_ICACHE_V2=0
timeout 60s ./a.out baremetal/new_dhrystone/dhrystone.bin

make clean && make -j USE_SIM_DDR=1 USE_ICACHE_V2=1
timeout 60s ./a.out baremetal/new_dhrystone/dhrystone.bin
```

SRAM lookup (fixed latency example):
```bash
make clean && make -j USE_SIM_DDR=1 USE_ICACHE_V2=0 EXTRA_CXXFLAGS='-DICACHE_USE_SRAM_MODEL=1 -DICACHE_SRAM_FIXED_LATENCY=2'
timeout 60s ./a.out baremetal/new_dhrystone/dhrystone.bin

make clean && make -j USE_SIM_DDR=1 USE_ICACHE_V2=1 EXTRA_CXXFLAGS='-DICACHE_USE_SRAM_MODEL=1 -DICACHE_SRAM_FIXED_LATENCY=2'
timeout 60s ./a.out baremetal/new_dhrystone/dhrystone.bin
```

### External source (lookup_from_input=1)
- Add temporary “feed-before-comb” code in the icache top wrapper:
  - Fill `lookup_set_*` each cycle for the active PC (or pending SRAM PC), set `lookup_from_input=1`
- Run the same regressions above (both `ICACHE_USE_SRAM_MODEL=0/1`, and `USE_ICACHE_V2=0/1`)
- Remove the feed code once validated
