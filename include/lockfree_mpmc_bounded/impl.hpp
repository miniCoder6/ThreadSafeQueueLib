#ifndef LOCKFREE_MPMC_BOUNDED_IMPL
#define LOCKFREE_MPMC_BOUNDED_IMPL

#include "defs.hpp"

template <typename T, size_t Capacity>
using queue = tsfqueue::__impl::lockfree_mpmc_bounded<T, Capacity>;

template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_push(T value){
    cell* curr_cell;
    size_t pos = tail.load(std::memory_order_relaxed);

    while (true){
        curr_cell = &buffer[pos & (Capacity - 1)];
        size_t seq = curr_cell->sequence.load(std::memory_order_acquire);

        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        // Slot available for producer
        if (diff == 0){
            // Try reserving slot
            if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)){
                break;
            }
        }

        // Queue full
        else if (diff < 0){
            return false;
        }

        // Another producer advanced tail
        else{
            pos = tail.load(std::memory_order_relaxed);
        }
    }

    // Producer owns slot exclusively now
    curr_cell->data = std::move(value);
    curr_cell->sequence.store(pos + 1, std::memory_order_release);

    return true;
}


template <typename T, size_t Capacity>
template <typename... Args>
bool queue<T, Capacity>::try_emplace(Args&&... args){
    cell* curr_cell;
    size_t pos = tail.load(std::memory_order_relaxed);

    while (true){
        curr_cell = &buffer[pos & (Capacity - 1)];
        size_t seq = curr_cell->sequence.load(std::memory_order_acquire);

        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        // Slot available
        if (diff == 0){
            if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)){
                break;
            }
        }

        // Queue full
        else if (diff < 0){
            return false;
        }

        // Retry
        else{
            pos = tail.load(std::memory_order_relaxed);
        }
    }

    curr_cell->data = T(std::forward<Args>(args)...);
    curr_cell->sequence.store(pos + 1, std::memory_order_release);

    return true;
}


template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_pop(T& value){
    cell* curr_cell;
    size_t pos = head.load(std::memory_order_relaxed);

    while (true){
        curr_cell = &buffer[pos & (Capacity - 1)];
        size_t seq = curr_cell->sequence.load(std::memory_order_acquire);

        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
        // Slot contains valid data
        if (diff == 0){
            // Try reserving slot for consumer
            if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)){
                break;
            }
        }

        // Queue empty
        else if (diff < 0){
            return false;
        }

        // Another consumer advanced head
        else{
            pos = head.load(std::memory_order_relaxed);
        }
    }

    value = std::move(curr_cell->data);
    // Mark slot reusable for next producer generation
    curr_cell->sequence.store(pos + Capacity, std::memory_order_release);

    return true;
}


template <typename T, size_t Capacity>
bool queue<T, Capacity>::empty(void){
    size_t pos = head.load(std::memory_order_relaxed);
    cell* curr_cell = &buffer[pos & (Capacity - 1)];
    size_t seq = curr_cell->sequence.load(std::memory_order_acquire);

    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

    return diff < 0;
}


template <typename T, size_t Capacity>
size_t queue<T, Capacity>::size(void){
    size_t t = tail.load(std::memory_order_relaxed);
    size_t h = head.load(std::memory_order_relaxed);

    // Prevent underflow during concurrent access:
    // If head advanced past tail after we read tail, casting to 
    // signed difference prevents producing a massively large incorrect size
    std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(t) - static_cast<std::ptrdiff_t>(h);
    return diff > 0 ? static_cast<size_t>(diff) : 0;
}

#endif