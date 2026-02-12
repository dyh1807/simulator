# Tools Overview

`tools/` scripts are grouped by function:

## 1) Generalized-IO generation
- Generic generators:
  - `gen_pi_po.py`
  - `gen_io_generator_outer.py`
  - `gen_io_generator_outer_header_only.py`
  - `gen_pi_po_bit_map.py`
- ICache generators:
  - `gen_icache_v1_pi_po.py`
  - `gen_icache_v2_pi_po.py`
  - `gen_icache_v1_io_generator_outer.py`
  - `gen_icache_v2_io_generator_outer.py`
  - `gen_icache_v1_io_generator_outer_header_only.py`
  - `gen_icache_v2_io_generator_outer_header_only.py`
- MMU generators:
  - `gen_mmu_tlb_pi_po.py`
  - `gen_mmu_ptw_pi_po.py`
  - `gen_mmu_tlb_io_generator_outer.py`
  - `gen_mmu_ptw_io_generator_outer.py`
  - `gen_mmu_tlb_io_generator_outer_header_only.py`
  - `gen_mmu_ptw_io_generator_outer_header_only.py`

## 2) Checkers / regression gates
- `check_comb_determinism.py`
- `check_icache_v2_generalized_io_consistency.py`
- `comb_determinism_harness.cpp`
- Shared entry helper for run scripts:
  - `regression_gate.py`

## 3) Commit message lint
- `commit_msg_lint.py`

## Notes
- Long regression scripts in repo root only run determinism gate when:
  - `ENABLE_DETERMINISM_CHECK=1`
- Optional gate tuning:
  - `DETERMINISM_SAMPLES`
  - `DETERMINISM_WARMUP`
  - `DETERMINISM_SEED`
  - `DETERMINISM_PROFILES=internal,external`

