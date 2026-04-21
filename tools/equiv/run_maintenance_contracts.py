#!/usr/bin/env python3
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"


def run(cmd, check=True, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=check, **kwargs)


def write_abs_flist(path, rtl_root, tb_name):
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
    lines.append(str(rtl_root / "tb" / tb_name))
    path.write_text("\n".join(lines) + "\n")


def main():
    submodule_root = (REPO_ROOT / "axi-interconnect-kit").resolve()
    rtl_root = submodule_root / "rtl"
    out_dir = TOOLS_DIR / "out" / "maintenance_contracts"
    cpp_dir = out_dir / "cpp"
    rtl_dir = out_dir / "rtl"
    out_dir.mkdir(parents=True, exist_ok=True)
    cpp_dir.mkdir(parents=True, exist_ok=True)
    rtl_dir.mkdir(parents=True, exist_ok=True)

    cpp_bin = cpp_dir / "invalidate_all_contract_runner"
    run([
        "g++", "-std=c++17", "-O0", "-g",
        str(TOOLS_DIR / "cpp" / "invalidate_all_contract_runner.cpp"),
        str(submodule_root / "axi_interconnect" / "AXI_Interconnect.cpp"),
        str(submodule_root / "axi_interconnect" / "AXI_LLC.cpp"),
        "-I", str(REPO_ROOT / "include"),
        "-I", str(REPO_ROOT / "back-end" / "include"),
        "-I", str(REPO_ROOT),
        "-I", str(submodule_root / "axi_interconnect" / "include"),
        "-I", str(submodule_root / "axi_interconnect"),
        "-I", str(submodule_root / "include"),
        "-I", str(submodule_root / "sim_ddr" / "include"),
        "-o", str(cpp_bin),
    ])
    cpp_log = cpp_dir / "run.log"
    with cpp_log.open("w") as fp:
        run([str(cpp_bin)], stdout=fp, stderr=subprocess.STDOUT)

    abs_flist = rtl_dir / "rtl_abs.f"
    write_abs_flist(abs_flist, rtl_root, "tb_axi_llc_subsystem_invalidate_all_contract.v")
    compile_cmd = (
        "source /eda-tools/eda-software/synopsys/source-scripts/bash_eda03 >/dev/null 2>&1 && "
        f"cd {rtl_dir} && "
        f"vcs -full64 -sverilog -f {abs_flist} -o simv -l compile.log && "
        "./simv -l run.log"
    )
    run(["ssh", "eda-03", compile_cmd])

    rtl_log = rtl_dir / "run.log"
    text = rtl_log.read_text()
    if "PASS" not in text:
        raise RuntimeError("RTL invalidate_all contract did not report PASS")

    print("maintenance contracts: PASS")


if __name__ == "__main__":
    main()
