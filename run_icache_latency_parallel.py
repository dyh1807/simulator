import os
import atexit
import re
import subprocess
import time
import multiprocessing
import sys

# Images to test
images = [
    "./baremetal/new_dhrystone/dhrystone.bin",
    "./baremetal/new_coremark/coremark.bin",
    "./baremetal/linux.bin",
]

# Target instruction count (MAX_COMMIT_INST)
target_inst = int(os.environ.get("TARGET_INST", "150000000"))

# Timeouts (seconds)
coremark_timeout = int(os.environ.get("COREMARK_TIMEOUT", "3000"))
dhrystone_timeout = int(os.environ.get("DHRYSTONE_TIMEOUT", "3000"))
linux_timeout = int(os.environ.get("LINUX_TIMEOUT", "3000"))

# SRAM latency configurations
fixed_latencies = [0, 1, 2, 4]
random_ranges = [(1, 4)]


def make_cfgs():
    cfgs = []
    cfgs.append(
        {
            "name": "ideal",
            "ideal": True,
        }
    )
    for lat in fixed_latencies:
        cfgs.append(
            {
                "name": f"fixed_{lat}",
                "random": False,
                "fixed": lat,
            }
        )
    for (min_lat, max_lat) in random_ranges:
        cfgs.append(
            {
                "name": f"rand_{min_lat}_{max_lat}",
                "random": True,
                "min": min_lat,
                "max": max_lat,
            }
        )
    return cfgs


def _cfg_sort_key(cfg):
    # ideal first
    if cfg.get("ideal"):
        return (0, 0, 0, 0)
    name = cfg.get("name", "")
    if name.startswith("fixed_"):
        try:
            lat = int(name.split("_")[1])
        except (IndexError, ValueError):
            lat = 9999
        return (1, lat, 0, 0)
    if name.startswith("rand_"):
        try:
            parts = name.split("_")
            min_lat = int(parts[1])
            max_lat = int(parts[2])
        except (IndexError, ValueError):
            min_lat, max_lat = 9999, 9999
        return (2, min_lat, max_lat, 0)
    return (3, 9999, 9999, 0)


def get_timeout(image_path: str) -> int:
    base = os.path.basename(image_path)
    if "coremark" in base:
        return coremark_timeout
    if "dhrystone" in base:
        return dhrystone_timeout
    return linux_timeout


def get_binary_name(cfg):
    return f"sim_icache_{cfg['name']}"


