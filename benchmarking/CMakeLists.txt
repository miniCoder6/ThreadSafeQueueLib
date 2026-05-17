cmake_minimum_required(VERSION 3.16)
project(ThreadSafeQueueBenchmarks CXX)

include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG main
)

set(BENCHMARK_ENABLE_TESTING OFF)

FetchContent_MakeAvailable(benchmark)

add_executable(
    queue_benchmarks
    all_queues_bench.cpp
)

find_package(Threads REQUIRED)

target_include_directories(
    queue_benchmarks
    PRIVATE
    ${PROJECT_SOURCE_DIR}/../include
)

target_link_libraries(
    queue_benchmarks
    benchmark::benchmark
    Threads::Threads
)

target_compile_features(
    queue_benchmarks
    PRIVATE
    cxx_std_20
)

if(MSVC)
    target_compile_options(queue_benchmarks PRIVATE /O2)
else()
    target_compile_options(
        queue_benchmarks
        PRIVATE
        -O3
        -march=native
    )
endif()