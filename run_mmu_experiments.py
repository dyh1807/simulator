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
# Latencies to test for PTW memory access
latencies = [0, 20, 50, 100]

# Configurable images (only using linux.bin as requested)
images = [
    "./baremetal/linux.bin",
]

# Target instructions (using small number for initial test, change to 150000000 later)
# target_inst = 500000
target_inst = 150000000

# Generate configurations
# Format: (latency)
configs = []
for lat in latencies:
    configs.append(lat)

binaries = []


def get_binary_name(lat):
    return f"sim_ptw_lat_{lat}"

def compile_binaries():
    print("Compiling binaries...")
    for lat in configs:
        # Clean build to ensure flags are applied
        subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # Original flags from Makefile: -O3 -march=native -funroll-loops -mtune=native --std=c++2a
        cxx_flags = f"-O3 -march=native -funroll-loops -mtune=native --std=c++2a -DMAX_COMMIT_INST={target_inst}"
        cxx_flags += f" -DPTW_MEM_LATENCY={lat}"
            
        try:
            subprocess.run(["make", "-j", f"CXXFLAGS={cxx_flags}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
        except subprocess.CalledProcessError as e:
            print(f"Compilation failed for config (latency={lat})")
            print(e.stderr)
            sys.exit(1)
        
        binary_name = get_binary_name(lat)
        subprocess.run(["mv", "a.out", binary_name], check=True)
        binaries.append(binary_name)
        print(f"Compiled {binary_name}")

def run_simulation(task):
    binary, image, lat = task
    img_name = os.path.basename(image)
    
    try:
        # Check if binary and image exist
        if not os.path.exists(binary):
            return {"config": lat, "image": img_name, "error": f"Binary {binary} not found"}
        if not os.path.exists(image):
            return {"config": lat, "image": img_name, "error": f"Image {image} not found"}

        # Run command: ./binary ./image
        start_time = time.time()
        # Using a timeout slightly longer than typical execution time (adjusted for full run)
        # Assuming full run takes significantly longer than 500k insts
        result = subprocess.run([f"./{binary}", image], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10000)
        end_time = time.time()
        
        # Check for success or specific known return codes
        if result.returncode != 0:
             return {"config": lat, "image": img_name, "error": f"RC {result.returncode}, Stderr: {result.stderr[:200]}"}

        output = result.stdout
        
        metrics = {}
        metrics["config"] = lat
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
            
        # Parse TLB stats
        # printf("\033[1;32mitlb access    : %ld\033[0m\n", itlb_access);
        # printf("\033[1;32mitlb hit       : %ld\033[0m\n", itlb_hit);
        
        itlb_acc_num_match = re.search(r"itlb access\s+:\s+(\d+)", output)
        if itlb_acc_num_match:
            metrics["itlb_access"] = int(itlb_acc_num_match.group(1))
            
        itlb_hit_num_match = re.search(r"itlb hit\s+:\s+(\d+)", output)
        if itlb_hit_num_match:
            metrics["itlb_hit"] = int(itlb_hit_num_match.group(1))
            if "itlb_access" in metrics:
                metrics["itlb_miss"] = metrics["itlb_access"] - metrics["itlb_hit"]

        itlb_hit_rate_match = re.search(r"itlb hit rate\s+:\s+([0-9.]+)", output)
        if itlb_hit_rate_match:
            metrics["itlb_hit_rate"] = float(itlb_hit_rate_match.group(1))

        dtlb_acc_num_match = re.search(r"dtlb access\s+:\s+(\d+)", output)
        if dtlb_acc_num_match:
            metrics["dtlb_access"] = int(dtlb_acc_num_match.group(1))
            
        dtlb_hit_num_match = re.search(r"dtlb hit\s+:\s+(\d+)", output)
        if dtlb_hit_num_match:
            metrics["dtlb_hit"] = int(dtlb_hit_num_match.group(1))
            if "dtlb_access" in metrics:
                metrics["dtlb_miss"] = metrics["dtlb_access"] - metrics["dtlb_hit"]
            
        dtlb_hit_rate_match = re.search(r"dtlb hit rate\s+:\s+([0-9.]+)", output)
        if dtlb_hit_rate_match:
            metrics["dtlb_hit_rate"] = float(dtlb_hit_rate_match.group(1))
            
        return metrics
    except Exception as e:
        return {"config": lat, "image": img_name, "error": str(e)}

def format_row(r):
    img_name = r.get('image', 'N/A')
    lat = r.get('config', 0)
    
    if "error" in r:
        return f"{img_name:<20} | {lat:<5} | Error: {r['error']}"
    else:
        inst_num = r.get('inst_num', 'N/A')
        cycles = r.get('cycles', 'N/A')
        ipc = r.get('ipc', 'N/A')
        
        itlb_acc = r.get('itlb_access', 'N/A')
        itlb_miss = r.get('itlb_miss', 'N/A')
        itlb_hit_rate = r.get('itlb_hit_rate', 'N/A')
        
        dtlb_acc = r.get('dtlb_access', 'N/A')
        dtlb_miss = r.get('dtlb_miss', 'N/A')
        dtlb_hit_rate = r.get('dtlb_hit_rate', 'N/A')
        
        elapsed = r.get('elapsed', 0)
        
        ipc_str = f"{ipc:.4f}" if isinstance(ipc, float) else str(ipc)
        itlb_hr_str = f"{itlb_hit_rate:.6f}" if isinstance(itlb_hit_rate, float) else str(itlb_hit_rate)
        dtlb_hr_str = f"{dtlb_hit_rate:.6f}" if isinstance(dtlb_hit_rate, float) else str(dtlb_hit_rate)
        elapsed_str = f"{elapsed:.2f}"
        
        return f"{img_name:<20} | {lat:<5} | {inst_num:<10} | {cycles:<10} | {ipc_str:<8} | {itlb_acc:<10} | {itlb_miss:<10} | {itlb_hr_str:<10} | {dtlb_acc:<10} | {dtlb_miss:<10} | {dtlb_hr_str:<10} | {elapsed_str:<7}"

def print_progress(completed, total, length=40):
    if total == 0: return
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
    binary_map = {get_binary_name(lat): lat for lat in configs}
    
    for binary in binaries:
        lat = binary_map[binary]
        for image in images:
            tasks.append((binary, image, lat))
            
    total_tasks = len(tasks)
    print(f"\nRunning {total_tasks} simulations in parallel...")
    
    pool = multiprocessing.Pool(processes=min(total_tasks, multiprocessing.cpu_count()))
    results = []
    
    # Print header first
    header = f"{'Image':<20} | {'Lat':<5} | {'Inst Num':<10} | {'Cycles':<10} | {'IPC':<8} | {'ITLB Acc':<10} | {'ITLB Miss':<10} | {'ITLB Rate':<10} | {'DTLB Acc':<10} | {'DTLB Miss':<10} | {'DTLB Rate':<10} | {'Time(s)':<7}"
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
    # Sort order: Image (index 0) -> Latency (config)
    results.sort(key=lambda x:
        (x.get("image", ""), 
        x.get("config", 0))  # latency
    )
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
