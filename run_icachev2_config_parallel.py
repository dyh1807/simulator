import atexit
import csv
import json
import multiprocessing
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple


# -----------------------------------------------------------------------------
# Experiment targets
# -----------------------------------------------------------------------------
images = [
    "./baremetal/new_dhrystone/dhrystone.bin",
    "./baremetal/new_coremark/coremark.bin",
    "./baremetal/linux.bin",
]

# Instruction target for experiment iterations (MAX_COMMIT_INST).
# Override via env for quick runs:
#   TARGET_INST=1500000 python3 run_icachev2_config_parallel.py
# Default: 15M (10x vs the quick 1.5M setting) so CoreMark/Dhrystone can finish
# and Linux can run longer without making the sweep prohibitively slow.
target_inst = int(os.environ.get("TARGET_INST", "15000000"))

# Timeouts (seconds); override via env if needed.
coremark_timeout = int(os.environ.get("COREMARK_TIMEOUT", "3000"))
dhrystone_timeout = int(os.environ.get("DHRYSTONE_TIMEOUT", "3000"))
linux_timeout = int(os.environ.get("LINUX_TIMEOUT", "3000"))

# Build backend controls (Makefile variables)
use_simddr = int(os.environ.get("USE_SIM_DDR", "1"))  # 1=SimDDR, 0=TrueICache


def get_timeout(image_path: str) -> int:
    base = os.path.basename(image_path)
    if "coremark" in base:
        return coremark_timeout
    if "dhrystone" in base:
        return dhrystone_timeout
    return linux_timeout


# -----------------------------------------------------------------------------
# ICacheV2Config sweep (edit here)
# -----------------------------------------------------------------------------
# Repl policy encoding:
#   0 = RR, 1 = RANDOM, 2 = PLRU
DEFAULT_V2_CFGS: List[Dict] = [
    # Baseline (matches current defaults)
    dict(name="v2_base", ways=8, mshr=4, rob=16, repl=2, pf=1, pf_dist=1, pf_reserve=1),
    # MSHR scaling
    dict(name="v2_mshr1", ways=8, mshr=1, rob=16, repl=2, pf=1, pf_dist=1, pf_reserve=0),
    dict(name="v2_mshr2", ways=8, mshr=2, rob=16, repl=2, pf=1, pf_dist=1, pf_reserve=1),
    dict(name="v2_mshr8", ways=8, mshr=8, rob=16, repl=2, pf=1, pf_dist=1, pf_reserve=2),
    # Prefetch off
    dict(name="v2_nopf", ways=8, mshr=4, rob=16, repl=2, pf=0, pf_dist=1, pf_reserve=1),
    # Replacement policy
    dict(name="v2_rr", ways=8, mshr=4, rob=16, repl=0, pf=1, pf_dist=1, pf_reserve=1),
    dict(name="v2_rand", ways=8, mshr=4, rob=16, repl=1, pf=1, pf_dist=1, pf_reserve=1),
    # Ways scaling (PLRU requires power-of-two ways; otherwise module falls back to RR)
    dict(name="v2_ways4", ways=4, mshr=4, rob=16, repl=2, pf=1, pf_dist=1, pf_reserve=1),
]


@dataclass(frozen=True)
class BuildCfg:
    name: str
    version: str  # "v1" or "v2"
    line_size: int = 32
    v2: Optional[Dict] = None


def make_build_cfgs(v2_cfgs: List[Dict]) -> List[BuildCfg]:
    cfgs: List[BuildCfg] = []
    cfgs.append(BuildCfg(name="v1_base", version="v1", line_size=32))
    for c in v2_cfgs:
        cfgs.append(BuildCfg(name=c["name"], version="v2", line_size=32, v2=c))
    return cfgs


def get_binary_name(cfg: BuildCfg) -> str:
    return f"sim_{cfg.name}"


