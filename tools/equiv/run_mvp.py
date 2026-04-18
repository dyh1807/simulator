#!/usr/bin/env python3
import argparse
import time
import shutil
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"


def run(cmd, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def wait_for_file(path, timeout_s=5.0):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if path.exists():
            return
        time.sleep(0.1)
    raise FileNotFoundError(f"timed out waiting for file: {path}")


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
        str(REPO_ROOT / "tests" / "equiv" / "seeds" / "mode1_bypass_rw.json"),
        str(REPO_ROOT / "tests" / "equiv" / "seeds" / "mode_transition_flush_write_block.json"),
    ]

    for seed in seeds:
        seed_path = Path(seed).resolve()
        case_name = seed_path.stem
        out_dir = TOOLS_DIR / "out" / case_name
        gen_dir = out_dir / "generated"
        cpp_build = out_dir / "cpp"
        rtl_build = out_dir / "rtl"
        out_dir.mkdir(parents=True, exist_ok=True)
        gen_dir.mkdir(parents=True, exist_ok=True)
        cpp_build.mkdir(parents=True, exist_ok=True)
        rtl_build.mkdir(parents=True, exist_ok=True)

        run(["python3", str(TOOLS_DIR / "gen_case.py"), str(seed_path), str(gen_dir)])

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
        wait_for_file(rtl_trace)

        with open(compare_log, "w") as fp:
            run(["python3", str(TOOLS_DIR / "compare_trace.py"),
                 str(cpp_trace), str(rtl_trace)], stdout=fp)
        print(f"case {case_name}: PASS")


if __name__ == "__main__":
    main()