def compile_binaries(cfgs):
    print("Compiling binaries...")
    binaries = []
    base_flags = "-O3 -march=native -funroll-loops -mtune=native --std=c++2a"
    for cfg in cfgs:
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        cxx_flags = f"{base_flags} -DMAX_COMMIT_INST={target_inst}"
        if cfg.get("ideal"):
            # Ideal icache: disable true icache model (no miss) and keep memory path unchanged.
            # Note: default build enables USE_TRUE_ICACHE unless USE_IDEAL_ICACHE is defined.
            cxx_flags += " -DUSE_IDEAL_ICACHE"
        else:
            cxx_flags += " -DUSE_SIM_DDR -DICACHE_USE_SRAM_MODEL=1"
            if cfg["random"]:
                cxx_flags += " -DICACHE_SRAM_RANDOM_DELAY=1"
                cxx_flags += f" -DICACHE_SRAM_RANDOM_MIN={cfg['min']}"
                cxx_flags += f" -DICACHE_SRAM_RANDOM_MAX={cfg['max']}"
            else:
                cxx_flags += " -DICACHE_SRAM_RANDOM_DELAY=0"
                cxx_flags += f" -DICACHE_SRAM_FIXED_LATENCY={cfg['fixed']}"

        try:
            subprocess.run(
                ["make", "-j", f"CXXFLAGS={cxx_flags}"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
                text=True,
            )
        except subprocess.CalledProcessError as e:
            print(f"Compilation failed for config {cfg['name']}")
            print(e.stderr)
            sys.exit(1)

        binary_name = get_binary_name(cfg)
        subprocess.run(["mv", "a.out", binary_name], check=True)
        binaries.append((binary_name, cfg))
        print(f"Compiled {binary_name}")
    return binaries


def cleanup_binaries(binaries):
    for binary, _cfg in binaries:
        try:
            if os.path.exists(binary):
                os.remove(binary)
        except OSError:
            pass
    # Remove any leftover default binary
    try:
        if os.path.exists("a.out"):
            os.remove("a.out")
    except OSError:
        pass


def parse_metrics(output: str):
    metrics = {}
    inst_match = re.search(r"instruction num: (\d+)", output)
    if inst_match:
        metrics["inst_num"] = int(inst_match.group(1))
    cycle_match = re.search(r"cycle\s+num:\s+(\d+)", output)
    if cycle_match:
        metrics["cycles"] = int(cycle_match.group(1))
    ipc_match = re.search(r"ipc\s+ :\s+([0-9.]+)", output)
    if ipc_match:
        metrics["ipc"] = float(ipc_match.group(1))
    bpu_acc_match = re.search(r"bpu\s+accuracy : ([0-9.]+)", output)
    if bpu_acc_match:
        metrics["bpu_acc"] = float(bpu_acc_match.group(1))
    access_match = re.search(r"icache access\s+ :\s+(\d+)", output)
    if access_match:
        metrics["icache_access"] = int(access_match.group(1))
    miss_match = re.search(r"icache miss\s+ :\s+(\d+)", output)
    if miss_match:
        metrics["icache_miss"] = int(miss_match.group(1))
    return metrics


def classify_status(output: str, rc):
    if re.search(r"difftest|Difftest", output):
        return "difftest"
    if re.search(r"TIME OUT|timeout|Timeout", output):
        return "timeout"
    if re.search(r"ERROR|Error", output):
        return "error"
    if "Success!!!!" in output:
        return "pass"
    if rc in (0, 1):
        return "pass"
    return f"rc{rc}"


def run_simulation(task):
    binary, image, cfg = task
    img_name = os.path.basename(image)
    timeout_s = get_timeout(image)

    start_time = time.time()
    try:
        result = subprocess.run(
            [f"./{binary}", image],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_s,
        )
        end_time = time.time()
        output = result.stdout + "\n" + result.stderr
        status = classify_status(output, result.returncode)
    except subprocess.TimeoutExpired as e:
        end_time = time.time()
        output = (e.stdout or "") + "\n" + (e.stderr or "")
        status = "timeout"

    metrics = parse_metrics(output)
    metrics["config"] = cfg
    metrics["image"] = img_name
    metrics["status"] = status
    metrics["elapsed"] = end_time - start_time
    return metrics


def format_row(r):
    img_name = r.get("image", "N/A")
    cfg = r.get("config", {})
    name = cfg.get("name", "N/A")
    status = r.get("status", "N/A")
    ipc = r.get("ipc", "N/A")
    bpu_acc = r.get("bpu_acc", "N/A")
    access = r.get("icache_access", "N/A")
    miss = r.get("icache_miss", "N/A")
    inst_num = r.get("inst_num", "N/A")
    elapsed = r.get("elapsed", 0)

    ipc_str = f"{ipc:.4f}" if isinstance(ipc, float) else str(ipc)
    bpu_str = f"{bpu_acc:.6f}" if isinstance(bpu_acc, float) else str(bpu_acc)
    elapsed_str = f"{elapsed:.2f}"

    return f"{img_name:<15} | {name:<10} | {inst_num:<10} | {ipc_str:<8} | {bpu_str:<9} | {access:<10} | {miss:<8} | {status:<8} | {elapsed_str:<7}"


def print_progress(completed, total, length=40):
    percent = completed / total
    filled_length = int(length * percent)
    bar = "=" * filled_length + "-" * (length - filled_length)
    sys.stdout.write(f"\rProgress: [{bar}] {completed}/{total} ({percent:.1%})")
    sys.stdout.flush()

def _result_sort_key(r):
    img_name = r.get("image", "")
    img_order = image_order.get(img_name, 9999)
    cfg = r.get("config", {})
    return (img_order, _cfg_sort_key(cfg), img_name, cfg.get("name", ""))

def main():
    cfgs = make_cfgs()
    cfgs = sorted(cfgs, key=_cfg_sort_key)
    binaries = compile_binaries(cfgs)
    atexit.register(cleanup_binaries, binaries)

    try:
        tasks = []
        for image in images:
            for binary, cfg in binaries:
                tasks.append((binary, image, cfg))

        total_tasks = len(tasks)
        print(f"\nRunning {total_tasks} simulations in parallel...")
        pool = multiprocessing.Pool(processes=min(total_tasks, multiprocessing.cpu_count()))
        results = []

        header = f"{'Image':<15} | {'Config':<10} | {'Inst Num':<10} | {'IPC':<8} | {'BPU Acc':<9} | {'Access':<10} | {'Miss':<8} | {'Status':<8} | {'Time(s)':<7}"
        print("\n" + "=" * len(header))
        print(header)
        print("-" * len(header))

        print_progress(0, total_tasks)

        for result in pool.imap_unordered(run_simulation, tasks):
            results.append(result)
            sys.stdout.write("\r" + " " * 100 + "\r")
            print(format_row(result))
            print_progress(len(results), total_tasks)

        pool.close()
        pool.join()

        # Final sorted summary
        results_sorted = sorted(results, key=_result_sort_key)
        print("\n\nSorted Results")
        print("=" * len(header))
        print(header)
        print("-" * len(header))
        for r in results_sorted:
            print(format_row(r))
    finally:
        cleanup_binaries(binaries)


if __name__ == "__main__":
    image_order = {os.path.basename(p): i for i, p in enumerate(images)}
    main()