def _run(cmd: List[str], *, check: bool, capture: bool = True) -> subprocess.CompletedProcess:
    kwargs = dict(text=True)
    if capture:
        kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return subprocess.run(cmd, check=check, **kwargs)


def compile_one(cfg: BuildCfg) -> Tuple[str, BuildCfg, Optional[str]]:
    """
    Returns (binary_name, cfg, error_str or None).
    """
    binary = get_binary_name(cfg)

    # Clean build to ensure flags are applied.
    _run(["make", "clean"], check=False, capture=False)

    base_flags = "-O3 -march=native -funroll-loops -mtune=native --std=c++2a"
    cxx_flags = f"{base_flags} -DMAX_COMMIT_INST={target_inst} -DICACHE_LINE_SIZE={cfg.line_size}"

    make_vars = [f"USE_SIM_DDR={use_simddr}"]
    # IMPORTANT:
    # This script passes CXXFLAGS on the command line. GNU make treats that as an
    # overriding assignment, so Makefile-side "CXXFLAGS +=" may not take effect
    # (e.g., -DUSE_SIM_DDR / -DUSE_ICACHE_V2). Therefore, we explicitly add
    # those compile definitions here when needed.
    if use_simddr == 1:
        cxx_flags += " -DUSE_SIM_DDR"
    if cfg.version == "v2":
        cxx_flags += " -DUSE_ICACHE_V2"
        assert cfg.v2 is not None
        v2 = cfg.v2
        cxx_flags += f" -DICACHE_V2_WAYS={int(v2['ways'])}"
        cxx_flags += f" -DICACHE_V2_MSHR_NUM={int(v2['mshr'])}"
        cxx_flags += f" -DICACHE_V2_ROB_DEPTH={int(v2['rob'])}"
        cxx_flags += f" -DICACHE_V2_REPL_POLICY={int(v2['repl'])}"
        cxx_flags += f" -DICACHE_V2_PREFETCH_ENABLE={int(v2['pf'])}"
        cxx_flags += f" -DICACHE_V2_PREFETCH_DISTANCE={int(v2['pf_dist'])}"
        cxx_flags += f" -DICACHE_V2_PREFETCH_RESERVE={int(v2['pf_reserve'])}"

    try:
        _run(["make", "-j", *make_vars, f"CXXFLAGS={cxx_flags}"], check=True, capture=True)
    except subprocess.CalledProcessError as e:
        err = (e.stderr or "")[-2000:]
        return binary, cfg, f"build_fail: {err}"

    try:
        _run(["mv", "a.out", binary], check=True, capture=True)
    except subprocess.CalledProcessError as e:
        err = (e.stderr or "")[-2000:]
        return binary, cfg, f"rename_fail: {err}"

    return binary, cfg, None


def compile_binaries(cfgs: List[BuildCfg]) -> List[Tuple[str, BuildCfg]]:
    print("Compiling binaries...")
    binaries: List[Tuple[str, BuildCfg]] = []
    for cfg in cfgs:
        binary, cfg_out, err = compile_one(cfg)
        if err is not None:
            print(f"Compilation failed for {cfg_out.name}: {err}")
            sys.exit(1)
        binaries.append((binary, cfg_out))
        print(f"Compiled {binary}")
    return binaries


def cleanup_binaries(binaries: List[Tuple[str, BuildCfg]]) -> None:
    for binary, _cfg in binaries:
        try:
            if os.path.exists(binary):
                os.remove(binary)
        except OSError:
            pass
    try:
        if os.path.exists("a.out"):
            os.remove("a.out")
    except OSError:
        pass


