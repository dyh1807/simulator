#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Dict, Optional


@dataclass
class RunResult:
    mode: int
    image: str
    status: str
    returncode: int
    timeout_s: int
    metrics: Dict[str, float]
    log_path: str


def parse_metrics(output: str) -> Dict[str, float]:
    metrics: Dict[str, float] = {}
    patterns = {
        "inst_num": r"instruction num:\s+(\d+)",
        "cycles": r"cycle\s+num:\s+(\d+)",
        "ipc": r"ipc\s+ :\s+([0-9.]+)",
        "icache_acc": r"icache accuracy :\s+([0-9.]+)",
        "bpu_acc": r"bpu\s+accuracy :\s+([0-9.]+)",
    }
    for key, pat in patterns.items():
        m = re.search(pat, output)
        if not m:
            continue
        if key in {"inst_num", "cycles"}:
            metrics[key] = float(int(m.group(1)))
        else:
            metrics[key] = float(m.group(1))
    return metrics


def classify_status(returncode: int, timed_out: bool, output: str) -> str:
    if timed_out:
        return "timeout"
    lower = output.lower()
    if "difftest" in lower and "error" in lower:
        return "difftest_error"
    if returncode == 0:
        return "ok"
    return "nonzero_exit"


def run_cmd(args: list[str], cwd: str, timeout_s: int) -> tuple[int, bool, str]:
    try:
        proc = subprocess.run(
            args,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout_s,
            check=False,
        )
        return proc.returncode, False, proc.stdout
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout if isinstance(exc.stdout, str) else ""
        return 124, True, output


def build_mode(*, repo_root: str, mode: int, target_inst: int) -> tuple[bool, str, str, int]:
    extra_cxxflags = (
        f"-DICACHE_V2_GENERALIZED_IO_MODE={mode} -DMAX_COMMIT_INST={target_inst}"
    )
    build_cmd = [
        "make",
        "-j",
        "USE_SIM_DDR=1",
        "USE_ICACHE_V2=1",
        f"EXTRA_CXXFLAGS={extra_cxxflags}",
    ]
    clean_rc, _, clean_out = run_cmd(["make", "clean"], repo_root, timeout_s=600)
    if clean_rc != 0:
        log_path = os.path.join("/tmp", f"icache_v2_mode{mode}_clean.log")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(clean_out)
        return False, "", log_path, clean_rc

    build_rc, build_to, build_out = run_cmd(build_cmd, repo_root, timeout_s=1200)
    if build_rc != 0 or build_to:
        log_path = os.path.join("/tmp", f"icache_v2_mode{mode}_build.log")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(build_out)
        return False, "", log_path, build_rc

    binary_path = os.path.join("/tmp", f"icache_v2_mode{mode}.out")
    cp_rc, cp_to, cp_out = run_cmd(["cp", "a.out", binary_path], repo_root, timeout_s=30)
    if cp_rc != 0 or cp_to:
        log_path = os.path.join("/tmp", f"icache_v2_mode{mode}_copy.log")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(cp_out)
        return False, "", log_path, cp_rc

    return True, binary_path, "", 0


def run_single(
    *,
    binary_path: str,
    mode: int,
    image: str,
    repo_root: str,
    timeout_s: int,
) -> RunResult:
    image_path = image if os.path.isabs(image) else os.path.join(repo_root, image)
    run_rc, run_to, run_out = run_cmd(
        [binary_path, image_path], repo_root, timeout_s=timeout_s
    )
    status = classify_status(run_rc, run_to, run_out)
    metrics = parse_metrics(run_out)
    log_path = os.path.join(
        "/tmp", f"icache_v2_mode{mode}_{os.path.basename(image)}.log"
    )
    with open(log_path, "w", encoding="utf-8") as f:
        f.write(run_out)
    return RunResult(
        mode=mode,
        image=image,
        status=status,
        returncode=run_rc,
        timeout_s=timeout_s,
        metrics=metrics,
        log_path=log_path,
    )


def metric_close(a: Optional[float], b: Optional[float], key: str) -> bool:
    if a is None or b is None:
        return a is None and b is None
    if key in {"inst_num", "cycles"}:
        return int(a) == int(b)
    return abs(a - b) <= 1e-9


def main() -> int:
    ap = argparse.ArgumentParser(
        description=(
            "Strong consistency self-check for ICacheV2 generalized IO mode: "
            "compare mode=0 and mode=1 on dhrystone/coremark/linux."
        )
    )
    ap.add_argument(
        "--repo-root",
        default=os.path.normpath(os.path.join(os.path.dirname(__file__), os.pardir)),
    )
    ap.add_argument("--target-inst", type=int, default=150000000)
    ap.add_argument("--timeout-dhry", type=int, default=60)
    ap.add_argument("--timeout-coremark", type=int, default=180)
    ap.add_argument("--timeout-linux", type=int, default=120)
    args = ap.parse_args()

    images = [
        ("baremetal/new_dhrystone/dhrystone.bin", args.timeout_dhry),
        ("baremetal/new_coremark/coremark.bin", args.timeout_coremark),
        ("baremetal/linux.bin", args.timeout_linux),
    ]

    all_ok = True
    print("=== ICacheV2 generalized-IO consistency check ===")
    print(f"repo: {args.repo_root}")
    print(f"target_inst: {args.target_inst}")

    ok0, bin0, log0, rc0 = build_mode(
        repo_root=args.repo_root, mode=0, target_inst=args.target_inst
    )
    if not ok0:
        print(f"[build] mode0 failed rc={rc0} log={log0}")
        return 1
    ok1, bin1, log1, rc1 = build_mode(
        repo_root=args.repo_root, mode=1, target_inst=args.target_inst
    )
    if not ok1:
        print(f"[build] mode1 failed rc={rc1} log={log1}")
        return 1

    for image, timeout_s in images:
        print(f"\n[case] {image} (timeout={timeout_s}s)")
        r0 = run_single(
            binary_path=bin0,
            mode=0,
            image=image,
            repo_root=args.repo_root,
            timeout_s=timeout_s,
        )

        r1 = run_single(
            binary_path=bin1,
            mode=1,
            image=image,
            repo_root=args.repo_root,
            timeout_s=timeout_s,
        )

        print(
            f"  mode0 status={r0.status} rc={r0.returncode} log={r0.log_path}"
        )
        print(
            f"  mode1 status={r1.status} rc={r1.returncode} log={r1.log_path}"
        )

        if r0.status != r1.status:
            all_ok = False
            print("  [mismatch] status differs")
            continue

        for key in ["inst_num", "cycles", "ipc", "icache_acc", "bpu_acc"]:
            a = r0.metrics.get(key)
            b = r1.metrics.get(key)
            if not metric_close(a, b, key):
                all_ok = False
                print(f"  [mismatch] {key}: mode0={a} mode1={b}")

    if not all_ok:
        print("\nRESULT: FAILED")
        return 1

    print("\nRESULT: PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
