#include "common.hpp"
#include "tsfqueue.hpp"
#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace tsfqueue::__impl;

// SPSC UNBOUNDED
static void BM_SPSC_UNBOUNDED(benchmark::State& state) {
    constexpr size_t OPS = SPSC_OPS;
    lockfree_spsc_unbounded<int> q;

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};
        std::thread producer([&] {
            while (!start.load(std::memory_order_acquire)) {}

            for (size_t i = 0; i < OPS; ++i) {
                q.push(static_cast<int>(i));
            }
        });

        std::thread consumer([&] {
            while (!start.load(std::memory_order_acquire)) {}

            int value;
            for (size_t i = 0; i < OPS; ++i) {
                while (!q.try_pop(value)) {}
                benchmark::DoNotOptimize(value);
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(state.iterations() * OPS);
    state.counters["ops/sec"] = benchmark::Counter(static_cast<double>(state.iterations() * OPS), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_SPSC_UNBOUNDED)->UseRealTime();


// SPSC BOUNDED
static void BM_SPSC_BOUNDED(benchmark::State& state) {
    constexpr size_t OPS = SPSC_OPS;
    constexpr size_t CAPACITY = 1024;

    lockfree_spsc_bounded<int, CAPACITY> q;

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};
        std::thread producer([&] {
            while (!start.load(std::memory_order_acquire)) {}

            for (size_t i = 0; i < OPS; ++i) {
                while (!q.try_push(static_cast<int>(i))) {}
            }
        });

        std::thread consumer([&] {
            while (!start.load(std::memory_order_acquire)) {}

            int value;
            for (size_t i = 0; i < OPS; ++i) {
                while (!q.try_pop(value)) {}
                benchmark::DoNotOptimize(value);
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(state.iterations() * OPS);
    state.counters["ops/sec"] = benchmark::Counter(static_cast<double>(state.iterations() * OPS), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_SPSC_BOUNDED)->UseRealTime();


// MPSC UNBOUNDED
static void BM_MPSC(benchmark::State& state) {
    constexpr size_t OPS = MPSC_OPS_PER_PRODUCER;
    const int producers = static_cast<int>(state.range(0));

    lockfree_mpsc_unbounded<int> q;

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};
        alignas(64) std::atomic<size_t> consumed{0};
        std::vector<std::thread> producer_threads;

        // Multiple Producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire)) {}

                for (size_t i = 0; i < OPS; ++i) {
                    q.push(static_cast<int>(i));
                }
            });
        }

        // Single Consumer
        std::thread consumer([&] {
            while (!start.load(std::memory_order_acquire)) {}

            int value;
            const size_t total = producers * OPS;

            while (consumed.load(std::memory_order_acquire) < total) {
                if (q.try_pop(value)) {
                    benchmark::DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_release);
                }
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        for (auto& t : producer_threads) {
            t.join();
        }

        consumer.join();
    }

    const size_t total_ops = state.iterations() * producers * OPS;

    state.SetItemsProcessed(total_ops);
    state.counters["ops/sec"] = benchmark::Counter(static_cast<double>(total_ops), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_MPSC)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();


// BLOCKING MPMC UNBOUNDED
static void BM_BLOCKING_MPMC(benchmark::State& state) {
    constexpr size_t OPS = MPMC_OPS_PER_THREAD;

    const int total_threads = static_cast<int>(state.range(0));
    const int producers = total_threads / 2;
    const int consumers = total_threads / 2;

    blocking_mpmc_unbounded<int> q;

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};
        alignas(64) std::atomic<size_t> consumed{0};
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;

        // Multiple Producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire)) {}

                for (size_t i = 0; i < OPS; ++i) {
                    q.push(static_cast<int>(i));
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
                    if (q.try_pop(value)) {
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

BENCHMARK(BM_BLOCKING_MPMC)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->UseRealTime();


BENCHMARK_MAIN();