def parse_metrics(output: str) -> Dict:
    metrics: Dict = {}
    inst_match = re.search(r"instruction num:\s*(\d+)", output)
    if inst_match:
        metrics["inst_num"] = int(inst_match.group(1))
    ipc_match = re.search(r"ipc\s*:\s*([0-9.]+)", output)
    if ipc_match:
        metrics["ipc"] = float(ipc_match.group(1))
    icache_acc_match = re.search(r"icache accuracy\s*:\s*([0-9.]+)", output)
    if icache_acc_match:
        metrics["icache_acc"] = float(icache_acc_match.group(1))
    bpu_acc_match = re.search(r"bpu\s+accuracy\s*:\s*([0-9.]+)", output)
    if bpu_acc_match:
        metrics["bpu_acc"] = float(bpu_acc_match.group(1))

    mshr_peak_match = re.search(
        r"\[icache_v2\]\s+mshr_peak:\s*(\d+)\s*/\s*(\d+)\s*\(([0-9.eE+-]+)\)",
        output,
    )
    if mshr_peak_match:
        metrics["mshr_peak"] = int(mshr_peak_match.group(1))
        metrics["mshr_peak_cfg"] = int(mshr_peak_match.group(2))
        metrics["mshr_peak_ratio"] = float(mshr_peak_match.group(3))

    txid_peak_match = re.search(
        r"\[icache_v2\]\s+txid_peak:\s*(\d+)\s*/\s*16\s*\(([0-9.eE+-]+)\)",
        output,
    )
    if txid_peak_match:
        metrics["txid_peak"] = int(txid_peak_match.group(1))
        metrics["txid_peak_ratio"] = float(txid_peak_match.group(2))
    return metrics


def classify_status(output: str, rc: Optional[int], timed_out: bool) -> str:
    if timed_out:
        return "timeout"
    if re.search(r"Difftest:\s*error|Difftest", output):
        return "difftest"
    if re.search(r"TIME OUT!!!!", output):
        return "sim_timeout"
    if re.search(r"ERROR|Error", output):
        return "error"
    if "Success!!!!" in output:
        return "pass"
    if rc == 0:
        # Some images may not reach 'Success' but still exit cleanly (rare).
        return "ok"
    if rc is None:
        return "unknown"
    return f"rc{rc}"


def run_simulation(task):
    binary, image, cfg = task
    img_name = os.path.basename(image)
    timeout_s = get_timeout(image)

    start_time = time.time()
    timed_out = False
    rc: Optional[int] = None
    output = ""
    try:
        result = subprocess.run(
            [f"./{binary}", image],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_s,
        )
        end_time = time.time()
        rc = result.returncode
        output = (result.stdout or "") + "\n" + (result.stderr or "")
    except subprocess.TimeoutExpired as e:
        end_time = time.time()
        timed_out = True
        output = (e.stdout or "") + "\n" + (e.stderr or "")

    status = classify_status(output, rc, timed_out)
    metrics = parse_metrics(output)

    row = dict(
        image=img_name,
        cfg_name=cfg.name,
        version=cfg.version,
        status=status,
        elapsed=end_time - start_time,
    )
    if cfg.v2 is not None:
        row.update(
            ways=cfg.v2["ways"],
            mshr=cfg.v2["mshr"],
            rob=cfg.v2["rob"],
            repl=cfg.v2["repl"],
            pf=cfg.v2["pf"],
        )
    row.update(metrics)
    return row


def _fmt_float(v, fmt: str) -> str:
    if isinstance(v, float):
        return format(v, fmt)
    return str(v)


