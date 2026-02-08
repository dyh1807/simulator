---
name: generalized-io-bitvector
description: "Refactor a module into generalized IO (ext-in + regs + lookup-in / ext-out + reg-write + table-write) and auto-generate bool* pi/po bitvectors (width + pack/unpack) for simulation/verification."
---

# Generalized IO + `bool* pi/po` Bitvector

When refactoring simulator modules to be easier to test/plug-in/trace, use a *generalized IO* split and a deterministic *bitvector flattening* scheme.

## 1) Generalized IO split (module-generic)

Define these data structures (names are flexible, roles are not):

- **`ExtIn`**: the original “external input signals” from other modules.
  - Keep *only* true external inputs (handshake/control/data).
  - Do **not** mix in “table read results” (lookup-set views) here.
- **`Regs`**: the module’s sequential state (pipeline regs/FSM regs/internal arrays), excluding perf counters.
  - Prefer types with an unambiguous bit width (`bool`, `uint{8,16,32,64}_t`, `wireK_t`, `regK_t`).
  - Avoid `enum` fields inside `Regs` unless your bitvector generator supports them (recommended: store as `uint8_t` / `regK_t` and cast).
- **`LookupIn`**: “table read result” style inputs (split out from the old `input` struct).
  - Example: `lookup_from_input` + `lookup_set_{data,tag,valid}`.
  - These model either (a) SRAM outputs, or (b) externally-fed register views for verification.

And on the output side:

- **`ExtOut`**: the original output signals to other modules.
- **`RegWrite`**: “register write” results that are **always** applied in `seq` for the current cycle.
  - Treat this as the *next-state* of the module’s sequential regs (pipeline regs/FSM regs/latency regs), excluding perf counters.
  - Recommended style: `RegWrite` carries the full next value for each reg; `seq` assigns regs unconditionally (regs hold their old value by default via `comb`).
- **`TableWrite`**: explicit write controls for updating the table state (single- or multi-port).
  - Minimal shape: `we` + `waddr` + `wdata`. For caches, `wdata` often includes `{line_data, tag, valid}`.
  - `TableWrite` may target different backends:
    - **Reg-table**: table state is maintained as normal arrays inside `Regs` (or a dedicated table struct); `seq` applies `TableWrite`.
    - **SRAM-table**: table state lives in an SRAM model/top-level memory; `seq` may *not* directly update arrays, but the SRAM consumes `TableWrite`.

Recommended aggregation (keeps ordering stable for packing):

- **Generalized input**: `ExtIn` + `Regs` + `LookupIn`
- **Generalized output**: `ExtOut` + `RegWrite` + `TableWrite`

## 1.5) Comb-phase output initialization (important)

At the beginning of each `comb()` call, initialize generalized outputs to deterministic defaults:

- `ExtOut`: initialize default valid/ready/data fields
- `RegWrite`: initialize to a deterministic baseline
  - common pattern: `reg_write = regs` (hold-by-default next-state)
- `TableWrite`: initialize to no-write (`we=0`, addresses/data cleared)

For modules with multi-phase comb or convergence loops:

- Prefer **one-time initialization per `comb()` call**, not per inner iteration.
- If outputs are produced across multiple comb sub-functions (for example phase1 writes MMU request and phase2 writes cache/memory signals), repeated clearing inside the loop can erase phase1 outputs and change behavior.

## 2) Deterministic flattening to `bool* pi` / `bool* po`

Goal: provide a *stable* bit ordering so any harness can drive/read the module via raw `bool` vectors.

### Ordering rules

- Pack fields in **struct declaration order**.
- For arrays: flatten **row-major** (`a[0][0]...a[0][N-1], a[1][0]...`).
- For scalar integers: pack bits **LSB-first** (`bit0, bit1, ...`).

### Width rules

- `bool`: 1
- `uint8_t`: 8
- `uint16_t`: 16 (if you need a non-16 logical width, use `wireK_t/regK_t` or an override mapping)
- `uint32_t`: 32
- `uint64_t`: 64
- `wireK_t` / `regK_t`: K (parse the decimal digits)
- Multi-d arrays: concatenate element-by-element

### Generator expectation

Write a Python generator that:

1. Parses the chosen header for `struct` fields (scalars + fixed-size arrays).
2. Computes `PI_WIDTH` / `PO_WIDTH`.
3. Emits C++ helpers:
   - `pack_pi(const GenIn&, bool* pi)` / `unpack_pi(const bool* pi, GenIn&)`
   - `pack_po(const GenOut&, bool* po)` / `unpack_po(const bool* po, GenOut&)`

If some types need custom widths (e.g., project-local typedefs), add a small `type_width_overrides` map (JSON/YAML) consumed by the generator.

## 3) Verification checklist (quick)

- **Baseline**: `lookup_from_input=0` → module should still work with its internally maintained tables.
- **External-feed path**: `lookup_from_input=1` → before each `comb()`, feed the current table set-view into `LookupIn`, then check behavior matches baseline.
- Keep any feed code *temporary*; remove after validation.
