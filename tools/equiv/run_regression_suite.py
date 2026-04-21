#!/usr/bin/env python3
import argparse
import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"
OUT_ROOT = TOOLS_DIR / "out" / "regression_suite"


def run(cmd, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rtl-node", default="eda-03")
    ap.add_argument(
        "--submodule-root",
        default=str(REPO_ROOT / "axi-interconnect-kit"),
        help="path to the submodule worktree used for both C++ and RTL builds",
    )
    ap.add_argument("--skip-pass", action="store_true")
    ap.add_argument("--skip-expected-diff", action="store_true")
    ap.add_argument("--skip-maintenance-contracts", action="store_true")
    ap.add_argument("--skip-random-matrix", action="store_true")
    ap.add_argument("--matrix-count", type=int, default=16)
    ap.add_argument(
        "--matrix-root-seed",
        dest="matrix_root_seeds",
        action="append",
        type=int,
        default=[],
        help="repeatable root seed for the random matrix",
    )
    args = ap.parse_args()

    submodule_root = Path(args.submodule_root).resolve()
    matrix_root_seeds = args.matrix_root_seeds or [20260423]
    suite_id = (
        f"pass_{int(not args.skip_pass)}"
        f"_xdiff_{int(not args.skip_expected_diff)}"
        f"_maint_{int(not args.skip_maintenance_contracts)}"
        f"_matrix_{int(not args.skip_random_matrix)}"
    )
    run_dir = OUT_ROOT / suite_id
    run_dir.mkdir(parents=True, exist_ok=True)

    summary = {
        "rtl_node": args.rtl_node,
        "submodule_root": str(submodule_root),
        "skip_pass": args.skip_pass,
        "skip_expected_diff": args.skip_expected_diff,
        "skip_maintenance_contracts": args.skip_maintenance_contracts,
        "skip_random_matrix": args.skip_random_matrix,
        "matrix_count": args.matrix_count,
        "matrix_root_seeds": matrix_root_seeds,
        "steps": [],
    }

    if not args.skip_pass:
        run(
            [
                "python3",
                str(TOOLS_DIR / "run_mvp.py"),
                "--rtl-node",
                args.rtl_node,
                "--submodule-root",
                str(submodule_root),
            ]
        )
        summary["steps"].append({"name": "pass_regression", "status": "PASS"})

    if not args.skip_expected_diff:
        run(
            [
                "python3",
                str(TOOLS_DIR / "run_expected_diff.py"),
                "--rtl-node",
                args.rtl_node,
                "--submodule-root",
                str(submodule_root),
            ]
        )
        summary["steps"].append({"name": "expected_diff_regression", "status": "PASS"})

    if not args.skip_maintenance_contracts:
        run(
            [
                "python3",
                str(TOOLS_DIR / "run_maintenance_contracts.py"),
            ]
        )
        summary["steps"].append(
            {"name": "maintenance_contract_regression", "status": "PASS"}
        )

    if not args.skip_random_matrix:
        cmd = [
            "python3",
            str(TOOLS_DIR / "run_random_matrix.py"),
            "--count",
            str(args.matrix_count),
            "--rtl-node",
            args.rtl_node,
            "--submodule-root",
            str(submodule_root),
        ]
        for root_seed in matrix_root_seeds:
            cmd.extend(["--root-seed", str(root_seed)])
        run(cmd)
        summary["steps"].append(
            {
                "name": "random_matrix_regression",
                "status": "PASS",
                "count": args.matrix_count,
                "root_seeds": matrix_root_seeds,
            }
        )

    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    (run_dir / "summary.txt").write_text(
        "\n".join(
            [
                f"rtl_node={args.rtl_node}",
                f"submodule_root={submodule_root}",
                f"skip_pass={args.skip_pass}",
                f"skip_expected_diff={args.skip_expected_diff}",
                f"skip_maintenance_contracts={args.skip_maintenance_contracts}",
                f"skip_random_matrix={args.skip_random_matrix}",
                f"matrix_count={args.matrix_count}",
                "matrix_root_seeds=" + ",".join(str(x) for x in matrix_root_seeds),
            ]
            + [f"PASS {step['name']}" for step in summary["steps"]]
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