def format_row(r: Dict) -> str:
    img = r.get("image", "N/A")
    name = r.get("cfg_name", "N/A")
    ver = r.get("version", "N/A")
    status = r.get("status", "N/A")
    inst_num = r.get("inst_num", "N/A")
    ipc = _fmt_float(r.get("ipc", "N/A"), ".4f")
    ic_acc = _fmt_float(r.get("icache_acc", "N/A"), ".6f")
    bpu = _fmt_float(r.get("bpu_acc", "N/A"), ".6f")
    elapsed = _fmt_float(r.get("elapsed", 0), ".2f")

    ways = r.get("ways", "-")
    mshr = r.get("mshr", "-")
    mshr_peak = r.get("mshr_peak", None)
    mshr_peak_cfg = r.get("mshr_peak_cfg", None)
    if isinstance(mshr_peak, int):
        denom = mshr_peak_cfg if isinstance(mshr_peak_cfg, int) else mshr
        mshr_peak_str = f"{mshr_peak}/{denom}"
    else:
        mshr_peak_str = "-"

    txid_peak = r.get("txid_peak", None)
    if isinstance(txid_peak, int):
        txid_peak_str = f"{txid_peak}/16"
    else:
        txid_peak_str = "-"
    repl = r.get("repl", "-")
    pf = r.get("pf", "-")

    return (
        f"{img:<15} | {name:<12} | {ver:<2} | {ways!s:<4} | {mshr!s:<4} | {mshr_peak_str:<7} | {txid_peak_str:<5} | {repl!s:<4} | {pf!s:<2} | "
        f"{inst_num!s:<10} | {ipc:<8} | {ic_acc:<10} | {bpu:<10} | {status:<10} | {elapsed:<7}"
    )


def print_progress(completed: int, total: int, length: int = 40) -> None:
    percent = completed / total if total else 1.0
    filled_length = int(length * percent)
    bar = "=" * filled_length + "-" * (length - filled_length)
    sys.stdout.write(f"\rProgress: [{bar}] {completed}/{total} ({percent:.1%})")
    sys.stdout.flush()


def build_comparison(results: List[Dict]) -> List[Dict]:
    """
    For each (image, v2_cfg), compare against v1_base on the same image.
    """
    by_key = {(r.get("image"), r.get("cfg_name")): r for r in results}
    comps: List[Dict] = []
    images_seen = sorted({r.get("image") for r in results})

    for img in images_seen:
        base = by_key.get((img, "v1_base"))
        for r in results:
            if r.get("image") != img:
                continue
            if r.get("version") != "v2":
                continue
            if base is None:
                continue

            comp = dict(image=img, cfg_name=r.get("cfg_name"), status=r.get("status"))
            base_status = base.get("status")
            comp["base_status"] = base_status

            if base_status != "pass" or r.get("status") != "pass":
                comps.append(comp)
                continue

            base_ipc = base.get("ipc")
            new_ipc = r.get("ipc")
            if isinstance(base_ipc, float) and isinstance(new_ipc, float) and base_ipc != 0.0:
                comp["ipc_v1"] = base_ipc
                comp["ipc_v2"] = new_ipc
                comp["ipc_speedup"] = new_ipc / base_ipc

            base_ic = base.get("icache_acc")
            new_ic = r.get("icache_acc")
            if isinstance(base_ic, float) and isinstance(new_ic, float):
                comp["icache_acc_v1"] = base_ic
                comp["icache_acc_v2"] = new_ic
                comp["icache_acc_delta"] = new_ic - base_ic

            base_bpu = base.get("bpu_acc")
            new_bpu = r.get("bpu_acc")
            if isinstance(base_bpu, float) and isinstance(new_bpu, float):
                comp["bpu_acc_v1"] = base_bpu
                comp["bpu_acc_v2"] = new_bpu
                comp["bpu_acc_delta"] = new_bpu - base_bpu

            base_inst = base.get("inst_num")
            new_inst = r.get("inst_num")
            if isinstance(base_inst, int) and isinstance(new_inst, int):
                comp["inst_num_v1"] = base_inst
                comp["inst_num_v2"] = new_inst

            comps.append(comp)
    return comps


