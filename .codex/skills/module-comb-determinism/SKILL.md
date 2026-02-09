---
name: module-comb-determinism
description: "Define and verify simulator-module combinational determinism: generalized Output must be fully determined by generalized Input, and lookup-from-input mode must not depend on hidden/internal table state."
---

# Module Combinational Determinism (Generalized IO)

Use this skill when you need to define, check, or enforce “synthesizability-like” behavior for simulator modules:
- Given one cycle’s generalized Input, generalized Output is uniquely determined.
- Output does not depend on uninitialized locals or hidden historical state outside generalized Input.
- In lookup-from-input mode, lookup result dependency is only from generalized Input (not internal table storage).

This skill is module-generic and applies to icache/tlb/ptw-style designs.

## Determinism contract

### Generalized IO split
- Generalized Input = `ExtIn + Regs + LookupIn`
- Generalized Output = `ExtOut + RegWrite + TableWrite`

### Required property
For fixed generalized Input `GI`, the comb result `GO = F(GI)` must be deterministic:
- No dependence on previous comb invocations (except through `Regs` carried in `GI`).
- No dependence on module-global mutable state not represented in `GI`.
- No dependence on uninitialized stack/heap data.

## Coding rules

### 1) One-time deterministic initialization at comb entry
At the start of each outer `comb()` call (not inner iterations):
- `out = {}`
- `reg_write = regs` (hold-by-default) or explicit deterministic baseline
- `table_write = {}`

Do not clear outputs inside iterative comb loops; this can erase earlier phase outputs.

### 2) No hidden mutable dependencies
Inside `comb()`:
- Do not read/write mutable globals, static mutable variables, or random generators directly.
- If historical state is required, it must be in `Regs`.
- Lookup backend state must be represented by `LookupIn`/`TableWrite` protocol, not hidden arrays.

### 3) Lookup-from-input strictness
When `lookup_from_input` is enabled (macro or runtime switch):
- Lookup hit/data/tag/valid/resp-valid must come from `LookupIn`.
- Internal table arrays must not affect lookup decision.
- Pipeline behavior may still depend on in-flight state in `Regs`, but table content dependency must be externalized.

### 4) Seq isolation
`seq()` should apply only `reg_write` (and external table writes handled by backend/top).
- Typical form: `regs = reg_write`.
- Avoid recomputing logic in `seq`.

## Review checklist (per module)

1. `comb()` entry initializes all generalized outputs deterministically.
2. Every output field is either explicitly assigned or covered by deterministic initialization.
3. No use of uninitialized temporaries before read.
4. No hidden mutable state dependency outside generalized Input.
5. `lookup_from_input` path bypasses internal table read dependency.
6. `seq()` is a pure state-application step from comb results.
7. For delayed/SRAM-style lookup, pending/resource state lives in `Regs` and participates in generalized Input.

## Validation workflow

### Static review
- Inspect module header: IO split and field ownership.
- Inspect comb/seq: initialization, assignment coverage, hidden dependency points.
- Inspect lookup path switching logic.

### Dynamic sanity checks
- Build and run existing regressions (`dhrystone`, `coremark`, `linux boot` timeout run).
- Optional: A/B compare with lookup-from-input enabled/disabled using equivalent fed lookup data.

## Common failure patterns

- Output field never assigned and not covered by `out = {}`.
- Lookup mode switch still reads internal table in one branch.
- Random-latency logic reading random source directly in comb without state capture in `Regs`.
- Comb-phase helper mutating persistent state directly instead of writing `reg_write`/`table_write`.

## Minimal remediation pattern

1. Move hidden state into `Regs` or externalize through `LookupIn`.
2. Add deterministic output initialization at outer comb entry.
3. Enforce lookup-from-input branch to consume only `LookupIn`.
4. Keep `seq()` as state-apply only.
5. Re-run regressions and keep branch-specific logs for review.

