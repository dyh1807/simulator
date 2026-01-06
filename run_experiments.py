import os
import re
import subprocess
import sys

config_path = "back-end/include/config.h"
latencies = [1, 20, 50, 100]
images = [
    "./baremetal/linux.bin",
    "./baremetal/new_coremark/coremark.bin",
    "./baremetal/new_dhrystone/dhrystone.bin"
]

def set_latency(latency):
    with open(config_path, "r") as f:
        content = f.read()
    
    new_content = re.sub(r"#define ICACHE_MISS_LATENCY \d+", f"#define ICACHE_MISS_LATENCY {latency}", content)
    
    with open(config_path, "w") as f:
        f.write(new_content)

def run_sim(image_path):
    # Compile
    subprocess.run(["make", "-j"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    
    # Run
    try:
        result = subprocess.run(["make", "run", f"IMG={image_path}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=300)
        # Note: The Makefile might not use the IMG variable directly in the 'run' target as expected by this script change.
        # Let's check how 'make run' works. If it hardcodes the image, we might need to invoke ./a.out directly.
        # Based on previous context, 'make run' runs './a.out ./baremetal/memory'.
        # So we should probably run the executable directly to support different images easily if the Makefile isn't flexible.
        # Let's try running ./a.out directly.
        cmd = ["./a.out", image_path]
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=300)
        output = result.stdout
    except subprocess.TimeoutExpired:
        print(f"Simulation timed out for {image_path}!")
        return None
    except Exception as e:
        print(f"Simulation failed for {image_path}: {e}")
        return None

    # Parse output
    metrics = {}
    
    cycle_match = re.search(r"cycle\s+num:\s+(\d+)", output)
    if cycle_match:
        metrics["cycles"] = int(cycle_match.group(1))
    
    ipc_match = re.search(r"ipc\s+:\s+([0-9.]+)", output)
    if ipc_match:
        metrics["ipc"] = float(ipc_match.group(1))
        
    access_match = re.search(r"icache access\s+:\s+(\d+)", output)
    if access_match:
        metrics["access"] = int(access_match.group(1))
        
    miss_match = re.search(r"icache miss\s+:\s+(\d+)", output)
    if miss_match:
        metrics["miss"] = int(miss_match.group(1))

    # Parse icache accuracy
    # icache accuracy : 0.934462
    icache_acc_match = re.search(r"icache accuracy : ([0-9.]+)", output)
    if icache_acc_match:
        metrics["icache_acc"] = float(icache_acc_match.group(1))
    
    # Parse bpu accuracy
    # bpu   accuracy : 0.980342
    bpu_acc_match = re.search(r"bpu\s+accuracy : ([0-9.]+)", output)
    if bpu_acc_match:
        metrics["bpu_acc"] = float(bpu_acc_match.group(1))
        
    return metrics

print(f"{'Image':<40} | {'Latency':<10} | {'Cycles':<12} | {'IPC':<10} | {'Access':<12} | {'Miss':<10} | {'ICache Acc':<10} | {'BPU Acc':<10}")
print("-" * 140)

for image in images:
    for lat in latencies:
        set_latency(lat)
        metrics = run_sim(image)
        if metrics:
            print(f"{image:<40} | {lat:<10} | {metrics.get('cycles', 'N/A'):<12} | {metrics.get('ipc', 'N/A'):<10.6f} | {metrics.get('access', 'N/A'):<12} | {metrics.get('miss', 'N/A'):<10} | {metrics.get('icache_acc', 'N/A'):<10.6f} | {metrics.get('bpu_acc', 'N/A'):<10.6f}")
        else:
            print(f"{image:<40} | {lat:<10} | {'Failed':<12} | {'N/A':<10} | {'N/A':<12} | {'N/A':<10} | {'N/A':<10} | {'N/A':<10}")

# Reset to default
set_latency(100)
