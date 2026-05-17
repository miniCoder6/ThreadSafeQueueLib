# ThreadsafeQueueLib-CodingClubIITG

ThreadsafeQueueLib provides high-performance wait-free, lock-free and blocking queues for C++20. It supports SPSC, MPSC, and MPMC with bounded or unbounded modes using policy-based templates for flexible, efficient, thread-safe data passing.

## Benchmarking & Performance

We include a professional benchmarking suite leveraging [Google Benchmark](https://github.com/google/benchmark) to track continuous queue performance, thread scaling, cache locality, and atomic contention.

### Benchmark Setup & Methodology

The pipeline isolates pure structural concurrency bounds by:
- Suspending thread creation/joining overheads explicitly out of operation timers via Google Benchmark `PauseTiming() / ResumeTiming()`.
- Tracking performance synchronously under `UseRealTime()` to avoid skewed CPU-bound metrics typical in sleep-heavy processes.
- Counteracting false sharing in shared benchmark metrics via manual `alignas(64)` payload padding.

### Sub-Queue Topologies
Multiple scalable tests are aggregated across implementations:
- **SPSC (Single Producer, Single Consumer):** Tracks raw 1-to-1 passing bandwidth.
- **MPSC (Multi-Producer, Single Consumer):** Explores multiplexed bottlenecks where $N$ producers fan into 1 sink.
- **MPMC (Multi-Producer, Multi-Consumer):** Stresses maximal synchronization primitives crossing active blocks or locks.

### Running Benchmarks
An automated test runner drives CMake builds, stat extraction, and Matplotlib processing automatically:
```bash
cd benchmarking
python run_benchmarks.py
```
*Results, JSON datasets, and generated visualizations output directly into `benchmarking/results/`.*

### Performance Insights
- **Bounded Cache Alignment:** Ring-buffer mechanisms completely avoid dynamic memory allocations (`new` and `delete`), outclassing unbounded forms drastically owing directly to cacheline locality hardware-level prefetching.
- **Spinning vs Block-Sleeping:** Try-based Lock-free iterations achieve minimal systemic latency under pure compute models compared to `wait_and_pop` mutex-backed queues inherently suffering from OS-level context switching during thread wake/sleep sequences. Uncontended bounded variations eclipse 145+ Million ops/sec on modern processors.

For an extensive dive into runtime statistics, generated graphs, and Google Benchmark flag customizations across individual setups, read out strictly through [benchmarking/benchmark.md](benchmarking/benchmark.md).