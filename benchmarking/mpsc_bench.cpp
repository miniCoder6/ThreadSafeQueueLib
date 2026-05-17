#include "common.hpp"

#include "tsfqueue.hpp"

using namespace tsfqueue::__impl;

static void BM_MPSC(
    benchmark::State& state
) {

    const int producers =
        state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        lockfree_mpsc_unbounded<int> q;

        std::atomic<bool> start = false;

        std::atomic<size_t> consumed = 0;

        std::vector<std::thread>
            producer_threads;

        for (int p = 0;
             p < producers;
             ++p) {

            producer_threads.emplace_back(
                [&] {

                while (!start.load(
                    std::memory_order_acquire));

                for (size_t i = 0;
                     i <
                     MPSC_OPS_PER_PRODUCER;
                     ++i) {

                    q.push(i);
                }
            });
        }

        std::thread consumer([&] {

            while (!start.load(
                std::memory_order_acquire));

            int value;

            const size_t total =
                producers *
                MPSC_OPS_PER_PRODUCER;

            while (
                consumed.load(
                    std::memory_order_relaxed
                ) < total
            ) {

                if (q.try_pop(value)) {

                    consume(value);

                    consumed.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            }
        });

        state.ResumeTiming();
        start.store(
            true,
            std::memory_order_release
        );

        for (auto& t : producer_threads) {
            t.join();
        }

        consumer.join();
    }

    state.SetItemsProcessed(
        state.iterations() *
        producers *
        MPSC_OPS_PER_PRODUCER
    );
}

BENCHMARK(BM_MPSC)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8);