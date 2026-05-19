#ifndef LOCKFREE_MPMC_BOUNDED_DEFS
#define LOCKFREE_MPMC_BOUNDED_DEFS

#include "utils.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace tsfqueue::__impl {
template <typename T, size_t Capacity> class lockfree_mpmc_bounded {

  // Lock-free bounded multiple-producer multiple-consumer queue
  // implemented using a ring buffer + per-slot sequence numbers.
  //
  // Core ideas:
  // 1. Producers reserve slots using CAS on tail
  // 2. Consumers reserve slots using CAS on head
  // 3. Every slot has a sequence number tracking lifecycle/generation
  // 4. Sequence numbers solve ABA + safe slot reuse
  //
  // Slot lifecycle:
  //
  // Initial:
  //   sequence == index
  //
  // Producer publishes:
  //   sequence = pos + 1
  //
  // Consumer releases:
  //   sequence = pos + Capacity
  //
  // Capacity MUST be power-of-two for fast modulo using bitmasking.

private:

  struct cell {
    // Per-slot lifecycle/generation state
    alignas(64) std::atomic<size_t> sequence;
    T data;
  };

  alignas(64) cell buffer[Capacity];
  alignas(64) std::atomic<size_t> head;
  alignas(64) std::atomic<size_t> tail;

  static_assert(Capacity > 1, "Capacity must be greater than 1");
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
  static_assert(std::is_move_constructible_v<T>, "T must be move constructible");
  static_assert(std::is_move_assignable_v<T>, "T must be move assignable");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

  // Prevent deadlocks entirely if an exception is thrown during element assignment
  static_assert(std::is_nothrow_move_assignable_v<T>, "T must be nothrow move assignable");
  static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible");

public:

  lockfree_mpmc_bounded() : head(0), tail(0) {
    for (size_t i = 0; i < Capacity; ++i){
        buffer[i].sequence.store(i, std::memory_order_relaxed);
    }
  }

  ~lockfree_mpmc_bounded() = default;

  bool try_push(T value);

  template <typename... Args>
  bool try_emplace(Args&&... args);

  bool try_pop(T& value);

  bool empty(void);

  size_t size(void);
};

} // namespace tsfqueue::__impl

#endif