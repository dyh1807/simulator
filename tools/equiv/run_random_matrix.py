#!/usr/bin/env python3
import argparse
import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"
OUT_ROOT = TOOLS_DIR / "out" / "random_matrix"


def run(cmd, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=32)
    ap.add_argument(
        "--root-seed",
        dest="root_seeds",
        action="append",
        type=int,
        required=True,
        help="repeatable root seed for one random-smoke batch",
    )
    ap.add_argument("--rtl-node", default="eda-03")
    ap.add_argument(
        "--submodule-root",
        default=str(REPO_ROOT / "axi-interconnect-kit"),
        help="path to the submodule worktree used for both C++ and RTL builds",
    )
    args = ap.parse_args()

    submodule_root = Path(args.submodule_root).resolve()
    run_dir = OUT_ROOT / f"matrix_c{args.count}_{'_'.join(str(x) for x in args.root_seeds)}"
    run_dir.mkdir(parents=True, exist_ok=True)

    summary = {
        "count": args.count,
        "root_seeds": args.root_seeds,
        "rtl_node": args.rtl_node,
        "submodule_root": str(submodule_root),
        "batches": [],
    }

    for root_seed in args.root_seeds:
        batch_cmd = [
            "python3",
            str(TOOLS_DIR / "run_random_smoke.py"),
            "--count",
            str(args.count),
            "--root-seed",
            str(root_seed),
            "--rtl-node",
            args.rtl_node,
            "--submodule-root",
            str(submodule_root),
        ]
        run(batch_cmd)
        summary["batches"].append(
            {
                "root_seed": root_seed,
                "count": args.count,
                "seedset_dir": str(
                    TOOLS_DIR / "out" / "random_smoke" / f"seedset_{root_seed}_{args.count}"
                ),
                "status": "PASS",
            }
        )

    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    (run_dir / "summary.txt").write_text(
        "\n".join(
            [
                f"count={args.count}",
                f"rtl_node={args.rtl_node}",
                f"submodule_root={submodule_root}",
                "root_seeds=" + ",".join(str(x) for x in args.root_seeds),
            ]
            + [
                f"PASS root_seed={batch['root_seed']} count={batch['count']} seedset_dir={batch['seedset_dir']}"
                for batch in summary["batches"]
            ]
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
