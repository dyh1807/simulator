import os
import re
import subprocess
import time
import multiprocessing
import sys

latencies = [0, 1, 20, 50, 100]
# Add your configurable images here
images = [
    "./baremetal/linux.bin",
    "./baremetal/new_coremark/coremark.bin",
    "./baremetal/new_dhrystone/dhrystone.bin"
]
target_inst = 150000000
binaries = []

def compile_binaries():
    print("Compiling binaries...")
    for lat in latencies:
        # Clean build to ensure flags are applied
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # Original flags from Makefile: -O3 -march=native -funroll-loops -mtune=native --std=c++2a
        cxx_flags = f"-O3 -march=native -funroll-loops -mtune=native --std=c++2a -DICACHE_MISS_LATENCY={lat} -DMAX_COMMIT_INST={target_inst}"
        
        subprocess.run(["make", "-j", f"CXXFLAGS={cxx_flags}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        
        binary_name = f"sim_lat_{lat}"
        subprocess.run(["mv", "a.out", binary_name], check=True)
        binaries.append(binary_name)
        print(f"Compiled {binary_name}")

def run_simulation(task):
    binary, image = task
    latency = int(binary.split("_")[-1])
    img_name = os.path.basename(image)
    
    try:
        # Check if binary and image exist
        if not os.path.exists(binary):
            return {"latency": latency, "image": img_name, "error": f"Binary {binary} not found"}
        if not os.path.exists(image):
            return {"latency": latency, "image": img_name, "error": f"Image {image} not found"}

        # Run command: ./binary ./image
        start_time = time.time()
        result = subprocess.run([f"./{binary}", image], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5000)
        end_time = time.time()
        
        if result.returncode != 0 and result.returncode != 1:
             return {"latency": latency, "image": img_name, "error": f"RC {result.returncode}, Stderr: {result.stderr[:200]}"}

        output = result.stdout
        
        metrics = {}
        metrics["latency"] = latency
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
        return {"latency": latency, "image": img_name, "error": str(e)}

def format_row(r):
    img_name = r.get('image', 'N/A')
    latency = r.get('latency', 'N/A')
    if "error" in r:
        return f"{img_name:<15} | {latency:<5} | Error: {r['error']}"
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
        
        return f"{img_name:<15} | {latency:<5} | {inst_num:<10} | {cycles:<10} | {ipc_str:<8} | {access:<10} | {miss:<8} | {icache_acc_str:<10} | {bpu_acc_str:<8} | {elapsed_str:<7}"

def print_progress(completed, total, length=40):
    percent = completed / total
    filled_length = int(length * percent)
    bar = '=' * filled_length + '-' * (length - filled_length)
    sys.stdout.write(f'\rProgress: [{bar}] {completed}/{total} ({percent:.1%})')
    sys.stdout.flush()

def main():
    compile_binaries()
    
    tasks = []
    for binary in binaries:
        for image in images:
            tasks.append((binary, image))
            
    total_tasks = len(tasks)
    print(f"\nRunning {total_tasks} simulations in parallel...")
    
    pool = multiprocessing.Pool(processes=min(total_tasks, multiprocessing.cpu_count()))
    results = []
    
    # Print header first
    header = f"{'Image':<15} | {'Lat':<5} | {'Inst Num':<10} | {'Cycles':<10} | {'IPC':<8} | {'Access':<10} | {'Miss':<8} | {'ICache Acc':<10} | {'BPU Acc':<8} | {'Time(s)':<7}"
    print("\n" + "="*len(header))
    print(header)
    print("-" * len(header))
    
    print_progress(0, total_tasks)
    
    for result in pool.imap_unordered(run_simulation, tasks):
        results.append(result)
        
        # Clear progress bar line
        sys.stdout.write('\r' + ' ' * 80 + '\r')
        
        # Print result row
        print(format_row(result))
        
        # Reprint progress bar
        print_progress(len(results), total_tasks)
    
    pool.close()
    pool.join()
    print() # Newline after progress bar
    print("="*len(header))
    
    # Optional: Reprint sorted table summary
    print("\nSorted Summary:")
    results.sort(key=lambda x: (x.get("image", ""), x.get("latency", 0)))
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
