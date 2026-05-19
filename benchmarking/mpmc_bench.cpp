#include "common.hpp"
#include "tsfqueue.hpp"
#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace tsfqueue::__impl;

static void BM_LOCKFREE_MPMC_BOUNDED(benchmark::State& state) {
    constexpr size_t OPS = MPMC_OPS_PER_THREAD;
    // Bounded queue with power of two capacity
    constexpr size_t CAPACITY = 65536;

    const int total_threads = static_cast<int>(state.range(0));
    const int producers = total_threads / 2;
    const int consumers = total_threads / 2;

    for (auto _ : state) {
        state.PauseTiming();

        auto q = std::make_unique<lockfree_mpmc_bounded<int, CAPACITY>>();
        std::atomic<bool> start{false};
        alignas(64) std::atomic<size_t> consumed{0};
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;

        // Multiple Producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire)) {}

                for (size_t i = 0; i < OPS; ++i) {
                    while (!q->try_push(static_cast<int>(i))) {
                        // Busy spin until push succeeds
                    }
                }
            });
        }

        // Multiple Consumers
        for (int c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire)) {}

                int value;
                const size_t total = producers * OPS;
                while (consumed.load(std::memory_order_acquire) < total) {
                    if (q->try_pop(value)) {
                        benchmark::DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_release);
                    }
                }
            });
        }

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        for (auto& t : producer_threads) {
            t.join();
        }

        for (auto& t : consumer_threads) {
            t.join();
        }
    }

    const size_t total_ops = state.iterations() * producers * OPS;

    state.SetItemsProcessed(total_ops);
    state.counters["ops/sec"] = benchmark::Counter(static_cast<double>(total_ops), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_LOCKFREE_MPMC_BOUNDED)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->UseRealTime();

BENCHMARK_MAIN();
