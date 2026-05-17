# ThreadSafeQueueLib Benchmarks

This directory contains performance benchmarks for the concurrent queues defined in `ThreadSafeQueueLib`, implemented using the [Google Benchmark](https://github.com/google/benchmark) library.

## Benchmarked Queues

The benchmark suite currently aggregates the completed queue implementations into a single benchmark executable:

- **SPSC (Single Producer, Single Consumer):**
  - Lock-free SPSC unbounded queue (`lockfree_spsc_unbounded`)
  - Lock-free SPSC bounded queue (`lockfree_spsc_bounded`)
- **MPSC (Multiple Producers, Single Consumer):**
  - Lock-free MPSC unbounded queue (`lockfree_mpsc_unbounded`)
- **Blocking Queues:**
  - Blocking MPMC unbounded queue (`blocking_mpmc_unbounded`)

## Prerequisites

- **CMake** (v3.16 or higher)
- A **C++20** compatible compiler (GCC, Clang, MSVC)
- Git (for fetching Google Benchmark via CMake `FetchContent`)

## Build Instructions

To build the benchmarks, you can configure and build the CMake project within the `benchmarking` directory:

```bash
cd benchmarking
mkdir build && cd build

# Configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the benchmark executable
cmake --build . --config Release
```

*Note: It is highly recommended to build with `Release` optimizations to get accurate performance measurements.*

## Running the Benchmarks

Once built, the benchmark executable `queue_benchmarks` will be available in the build directory.

Run all benchmarks:

```bash
./queue_benchmarks
```

### Useful Google Benchmark Flags

Google Benchmark provides several flags to customize the execution:

- **Filter benchmarks:** Run only a specific subset of benchmarks using regex.
  ```bash
  ./queue_benchmarks --benchmark_filter=BM_SPSC
  ```
- **Output to file:** Save the results to a file (e.g., in JSON format) which can be useful for plotting or later analysis.
  ```bash
  ./queue_benchmarks --benchmark_format=json --benchmark_out=results.json
  ```
- **Iterations:** Control the minimum duration.
  ```bash
  ./queue_benchmarks --benchmark_min_time=1.0
  ```

For more options, you can use `./queue_benchmarks --help`.

## Generating Plots (Automated)
We have provided an automated pipeline to handle building, collecting results to JSON, parsing stats into a CSV, and rendering plots automatically via Python. 
Make sure you have mapped your python environment, then run:

```bash
python run_benchmarks.py
```
This automatically updates `results_all.json`, processes raw throughput stats to `results_all.csv`, and produces throughput comparison PNG visualizations in `results/throughput.png`.

## Benchmarking Methodology

To ensure accurate multi-threaded profiling:
- We exclude thread creation and joining overhead using `state.PauseTiming()` and `state.ResumeTiming()`.
- Thread execution is synchronized before operation timing begins using a shared spinlock (`std::atomic<bool> start`).
- All benchmarks use real-time tracking (`->UseRealTime()`) correctly measuring true wall-clock concurrency performance instead of CPU time (which can be misleading when threads block/yield on different cores).
- To minimize false sharing, shared metrics like the atomic `consumed` counters utilize `alignas(64)` where applicable.

## Queue Topology Variations Evaluated

Across our benchmark suite:
- **SPSC:** Evaluated across a basic 1 producer & 1 consumer pairing. The differences between Bounded and Unbounded highlight the runtime allocation costs.
- **MPSC:** Scales the producer count $(N=1, 2, 4, 8)$ while maintaining 1 active consumer, mimicking a multiplexed event sink or logging pipeline.
- **MPMC:** Scales threads uniformly across producers ($N/2$) and consumers ($N/2$). The locking (`blocking_mpmc_unbounded`) versus active spin queues demonstrates blocking/sleeping tradeoffs when congestion spikes.

## Example Results

Here is a sample result (on a system with 16 logical cores) compiled with `Release` options:

```
----------------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------------
BM_SPSC_UNBOUNDED/real_time   1552548400 ns        0.000 ns            1 items_per_second=6.44102M/s ops/sec=6.44102M/s
BM_SPSC_BOUNDED/real_time       71895508 ns        0.000 ns           12 items_per_second=139.091M/s ops/sec=139.091M/s
BM_MPSC/1/real_time            348759500 ns        0.000 ns            2 items_per_second=5.73461M/s ops/sec=5.73461M/s
BM_MPSC/2/real_time            603713400 ns        0.000 ns            1 items_per_second=6.62566M/s ops/sec=6.62566M/s
BM_MPSC/4/real_time           1153064000 ns        0.000 ns            1 items_per_second=6.93804M/s ops/sec=6.93804M/s
BM_MPSC/8/real_time           2324944000 ns        0.000 ns            1 items_per_second=6.88189M/s ops/sec=6.88189M/s
BM_BLOCKING_MPMC/2/real_time   316236500 ns        0.000 ns            2 items_per_second=3.16219M/s ops/sec=3.16219M/s
BM_BLOCKING_MPMC/4/real_time   816578600 ns        0.000 ns            1 items_per_second=2.44924M/s ops/sec=2.44924M/s
BM_BLOCKING_MPMC/8/real_time  2578731000 ns        0.000 ns            1 items_per_second=1.55115M/s ops/sec=1.55115M/s
BM_BLOCKING_MPMC/16/real_time 7226145200 ns        0.000 ns            1 items_per_second=1.10709M/s ops/sec=1.10709M/s
```
*Note that the bounded `lockfree_spsc_bounded` greatly outperforms the unbounded variations primarily because it requires no runtime memory allocation payload, emphasizing high cache locality.*

## Performance Analysis & Insights

### Throughput and Scalability Behavior
- **SPSC Scale:** Our lock-free SPSC queues demonstrate maximum base throughput, naturally avoiding contention across multiple producers or consumers.
- **MPSC/MPMC Scale:** As thread counts increase ($N=2, 4, 8, \dots$), active spin-based implementations generally sustain throughput within core count limits before experiencing drop-offs due to context-switching overhead and CPU saturation.

### Busy Spinning vs Blocking
- **Spinning (Lock-free):** Queues adopting `try_pop` semantics rapidly poll memory. This guarantees the lowest raw latency under sparse congestion but aggressively consumes CPU cycles.
- **Blocking (wait_and_pop):** The `blocking_mpmc_unbounded` implementation relies on condition variables and mutexes. When empty, threads sleep, yielding the core back to the OS. Overall throughput limits drop dramatically (~1-4 Million Ops/Sec) relative to spin-queues, due to kernel context-switch latencies. It shines structurally under severe over-subscription or mixed heavy-IO workloads where preserving CPU ticks matters.

### Cache Locality & Allocation Overhead
- **Bounded (Pre-allocated Array / Buffer):** `lockfree_spsc_bounded` hits exceptional throughputs (~$139$-$164$ Million Ops/Sec) due to contiguous CPU cacheline hardware prefetching and zero heap allocations.
- **Unbounded (Linked List / Node Payload):** Unbounded variants dynamically `new`/`delete` or `make_shared` memory nodes at runtime. Throughput systematically shrinks ($<10$ Million Ops/Sec) explicitly gated by the system's memory allocator locking.

### Contention Effects
Unbounded queue nodes create shared state cross-traffic logic. Higher producer counts introduce cache coherency misses (L1/L2 invalidations) causing cascading performance degradation known as false sharing. Ensuring padded alignments (`alignas(64)`) helps mitigate these artificial bottlenecks significantly in our benchmark runners.
