#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::lockfree_spsc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {
    node *new_node = new node();
    new_node->next.store(nullptr, std::memory_order_relaxed);

    node *curr_tail = tail.load(std::memory_order_relaxed);

    // move value into current tail
    curr_tail->data = std::move(value);

    // link new stub node
    curr_tail->next.store(new_node, std::memory_order_release);

    // move tail forward
    tail.store(new_node, std::memory_order_relaxed);

    sz.fetch_add(1, std::memory_order_relaxed);
}

// perfect forwarding
template <typename... Args>
void emplace_back(Args &&...args)
{
    node *new_node = new node();
    new_node->next.store(nullptr, std::memory_order_relaxed);

    node *curr_tail = tail.load(std::memory_order_relaxed);

    curr_tail->data = T(std::forward<Args>(args)...);

    curr_tail->next.store(new_node, std::memory_order_release);
    tail.store(new_node, std::memory_order_relaxed);

    sz.fetch_add(1, std::memory_order_relaxed);
}

template <typename T> bool queue<T>::try_pop(T &value) {
    node *curr_head = head.load(std::memory_order_relaxed);
    node *next = curr_head->next.load(std::memory_order_acquire);

    if (!next) return false;

    value = std::move(curr_head->data);

    head.store(next, std::memory_order_relaxed);
    delete curr_head;

    sz.fetch_sub(1, std::memory_order_relaxed);

    return true;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {
    node *curr_head;
    node *next;

    while (true)
    {
        curr_head = head.load(std::memory_order_relaxed);
        next = curr_head->next.load(std::memory_order_acquire);

        if (next) break;
        // spin (can add backoff/yield)
    }

    value = std::move(curr_head->data);

    head.store(next, std::memory_order_relaxed);
    delete curr_head;

    sz.fetch_sub(1, std::memory_order_relaxed);
}

template <typename T> bool queue<T>::peek(T &value) {
    node *curr_head = head.load(std::memory_order_relaxed);
    node *next = curr_head->next.load(std::memory_order_acquire);

    if (!next) return false;

    value = curr_head->data;
    return true;
}

template <typename T> bool queue<T>::empty(void) {
    node *curr_head = head.load(std::memory_order_relaxed);
    node *next = curr_head->next.load(std::memory_order_acquire);
    return next == nullptr;
}

template <typename T> size_t queue<T>::size(){
    return sz.load(std::memory_order_relaxed);
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??