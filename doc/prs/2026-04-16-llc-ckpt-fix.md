## Summary

- fix LLC refill commit head-of-line blocking that could deadlock with pending victim-line writes
- restore CKPT warmup to use compile-time `WARMUP` by default
- let CKPT interval runs use compile-time `SIMPOINT_INTERVAL` by default in `run_all.sh`

## Root Cause

- LLC refill commit always selected the first `refill_valid` slot, even when that slot was blocked by a victim-line write dependency
- victim snapshot externalization treated all same-line pending writes as hard blockers, including replayable states
- CKPT default warmup was hardcoded to `10M` in `main.cpp`, so profile-configured `WARMUP` did not take effect
- `run_all.sh` passed `MAX_COMMIT_INST` to `-w`, so the script overrode CKPT warmup and indirectly bypassed config-driven interval setup

## Changes

- refine LLC victim-write blocking to wait only on non-replayable same-line writes
- allow refill commit to skip blocked slots and commit later ready refills
- add LLC tests that cover replayable/non-replayable victim writes and blocked-slot bypass
- use `WARMUP` as the CKPT default warmup target
- keep CKPT stop conditions on the warmup/measure path instead of the generic main-loop `max_commit` gate
- remove the script-level warmup override from `run_all.sh`

## Validation

- updated `axi_llc_test` with targeted deadlock regression coverage
- A/B short-window checkpoint comparison on `429.mcf_ref_run3/ckpt_sp25_target98_warmup1.gz` showed identical simulated cycles, IPC, and LLC counters between baseline and LLC-fix builds
- ran the target checkpoint with `large` profile and `-w 100000000`; it continued making forward progress past the previously reported deadlock window