def format_comp_row(c: Dict) -> str:
    img = c.get("image", "N/A")
    name = c.get("cfg_name", "N/A")
    base_status = c.get("base_status", "N/A")
    status = c.get("status", "N/A")

    ipc_v1 = c.get("ipc_v1", "N/A")
    ipc_v2 = c.get("ipc_v2", "N/A")
    speed = c.get("ipc_speedup", "N/A")

    ic_delta = c.get("icache_acc_delta", "N/A")
    bpu_delta = c.get("bpu_acc_delta", "N/A")

    ipc_v1_s = _fmt_float(ipc_v1, ".4f")
    ipc_v2_s = _fmt_float(ipc_v2, ".4f")
    speed_s = _fmt_float(speed, ".4f")
    ic_s = _fmt_float(ic_delta, ".6f")
    bpu_s = _fmt_float(bpu_delta, ".6f")

    return (
        f"{img:<15} | {name:<12} | {base_status:<10} | {status:<10} | "
        f"{ipc_v1_s:<8} | {ipc_v2_s:<8} | {speed_s:<8} | {ic_s:<10} | {bpu_s:<10}"
    )


def main():
    v2_cfgs = DEFAULT_V2_CFGS
    cfgs = make_build_cfgs(v2_cfgs)
    binaries = compile_binaries(cfgs)
    atexit.register(cleanup_binaries, binaries)

    tasks = []
    for image in images:
        for binary, cfg in binaries:
            tasks.append((binary, image, cfg))

    total = len(tasks)
    print(f"\nRunning {total} simulations in parallel...")
    pool = multiprocessing.Pool(processes=min(total, multiprocessing.cpu_count()))

    header = (
        f"{'Image':<15} | {'Config':<12} | {'V':<2} | {'Ways':<4} | {'MSHR':<4} | {'MSHRpk':<7} | {'TXpk':<5} | {'Repl':<4} | {'PF':<2} | "
        f"{'Inst Num':<10} | {'IPC':<8} | {'ICacheAcc':<10} | {'BPUAcc':<10} | {'Status':<10} | {'Time(s)':<7}"
    )
    print("\n" + "=" * len(header))
    print(header)
    print("-" * len(header))

    results: List[Dict] = []
    print_progress(0, total)
    for r in pool.imap_unordered(run_simulation, tasks):
        results.append(r)
        sys.stdout.write("\r" + " " * 120 + "\r")
        print(format_row(r))
        print_progress(len(results), total)

    pool.close()
    pool.join()

    # Sorted summary
    def _sort_key(r: Dict) -> Tuple:
        img = r.get("image", "")
        cfg_name = r.get("cfg_name", "")
        return (img, cfg_name)

    print("\n\nSorted Results")
    print("=" * len(header))
    print(header)
    print("-" * len(header))
    for r in sorted(results, key=_sort_key):
        print(format_row(r))

    # V1 vs V2 comparison
    comps = build_comparison(results)
    comp_header = (
        f"{'Image':<15} | {'V2 Config':<12} | {'V1 Status':<10} | {'V2 Status':<10} | "
        f"{'IPC_v1':<8} | {'IPC_v2':<8} | {'Speedup':<8} | {'IAccΔ':<10} | {'BPUΔ':<10}"
    )
    print("\n\nV1 vs V2 Comparison (per image)")
    print("=" * len(comp_header))
    print(comp_header)
    print("-" * len(comp_header))
    for c in sorted(comps, key=lambda x: (x.get("image", ""), x.get("cfg_name", ""))):
        print(format_comp_row(c))

    # Optional artifact dumps for post-processing
    out_json = os.environ.get("OUT_JSON", "").strip()
    if out_json:
        with open(out_json, "w", encoding="utf-8") as f:
            json.dump({"results": results, "comparison": comps}, f, indent=2, sort_keys=True)
        print(f"\nWrote {out_json}")

    out_csv = os.environ.get("OUT_CSV", "").strip()
    if out_csv:
        fields = sorted({k for r in results for k in r.keys()})
        with open(out_csv, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=fields)
            w.writeheader()
            for r in sorted(results, key=lambda x: (x.get("image", ""), x.get("cfg_name", ""))):
                w.writerow(r)
        print(f"Wrote {out_csv}")


if __name__ == "__main__":
    main()
