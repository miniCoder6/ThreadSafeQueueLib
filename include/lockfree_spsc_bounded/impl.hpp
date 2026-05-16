#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"

template <typename T, size_t Capacity>
using queue = tsfqueue::__impl::lockfree_spsc_bounded<T, Capacity>;

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_push(T value) {
    wait_and_emplace(std::move(value));
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_push(T value) {
    return try_emplace(std::move(value));
}

template <typename T, size_t Capacity>
template <typename... Args>
bool queue<T, Capacity>::try_emplace(Args&&... args) {
    size_t current_tail = tail.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity;

    if (next_tail == head_cache) {
        head_cache = head.load(std::memory_order_acquire);

        if (next_tail == head_cache) {
            return false; // Queue full
        }
    }

    arr[current_tail] = T(std::forward<Args>(args)...);

    tail.store(next_tail, std::memory_order_release);

    return true;
}

template <typename T, size_t Capacity>
template <typename... Args>
void queue<T, Capacity>::wait_and_emplace(Args&&... args) {
    while (true) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % capacity;

        if (next_tail == head_cache) {
            head_cache = head.load(std::memory_order_acquire);

            if (next_tail == head_cache) {
                continue; // Queue full
            }
        }

        arr[current_tail] = T(std::forward<Args>(args)...);

        tail.store(next_tail, std::memory_order_release);

        return;
    }
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_pop(T &value) {
    size_t current_head = head.load(std::memory_order_relaxed);

    if (current_head == tail_cache) {
        tail_cache = tail.load(std::memory_order_acquire);
        if (current_head == tail_cache) {
            return false; // Queue is empty
        }
    }

    value = std::move(arr[current_head]);
    head.store((current_head + 1) % capacity, std::memory_order_release);
    return true;
}

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_pop(T &value) {
    while (true) {
        size_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail_cache) {
            tail_cache = tail.load(std::memory_order_acquire);
            if (current_head == tail_cache) {
                continue; // Queue is empty, keep waiting
            }
        }

        value = std::move(arr[current_head]);
        head.store((current_head + 1) % capacity, std::memory_order_release);
        return;
    }
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::peek(T &value) {
    size_t current_head = head.load(std::memory_order_relaxed);

    if (current_head == tail_cache) {
        tail_cache = tail.load(std::memory_order_acquire);
        if (current_head == tail_cache) {
            return false; // Queue is empty
        }
    }

    value = arr[current_head];
    return true;
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::empty() {
    // Approximate, non-linearizable check
    // Between reads, producer/consumer may modify queue
    return head.load(std::memory_order_relaxed) == tail.load(std::memory_order_relaxed);
}

template <typename T, size_t Capacity>
size_t queue<T, Capacity>::size() {
    size_t current_head = head.load(std::memory_order_relaxed);
    size_t current_tail = tail.load(std::memory_order_relaxed);
    return (current_tail + capacity - current_head) % capacity;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??