#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
from typing import List


def truthy_env(name: str, default: str = "0") -> bool:
    return os.environ.get(name, default).strip().lower() in (
        "1",
        "true",
        "yes",
        "on",
    )


def _parse_profiles(raw: str) -> List[str]:
    profiles = [item.strip() for item in raw.split(",") if item.strip()]
    return profiles if profiles else ["internal", "external"]


def maybe_run_determinism_check(*, repo_root: str) -> None:
    if not truthy_env("ENABLE_DETERMINISM_CHECK", "0"):
        return

    samples = int(os.environ.get("DETERMINISM_SAMPLES", "16"))
    warmup = int(os.environ.get("DETERMINISM_WARMUP", "4"))
    seed = int(os.environ.get("DETERMINISM_SEED", "12345"))
    profiles = _parse_profiles(
        os.environ.get("DETERMINISM_PROFILES", "internal,external")
    )

    cmd = [
        sys.executable,
        os.path.join(repo_root, "tools", "check_comb_determinism.py"),
        "--samples",
        str(samples),
        "--warmup",
        str(warmup),
        "--seed",
        str(seed),
    ]
    for profile in profiles:
        if profile not in ("internal", "external"):
            print(
                f"[regression-check] invalid DETERMINISM_PROFILES item: {profile}",
                file=sys.stderr,
            )
            raise SystemExit(2)
        cmd.extend(["--profile", profile])

    print("[regression-check] running determinism gate:")
    print("  " + " ".join(cmd))
    result = subprocess.run(cmd, cwd=repo_root)
    if result.returncode != 0:
        print(
            f"[regression-check] determinism gate failed (rc={result.returncode})",
            file=sys.stderr,
        )
        raise SystemExit(result.returncode)
    print("[regression-check] determinism gate passed")

