---
name: pi-po-toolchain
description: "Generalized-IO toolchain for bool* PI/PO: generate pi_po headers, export PI/PO bit-to-signal CSV maps, and query pi:K/po:K ownership for debugging and integration."
---

# PI/PO Toolchain (Generalized IO)

Use this skill when you need to:
- Generate generalized-IO `bool* pi/po` pack/unpack headers from module structs
- Export stable PI/PO bit mapping CSV files (`bit_lsb/bit_msb -> signal`)
- Query a specific bit (`pi:K` or `po:K`) to find the owning signal
- Diagnose missing PI/PO fields after struct changes

This skill is module-generic. It works for icache and any module that follows the same generalized-IO split.

## Tool scripts

- `tools/gen_pi_po.py`
  - Generic header generator (produces `*_pi_po.h`)
- `tools/gen_pi_po_bit_map.py`
  - Generic PI/PO bit map generator and bit query tool
- Project wrappers (icache examples):
  - `tools/gen_icache_v1_pi_po.py`
  - `tools/gen_icache_v2_pi_po.py`

## Standard workflow

1) Update module structs (`ExtIn + Regs + LookupIn`, `ExtOut + RegWrite + TableWrite`).

2) Re-generate PI/PO headers:
```bash
python tools/gen_icache_v1_pi_po.py
python tools/gen_icache_v2_pi_po.py
```

3) Re-generate or query PI/PO bit maps:
```bash
python tools/gen_pi_po_bit_map.py \
  --header front-end/icache/include/icache_module.h \
  --namespace icache_module_n \
  --pi-field in:ICache_in_t \
  --pi-field regs:ICache_regs_t \
  --pi-field lookup_in:ICache_lookup_in_t \
  --po-field out:ICache_out_t \
  --po-field reg_write:ICache_regs_t \
  --po-field table_write:ICache_table_write_t \
  --include-dir front-end \
  --include-dir back-end/include \
  --include-dir include \
  --out-prefix /tmp/icache_v1_map \
  --query pi:0 \
  --query po:0
```

4) If needed, generate per-bit expanded CSV:
```bash
python tools/gen_pi_po_bit_map.py ... --emit-per-bit-csv
```

## Output files

For prefix `<prefix>`:
- `<prefix>_pi_map.csv`
- `<prefix>_po_map.csv`
- optional: `<prefix>_pi_bits.csv`, `<prefix>_po_bits.csv`

`*_map.csv` row schema:
- `vector, bit_lsb, bit_msb, width, wrapper_field, struct_name, signal, type_name`

## Common pitfalls and checks

### 1) Field missing from PI/PO
Cause (historical): fields with initializer expressions (for example `static_cast<...>(...)`) can be dropped if parser order is wrong.

Current rule in `gen_pi_po.py`:
- Strip initializer first (`= ...`)
- Then filter out function-like declarations

If a field is still missing:
- Check struct declaration syntax is `type name[...];`
- Avoid unsupported types unless provided in `--override`

### 2) Width mismatch from array macros

`gen_pi_po_bit_map.py` evaluates array dimensions by compiling a tiny C++ program.
If this fails, pass required include paths:
- `--include-dir front-end`
- `--include-dir back-end/include`
- `--include-dir include`

### 3) Deterministic order

Bit order is fixed:
- Struct fields in declaration order
- Arrays in row-major order
- Bits in LSB-first order

## Repository policy for map CSV

Bit map CSV files are generated artifacts.
- Keep scripts in git
- Do not commit generated map CSV by default
- Ignore patterns (already configured in `.gitignore`):
  - `front-end/icache/include/icache_*_pi_map.csv`
  - `front-end/icache/include/icache_*_po_map.csv`
  - `front-end/icache/include/icache_*_pi_bits.csv`
  - `front-end/icache/include/icache_*_po_bits.csv`

## Adapting to another module

To reuse this toolchain for a new module:

1) Prepare wrapper fields:
- PI side: `--pi-field <wrapper_field>:<struct_name>`
- PO side: `--po-field <wrapper_field>:<struct_name>`

2) Add type overrides if needed:
```bash
--override CustomType=13
```

3) Run `gen_pi_po.py` then `gen_pi_po_bit_map.py`.

Recommended: create a module-specific wrapper script (same style as `gen_icache_v1_pi_po.py`) so generation is one command.
