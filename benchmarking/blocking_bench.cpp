#include "common.hpp"
#include "tsfqueue.hpp"

using namespace tsfqueue::__impl;

static void BM_BLOCKING_MPMC(benchmark::State& state) {
    const int threads = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        blocking_mpmc_unbounded<int> q;
        std::atomic<bool> start = false;
        std::vector<std::thread> workers;

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&] {

                while (!start.load(std::memory_order_acquire));

                for (size_t i = 0; i < MPMC_OPS_PER_THREAD; ++i){
                    q.push(i);
                    int value;
                    while (!q.try_pop(value));
                    consume(value);
                }
            });
        }

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        for (auto& t : workers) {
            t.join();
        }
    }

    state.SetItemsProcessed(
        state.iterations() *
        threads *
        MPMC_OPS_PER_THREAD
    );
}

BENCHMARK(BM_BLOCKING_MPMC)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16);

BENCHMARK_MAIN();