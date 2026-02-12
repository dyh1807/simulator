import os
import re
import subprocess
import time
import multiprocessing
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(THIS_DIR, "tools"))

from regression_gate import maybe_run_determinism_check

# Configurations
latencies = [0, 20, 50, 100]
line_sizes = [32, 64]
# Add your configurable images here
images = [
    "./baremetal/linux.bin",
    "./baremetal/new_coremark/coremark.bin",
    "./baremetal/new_dhrystone/dhrystone.bin"
]
# Instruction target for experiment iterations.
# Default matches the original setting; for quick local runs, override via env:
#   TARGET_INST=1500000 python3 run_experiments_parallel.py
target_inst = int(os.environ.get("TARGET_INST", "150000000"))

# Generate configurations
# Format: (latency, line_size, use_true_icache)
# Note: Latency 0 represents ideal icache (no USE_TRUE_ICACHE), where latency is irrelevant
configs = []

# Ideal ICache (Simple Model) - Latency irrelevant, but we track line size
for size in line_sizes:
    configs.append((0, size, False))

# True ICache - Latency and line size relevant
for lat in latencies:
    for size in line_sizes:
        configs.append((lat, size, True))

binaries = []


def get_binary_name(lat, size, use_true):
    type_str = "true" if use_true else "ideal"
    return f"sim_{type_str}_lat_{lat}_ls_{size}"

