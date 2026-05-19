import json
import matplotlib.pyplot as plt
import sys
import os
import pandas as pd

def main():
    if len(sys.argv) < 2:
        print("Usage: python plot.py <results.json>")
        sys.exit(1)
        
    with open(sys.argv[1], 'r') as f:
        data = json.load(f)

    benchmarks = data.get("benchmarks", [])
    
    # Process into pandas DF
    records = []
    for b in benchmarks:
        name = b["name"].replace("/real_time", "")
        # Parsing topologies
        parts = name.split("/")
        qtype = parts[0]
        threads = int(parts[1]) if len(parts) > 1 else (2 if qtype.startswith("BM_SPSC") else 1)
        
        ops_sec = b.get("ops/sec", b.get("items_per_second", 0))
        records.append({
            "Queue": qtype,
            "Threads": threads,
            "Ops_Sec": ops_sec
        })

    df = pd.DataFrame(records)
    
    os.makedirs('results', exist_ok=True)
    
    # Export to CSV
    csv_file = os.path.join('results', 'results_all.csv')
    df.to_csv(csv_file, index=False)
    print(f"Exported metrics to {csv_file}")

    # Plot
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for qtype, group in df.groupby("Queue"):
        group = group.sort_values(by="Threads")
        ax.plot(group["Threads"], group["Ops_Sec"] / 1_000_000, marker='o', label=qtype)

    ax.set_title("Concurrent Queue Throughput Comparison")
    ax.set_xlabel("Total Threads (Producers + Consumers)")
    ax.set_ylabel("Throughput (Millions Ops / Sec)")
    ax.grid(True)
    ax.legend(title="Queue Type")

    os.makedirs('results', exist_ok=True)
    plot_file = os.path.join('results', 'throughput.png')
    plt.savefig(plot_file, dpi=300)
    print(f"Plot saved to {plot_file}")

if __name__ == '__main__':
    main()