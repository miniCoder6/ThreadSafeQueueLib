#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "include/blocking_mpmc_unbounded/queue.hpp"
#include "include/blocking_mpmc_unbounded/defs.hpp"

// If you expose a public alias elsewhere, use that.
// Otherwise use the concrete type:
using Q = tsfqueue::__impl::blocking_mpmc_unbounded<int>;

int main() {
    constexpr int producers = 4;
    constexpr int consumers = 4;
    constexpr int items_per_producer = 200000;

    Q q;
 
    std::atomic<long long> produced_sum{0};
    std::atomic<long long> consumed_sum{0};
    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};

    std::vector<std::thread> prod_threads;
    std::vector<std::thread> cons_threads;

    for (int p = 0; p < producers; ++p) {
        prod_threads.emplace_back([&, p] {
            for (int i = 1; i <= items_per_producer; ++i) {
                int v = p * items_per_producer + i;
                q.push(v);
                produced_sum.fetch_add(v, std::memory_order_relaxed);
                produced_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int c = 0; c < consumers; ++c) {
        cons_threads.emplace_back([&] {
            for (;;) {
                int v = 0;
                q.wait_and_pop(v);
                if (v == -1) break; // poison pill
                consumed_sum.fetch_add(v, std::memory_order_relaxed);
                consumed_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : prod_threads) t.join();

    // Stop consumers
    for (int i = 0; i < consumers; ++i) q.push(-1);

    for (auto& t : cons_threads) t.join();

    std::cout << "produced_count=" << produced_count.load() << "\n";
    std::cout << "consumed_count=" << consumed_count.load() << "\n";
    std::cout << "produced_sum=" << produced_sum.load() << "\n";
    std::cout << "consumed_sum=" << consumed_sum.load() << "\n";

    assert(produced_count.load() == producers * items_per_producer);
    assert(consumed_count.load() == producers * items_per_producer);
    assert(produced_sum.load() == consumed_sum.load());

    std::cout << "PASS\n";
    return 0;
}