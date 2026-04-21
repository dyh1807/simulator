#!/usr/bin/env python3
import argparse
import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"


def run(cmd, check=True, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=check, **kwargs)


def write_abs_flist(path, rtl_root):
    rtl_f = rtl_root / "flist" / "rtl.f"
    lines = []
    for raw in rtl_f.read_text().splitlines():
        raw = raw.strip()
        if not raw:
            continue
        if raw.startswith("+incdir+"):
            inc = raw[len("+incdir+"):]
            lines.append(f"+incdir+{rtl_root / inc}")
        else:
            lines.append(str(rtl_root / raw))
    lines.append(str(rtl_root / "tb" / "tb_axi_llc_subsystem_equiv_replay.v"))
    path.write_text("\n".join(lines) + "\n")


def prepare_seed(seed_path, seed_meta):
    expected = seed_meta.get("expected_diff", {})
    if not expected.get("disable_compare_policy"):
        return seed_path
    tmp_dir = TOOLS_DIR / "out" / "expected_diff_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    tmp_seed = tmp_dir / f"{seed_path.stem}_nopolicy.json"
    obj = dict(seed_meta)
    obj.pop("compare_policy", None)
    tmp_seed.write_text(json.dumps(obj, indent=2) + "\n")
    return tmp_seed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", action="append", default=[])
    ap.add_argument("--rtl-node", default="eda-03")
    ap.add_argument(
        "--submodule-root",
        default=str(REPO_ROOT / "axi-interconnect-kit"),
        help="path to the submodule worktree used for both C++ and RTL builds",
    )
    args = ap.parse_args()

    submodule_root = Path(args.submodule_root).resolve()
    rtl_root = submodule_root / "rtl"
    seeds = args.seed or [
        str(REPO_ROOT / "tests" / "equiv" / "seeds" / "invalidate_all_idle_accept.json"),
        str(REPO_ROOT / "tests" / "equiv" / "seeds" / "invalidate_line_during_other_write.json"),
        str(REPO_ROOT / "tests" / "equiv" / "seeds" / "mode1_mmio_write_id_reuse_overlap.json"),
    ]

    for seed in seeds:
        seed_path = Path(seed).resolve()
        seed_meta = json.loads(seed_path.read_text())
        expected = seed_meta.get("expected_diff", {})
        if not expected:
            raise RuntimeError(f"seed missing expected_diff metadata: {seed_path}")

        effective_seed = prepare_seed(seed_path, seed_meta)
        case_name = seed_path.stem
        out_dir = TOOLS_DIR / "out" / f"expected_diff_{case_name}"
        gen_dir = out_dir / "generated"
        cpp_build = out_dir / "cpp"
        rtl_build = out_dir / "rtl"
        out_dir.mkdir(parents=True, exist_ok=True)
        gen_dir.mkdir(parents=True, exist_ok=True)
        cpp_build.mkdir(parents=True, exist_ok=True)
        rtl_build.mkdir(parents=True, exist_ok=True)

        run(["python3", str(TOOLS_DIR / "gen_case.py"), str(effective_seed), str(gen_dir)])

        cpp_bin = cpp_build / "equiv_cpp_runner"
        cpp_trace = out_dir / "cpp_trace.txt"
        rtl_trace = out_dir / "rtl_trace.txt"
        compare_log = out_dir / "compare.txt"

        run([
            "g++", "-std=c++17", "-O0", "-g",
            str(TOOLS_DIR / "cpp" / "equiv_case_runner.cpp"),
            str(submodule_root / "axi_interconnect" / "AXI_Interconnect.cpp"),
            str(submodule_root / "axi_interconnect" / "AXI_LLC.cpp"),
            "-I", str(gen_dir),
            "-I", str(submodule_root / "axi_interconnect" / "include"),
            "-I", str(submodule_root / "axi_interconnect"),
            "-I", str(submodule_root / "include"),
            "-I", str(submodule_root / "sim_ddr" / "include"),
            "-o", str(cpp_bin),
        ])
        run([str(cpp_bin), str(cpp_trace)])

        abs_flist = rtl_build / "rtl_abs.f"
        write_abs_flist(abs_flist, rtl_root)
        compile_cmd = (
            f"source /eda-tools/eda-software/synopsys/source-scripts/bash_eda03 >/dev/null 2>&1 && "
            f"cd {rtl_build} && "
            f"vcs -full64 -sverilog -f {abs_flist} +incdir+{gen_dir} "
            f"-o simv -l compile.log && "
            f"./simv +trace_file={rtl_trace} -l run.log"
        )
        run(["ssh", args.rtl_node, compile_cmd])

        compare_cmd = [
            "python3",
            str(TOOLS_DIR / "compare_trace.py"),
            str(cpp_trace),
            str(rtl_trace),
        ]
        compare_proc = run(compare_cmd, check=False, capture_output=True, text=True)
        compare_log.write_text(compare_proc.stdout)
        if compare_proc.returncode == 0:
            raise RuntimeError(f"expected compare failure but got PASS: {case_name}")

        for snippet in expected.get("must_contain", []):
            if snippet not in compare_proc.stdout:
                raise RuntimeError(
                    f"expected diff snippet missing for {case_name}: {snippet!r}"
                )
        print(f"case {case_name}: EXPECTED_DIFF_PASS")


if __name__ == "__main__":
    main()
