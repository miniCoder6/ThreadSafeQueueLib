#pragma once

#include <atomic>
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>

constexpr size_t SPSC_OPS = 10'000'000;
constexpr size_t MPSC_OPS_PER_PRODUCER = 2'000'000;
constexpr size_t MPMC_OPS_PER_THREAD = 1'000'000;

template <typename T>
inline void consume(T const& value) {
    benchmark::DoNotOptimize(value);
}