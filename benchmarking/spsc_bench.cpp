#include "common.hpp"

#include "tsfqueue.hpp"

using namespace tsfqueue::__impl;

// =====================================================
// STANDARD SPSC
// =====================================================

static void BM_SPSC_UNBOUNDED(
    benchmark::State& state
) {

    for (auto _ : state) {
        state.PauseTiming();

        lockfree_spsc_unbounded<int> q;

        std::atomic<bool> start = false;

        std::thread producer([&] {

            while (!start.load(
                std::memory_order_acquire));

            for (size_t i = 0;
                 i < SPSC_OPS;
                 ++i) {

                q.push(i);
            }
        });

        std::thread consumer([&] {

            while (!start.load(
                std::memory_order_acquire));

            int value;

            for (size_t i = 0;
                 i < SPSC_OPS;
                 ++i) {

                while (!q.try_pop(value));

                consume(value);
            }
        });

        state.ResumeTiming();
        start.store(
            true,
            std::memory_order_release
        );

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(
        state.iterations() *
        SPSC_OPS
    );
}

BENCHMARK(BM_SPSC_UNBOUNDED);


// =====================================================
// BOUNDED SPSC
// =====================================================

static void BM_SPSC_BOUNDED(
    benchmark::State& state
) {

    constexpr size_t CAPACITY = 1024;

    for (auto _ : state) {
        state.PauseTiming();

        lockfree_spsc_bounded<int, CAPACITY> q;

        std::atomic<bool> start = false;

        std::thread producer([&] {

            while (!start.load(
                std::memory_order_acquire));

            for (size_t i = 0;
                 i < SPSC_OPS;
                 ++i) {

                while (!q.try_push(i));
            }
        });

        std::thread consumer([&] {

            while (!start.load(
                std::memory_order_acquire));

            int value;

            for (size_t i = 0;
                 i < SPSC_OPS;
                 ++i) {

                while (!q.try_pop(value));

                consume(value);
            }
        });

        state.ResumeTiming();
        start.store(
            true,
            std::memory_order_release
        );

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(
        state.iterations() *
        SPSC_OPS
    );
}

BENCHMARK(BM_SPSC_BOUNDED);
