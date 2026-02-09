#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
from dataclasses import dataclass


@dataclass
class Profile:
    name: str
    cxxflags: list[str]


def run_cmd(cmd: list[str], cwd: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=cwd,
        text=True,
        capture_output=True,
        check=True,
    )


def compile_harness(repo_root: str, output_bin: str, cxxflags: list[str]) -> None:
    cmd = [
        "g++",
        "-std=c++2a",
        "-O2",
        "-DUSE_SIM_DDR",
        "-I.",
        "-I./include",
        "-I./back-end/include",
        "-I./front-end",
        "-I./mmu/include",
        "tools/comb_determinism_harness.cpp",
        "front-end/icache/icache_module.cpp",
        "front-end/icache/icache_module_v2.cpp",
        "mmu/tlb_module.cpp",
        "mmu/ptw_module.cpp",
        "-o",
        output_bin,
    ] + cxxflags
    run_cmd(cmd, repo_root)


def run_profile(
    repo_root: str,
    profile: Profile,
    samples: int,
    warmup: int,
    seed: int,
    extra_flags: list[str],
) -> str:
    with tempfile.TemporaryDirectory(prefix="comb_det_") as td:
        out_bin = os.path.join(td, "comb_det")
        all_flags = profile.cxxflags + extra_flags
        compile_harness(repo_root, out_bin, all_flags)
        proc = run_cmd([out_bin, str(samples), str(warmup), str(seed)], repo_root)
        return proc.stdout


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Compile and run generalized-IO comb determinism checks: "
            "same PI -> two comb outputs, and cross-history same PI output compare."
        )
    )
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument(
        "--profile",
        action="append",
        choices=["internal", "external"],
        help=(
            "internal: default lookup source behavior; "
            "external: force lookup_from_input path for icache v1/v2 and tlb"
        ),
    )
    parser.add_argument(
        "--extra-cxxflag",
        action="append",
        default=[],
        help="Additional compiler flag passed to harness compilation",
    )
    args = parser.parse_args()

    profiles = args.profile if args.profile else ["internal", "external"]
    repo_root = os.path.normpath(os.path.join(os.path.dirname(__file__), os.pardir))

    profile_defs: dict[str, Profile] = {
        "internal": Profile(
            "internal",
            [
                "-DICACHE_V2_GENERALIZED_IO_MODE=1",
            ],
        ),
        "external": Profile(
            "external",
            [
                "-DICACHE_V2_GENERALIZED_IO_MODE=1",
                "-DICACHE_LOOKUP_FROM_INPUT=1",
                "-DICACHE_V2_LOOKUP_FROM_INPUT=1",
                "-DMMU_TLB_LOOKUP_FROM_INPUT=1",
            ],
        ),
    }

    for name in profiles:
        profile = profile_defs[name]
        print(f"=== profile: {profile.name} ===")
        output = run_profile(
            repo_root=repo_root,
            profile=profile,
            samples=args.samples,
            warmup=args.warmup,
            seed=args.seed,
            extra_flags=args.extra_cxxflag,
        )
        print(output, end="" if output.endswith("\n") else "\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