def compile_binaries():
    print("Compiling binaries...")
    for lat, size, use_true in configs:
        # Clean build to ensure flags are applied
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # Original flags from Makefile: -O3 -march=native -funroll-loops -mtune=native --std=c++2a
        cxx_flags = f"-O3 -march=native -funroll-loops -mtune=native --std=c++2a -DMAX_COMMIT_INST={target_inst}"
        cxx_flags += f" -DICACHE_LINE_SIZE={size}"
        
        if use_true:
            cxx_flags += f" -DICACHE_MISS_LATENCY={lat}"
        else:
            # Disable default true icache
            cxx_flags += f" -DUSE_IDEAL_ICACHE"
            
        try:
            subprocess.run(["make", "-j", f"CXXFLAGS={cxx_flags}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
        except subprocess.CalledProcessError as e:
            print(f"Compilation failed for config ({lat}, {size}, {use_true})")
            print(e.stderr)
            sys.exit(1)
        
        binary_name = get_binary_name(lat, size, use_true)
        subprocess.run(["mv", "a.out", binary_name], check=True)
        binaries.append(binary_name)
        print(f"Compiled {binary_name}")

def run_simulation(task):
    binary, image, config = task
    lat, size, use_true = config
    img_name = os.path.basename(image)
    
    try:
        # Check if binary and image exist
        if not os.path.exists(binary):
            return {"config": config, "image": img_name, "error": f"Binary {binary} not found"}
        if not os.path.exists(image):
            return {"config": config, "image": img_name, "error": f"Image {image} not found"}

        # Run command: ./binary ./image
        start_time = time.time()
        result = subprocess.run([f"./{binary}", image], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5000)
        end_time = time.time()
        
        if result.returncode != 0 and result.returncode != 1:
             return {"config": config, "image": img_name, "error": f"RC {result.returncode}, Stderr: {result.stderr[:200]}"}

        output = result.stdout
        
        metrics = {}
        metrics["config"] = config
        metrics["image"] = img_name
        metrics["elapsed"] = end_time - start_time
        
        inst_match = re.search(r"instruction num: (\d+)", output)
        if inst_match:
            metrics["inst_num"] = int(inst_match.group(1))
            
        cycle_match = re.search(r"cycle\s+num:\s+(\d+)", output)
        if cycle_match:
            metrics["cycles"] = int(cycle_match.group(1))
        
        ipc_match = re.search(r"ipc\s+ :\s+([0-9.]+)", output)
        if ipc_match:
            metrics["ipc"] = float(ipc_match.group(1))
            
        access_match = re.search(r"icache access\s+ :\s+(\d+)", output)
        if access_match:
            metrics["access"] = int(access_match.group(1))
            
        miss_match = re.search(r"icache miss\s+ :\s+(\d+)", output)
        if miss_match:
            metrics["miss"] = int(miss_match.group(1))
    
        icache_acc_match = re.search(r"icache accuracy : ([0-9.]+)", output)
        if icache_acc_match:
            metrics["icache_acc"] = float(icache_acc_match.group(1))
        
        bpu_acc_match = re.search(r"bpu\s+accuracy : ([0-9.]+)", output)
        if bpu_acc_match:
            metrics["bpu_acc"] = float(bpu_acc_match.group(1))
            
        return metrics
    except Exception as e:
        return {"config": config, "image": img_name, "error": str(e)}

def format_row(r):
    img_name = r.get('image', 'N/A')
    lat, size, use_true = r.get('config', (0, 0, False))
    type_str = "True" if use_true else "Ideal"
    
    if "error" in r:
        return f"{img_name:<15} | {type_str:<6} | {size:<4} | {lat:<5} | Error: {r['error']}"
    else:
        inst_num = r.get('inst_num', 'N/A')
        cycles = r.get('cycles', 'N/A')
        ipc = r.get('ipc', 'N/A')
        access = r.get('access', 'N/A')
        miss = r.get('miss', 'N/A')
        icache_acc = r.get('icache_acc', 'N/A')
        bpu_acc = r.get('bpu_acc', 'N/A')
        elapsed = r.get('elapsed', 0)
        
        ipc_str = f"{ipc:.4f}" if isinstance(ipc, float) else str(ipc)
        icache_acc_str = f"{icache_acc:.6f}" if isinstance(icache_acc, float) else str(icache_acc)
        bpu_acc_str = f"{bpu_acc:.6f}" if isinstance(bpu_acc, float) else str(bpu_acc)
        elapsed_str = f"{elapsed:.2f}"
        
        return f"{img_name:<15} | {type_str:<6} | {size:<4} | {lat:<5} | {inst_num:<10} | {cycles:<10} | {ipc_str:<8} | {access:<10} | {miss:<8} | {icache_acc_str:<10} | {bpu_acc_str:<8} | {elapsed_str:<7}"

def print_progress(completed, total, length=40):
    percent = completed / total
    filled_length = int(length * percent)
    bar = '=' * filled_length + '-' * (length - filled_length)
    sys.stdout.write(f'\rProgress: [{bar}] {completed}/{total} ({percent:.1%})')
    sys.stdout.flush()

def main():
    maybe_run_determinism_check(repo_root=THIS_DIR)

    compile_binaries()
    
    tasks = []
    # Map binary to config
    binary_map = {get_binary_name(lat, size, use_true): (lat, size, use_true) for lat, size, use_true in configs}
    
    for binary in binaries:
        config = binary_map[binary]
        for image in images:
            tasks.append((binary, image, config))
            
    total_tasks = len(tasks)
    print(f"\nRunning {total_tasks} simulations in parallel...")
    
    pool = multiprocessing.Pool(processes=min(total_tasks, multiprocessing.cpu_count()))
    results = []
    
    # Print header first
    header = f"{'Image':<15} | {'Type':<6} | {'Size':<4} | {'Lat':<5} | {'Inst Num':<10} | {'Cycles':<10} | {'IPC':<8} | {'Access':<10} | {'Miss':<8} | {'ICache Acc':<10} | {'BPU Acc':<8} | {'Time(s)':<7}"
    print("\n" + "="*len(header))
    print(header)
    print("-" * len(header))
    
    print_progress(0, total_tasks)
    
    for result in pool.imap_unordered(run_simulation, tasks):
        results.append(result)
        
        # Clear progress bar line
        sys.stdout.write('\r' + ' ' * 100 + '\r')
        
        # Print result row
        print(format_row(result))
        
        # Reprint progress bar
        print_progress(len(results), total_tasks)
    
    pool.close()
    pool.join()
    print() # Newline after progress bar
    print("="*len(header))
    
    # Reprint sorted table summary
    print("\nSorted Summary:")
    # Sort order: Image (index 0) -> Type (config[2]) -> Size (config[1]) -> Latency (config[0])
    results.sort(key=lambda x: (
        x.get("image", ""), 
        x.get("config", (0,0,False))[2], # use_true (False/Ideal first)
        x.get("config", (0,0,False))[1], # line_size
        x.get("config", (0,0,False))[0]  # latency
    ))
    print("="*len(header))
    print(header)
    print("-" * len(header))
    for r in results:
        print(format_row(r))
    print("="*len(header))

    # Cleanup
    for b in binaries:
        if os.path.exists(b):
            os.remove(b)

if __name__ == "__main__":
    main()
