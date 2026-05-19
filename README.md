# ThreadsafeQueueLib-CodingClubIITG

ThreadsafeQueueLib provides high-performance wait-free, lock-free and blocking queues for C++20. It supports SPSC, MPSC, and MPMC with bounded or unbounded modes using policy-based templates for flexible, efficient, thread-safe data passing.

## Features & Supported Queues

The library provides various queue variants nested within the `tsfqueue::__impl` namespace via the aggregator header `<tsfqueue.hpp>`.

| Topology | Variant | Class Name | Characteristics |
| :--- | :--- | :--- | :--- |
| **SPSC** (1:1) | Bounded | `lockfree_spsc_bounded<T, Cap>` | Compile-time ring buffer, lock-free, zero runtime allocation, optimal cache proximity. |
| **SPSC** (1:1) | Unbounded | `lockfree_spsc_unbounded<T>` | Lock-free linked-list architecture allowing dynamic scaling. |
| **SPSC** (1:1) | FAST Unbounded | `FAST_lockfree_spsc_unbounded<T>` | Highly optimized unbounded list leveraging internal node block-caching. |
| **MPSC** (N:1) | Unbounded | `lockfree_mpsc_unbounded<T>` | Multiplexed consumer design, waits on contested writes from multiple producers. |
| **MPMC** (N:N) | Bounded | `lockfree_mpmc_bounded<T, Cap>` | Heavily contended lock-free array handling massive concurrency. |
| **MPMC** (N:N) | Unbounded | `blocking_mpmc_unbounded<T>` | Mutex + CV backed, yielding logical threads to OS reducing CPU spine cycles. |

## Quick Start

Since `ThreadSafeQueueLib` is purely header-only, installation strictly requires adding the `include` directory directly to your C++20-supported project's include path and enabling C++20 standard compiler features.

## Benchmarking & Performance

We include a professional benchmarking suite leveraging [Google Benchmark](https://github.com/google/benchmark) to track continuous queue performance, thread scaling, cache locality, and atomic contention.

### Benchmark Setup & Methodology

The pipeline isolates pure structural concurrency bounds by:
- Suspending thread creation/joining overheads explicitly out of operation timers via Google Benchmark `PauseTiming() / ResumeTiming()`.
- Tracking performance synchronously under `UseRealTime()` to avoid skewed CPU-bound metrics typical in sleep-heavy processes.
- Counteracting false sharing in shared benchmark metrics via manual `alignas(64)` payload padding.

### Running Benchmarks
An automated test runner drives CMake builds, runtime stat extraction (to JSON/CSV), and Matplotlib charting pipeline automatically. You can invoke it easily from the project root by running:

```bash
python benchmarking/run_benchmarks.py
```

*Results output visually and textually directly into the `benchmarking/results/` folder.*

### Example Results

Here is a sample result (on a system with 16 logical cores) compiled with `Release` optimizations:

```text
----------------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations UserCounters...
----------------------------------------------------------------------------------------
BM_SPSC_UNBOUNDED/real_time   1552548400 ns        0.000 ns            1 items_per_second=6.44102M/s ops/sec=6.44102M/s
BM_SPSC_BOUNDED/real_time       71895508 ns        0.000 ns           12 items_per_second=139.091M/s ops/sec=139.091M/s
BM_MPSC/1/real_time            348759500 ns        0.000 ns            2 items_per_second=5.73461M/s ops/sec=5.73461M/s
BM_MPSC/2/real_time            603713400 ns        0.000 ns            1 items_per_second=6.62566M/s ops/sec=6.62566M/s
BM_MPSC/4/real_time           1153064000 ns        0.000 ns            1 items_per_second=6.93804M/s ops/sec=6.93804M/s
BM_MPSC/8/real_time           2324944000 ns        0.000 ns            1 items_per_second=6.88189M/s ops/sec=6.88189M/s
BM_LOCKFREE_MPMC_BOUNDED/2/real_time    31370759 ns        0.000 ns           22 items_per_second=31.8768M/s ops/sec=31.8768M/s
BM_LOCKFREE_MPMC_BOUNDED/4/real_time   203394600 ns        0.000 ns            4 items_per_second=9.8331M/s ops/sec=9.8331M/s
BM_LOCKFREE_MPMC_BOUNDED/8/real_time   472495700 ns        0.000 ns            2 items_per_second=8.46569M/s ops/sec=8.46569M/s
BM_LOCKFREE_MPMC_BOUNDED/16/real_time 1389355600 ns        0.000 ns            1 items_per_second=5.75807M/s ops/sec=5.75807M/s
BM_BLOCKING_MPMC/2/real_time   316236500 ns        0.000 ns            2 items_per_second=3.16219M/s ops/sec=3.16219M/s
BM_BLOCKING_MPMC/4/real_time   816578600 ns        0.000 ns            1 items_per_second=2.44924M/s ops/sec=2.44924M/s
BM_BLOCKING_MPMC/8/real_time  2578731000 ns        0.000 ns            1 items_per_second=1.55115M/s ops/sec=1.55115M/s
BM_BLOCKING_MPMC/16/real_time 7226145200 ns        0.000 ns            1 items_per_second=1.10709M/s ops/sec=1.10709M/s
```

### Performance Analysis & Insights

- **Bounded Cache Alignment:** Ring-buffer mechanisms (`lockfree_spsc_bounded`) completely avoid dynamic memory allocations (`new` and `delete`), outclassing unbounded forms drastically owing directly to cacheline locality hardware-level prefetching. Uncontended bounded variations eclipse 145+ Million ops/sec on modern processors.
- **Spinning vs Block-Sleeping:** Try-based Lock-free iterations achieve minimal systemic latency under pure compute models compared to `wait_and_pop` mutex-backed queues inherently suffering from OS-level context switching during thread wake/sleep sequences (the MPMC blocking queue limits out at around ~1-4M ops/sec). They remain preferable only where pure un-contested CPU savings are aggressively demanded over raw throughput.
- **Throughput and Scalability Behavior:** Our lock-free SPSC queues demonstrate maximum base throughput, naturally avoiding contention across multiple producers or consumers. As thread counts increase ($N=2, 4, 8, \dots$), active spin-based MPSC implementations generally sustain throughput within core count limits before experiencing drop-offs due to context-switching overhead and CPU saturation.
- **Contention Effects:** Unbounded queue nodes create shared state cross-traffic logic. Higher producer counts introduce cache coherency misses (L1/L2 invalidations) causing cascading performance degradation known as false sharing. Ensuring padded alignments (`alignas(64)`) helps mitigate these artificial bottlenecks significantly.

