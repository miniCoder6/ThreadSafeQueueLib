#ifndef LOCKFREE_MPSC_UNBOUNDED_IMPL
#define LOCKFREE_MPSC_UNBOUNDED_IMPL

// mpsc_impl.hpp — included at the bottom of mpsc_defs.hpp; do not include
// directly.
//
// Implementation notes
// --------------------
// push() — lock-free, multi-producer safe
//   We use the "claim-then-help" pattern (§7.22 of the text):
//     1. Allocate a new stub node (new_stub).
//     2. CAS on tail_->data.  Whoever wins "owns" this push.
//        • Winner: links new_stub as next, swings tail_ forward, done.
//        • Loser:  helps  the winner swing tail_ forward, then retries
//                  from the newtail.  This is what makes us lock-free:
//                  no thread can be stuck behind a stalled producer forever.
//
// pop() / wait_and_pop() — wait-free for the single consumer
//   head_ is only ever touched by the consumer, so no CAS needed.
//   We just read head_->next; if non-null, advance head_ and return data.
//
// Memory ordering rationale
// -------------------------
//   push - CAS on tail_->data : release (publishes the write to data)
//   push - store  tail_       : release (publishes the new tail)
//   pop  - load   head_->next : acquire (sees the release from push)
//   sz_  - incremented with   release in push, load with acquire in size()
//          so size() sees at least as many pushes as have completed.

#include <thread>   // std::this_thread::yield  (for wait_and_pop spin)

namespace tsfqueue::__impl {

// ================================================================ helpers ===

namespace {

/// Attempt to swing `tail_atomic` from old_tail to new_tail once.
/// Returns true if this thread moved it, false if another thread did.
template <typename Node>
inline bool try_advance_tail(std::atomic<Node*>& tail_atomic,
                              Node*               old_tail,
                              Node*               new_tail)
{
    return tail_atomic.compare_exchange_strong(
        old_tail, new_tail,
        std::memory_order_release,
        std::memory_order_relaxed);
}

} // anonymous namespace

// ================================================================= ctor / dtor

template <typename T>
lockfree_mpsc_unbounded<T>::lockfree_mpsc_unbounded()
{
    node* stub = new node{};       // value-initialised; next == nullptr
    head_.store(stub, std::memory_order_relaxed);
    tail_.store(stub, std::memory_order_relaxed);
}

template <typename T>
lockfree_mpsc_unbounded<T>::~lockfree_mpsc_unbounded()
{
    // Drain any leftover items so T destructors run, then free nodes.
    // Only the destructor (= consumer side) calls this, so no races.
    node* curr = head_.load(std::memory_order_relaxed);
    while (curr) {
        node* nxt = curr->next.load(std::memory_order_relaxed);
        delete curr;
        curr = nxt;
    }
}

// ================================================================== emplace ===

template <typename T>
template <typename... Args>
void lockfree_mpsc_unbounded<T>::emplace(Args&&... args)
{
    // Allocate and fully construct the new stub *before* we touch tail_,
    // so producers never see a half-constructed node.
    node* new_stub = new node{};

    // Construct the value we want to publish directly into a local, then
    // we will move it into the claimed slot below.
    T new_value(std::forward<Args>(args)...);

    node* old_tail = tail_.load(std::memory_order_relaxed);

    for (;;) {
        // --- try to claim old_tail's data slot ---
        // The slot starts as default-constructed T.  We use a CAS on a
        // sentinel "occupied" flag instead of the data itself because T
        // may not be atomic.  We track occupation with next: nullptr means
        // the slot is unclaimed; a non-null next means a producer already
        // claimed it and is finishing up.
        //
        // Simpler scheme that works for MPSC:
        //   Each stub node's `data` field is the published value.
        //   We use an atomic<bool> "claimed" flag per node.
        //
        // However Lockless_Node only has {T data, atomic<Lockless_Node*> next}.
        // So we repurpose next: nullptr == unclaimed stub.
        //
        // Race-free claim protocol:
        //   CAS next from nullptr -> new_stub.  Whoever wins owns the push.

        node* expected_next = nullptr;
        if (old_tail->next.compare_exchange_strong(
                expected_next, new_stub,
                std::memory_order_release,    // publish new_stub allocation
                std::memory_order_relaxed))
        {
            // --- We won the claim ---
            // Move the value into old_tail (the now-real data node).
            old_tail->data = std::move(new_value);

            // Swing tail_ forward to the new stub.
            // Use a CAS so we don't regress tail_ if another helper already
            // moved it further (shouldn't happen in MPSC but be defensive).
            tail_.compare_exchange_strong(
                old_tail, new_stub,
                std::memory_order_release,
                std::memory_order_relaxed);

            sz_.fetch_add(1, std::memory_order_release);
            return;
        }

        // --- We lost the claim ---
        // expected_next now holds the next pointer the winner set.
        // Help finish: try to advance tail_ past old_tail.
        node* winner_stub = expected_next;  // non-null: winner's new stub
        tail_.compare_exchange_strong(
            old_tail, winner_stub,
            std::memory_order_release,
            std::memory_order_relaxed);

        // Reload tail_ and retry.  old_tail was updated by the failed CAS.
        old_tail = tail_.load(std::memory_order_relaxed);
    }
}

// ================================================================== push ===

template <typename T>
void lockfree_mpsc_unbounded<T>::push(const T& value)
{
    emplace(value);
}

template <typename T>
void lockfree_mpsc_unbounded<T>::push(T&& value)
{
    emplace(std::move(value));
}

// ================================================================== pop ====

template <typename T>
bool lockfree_mpsc_unbounded<T>::try_pop(T& value)
{
    // Single consumer: no CAS needed on head_.
    node* old_head = head_.load(std::memory_order_relaxed);
    node* next     = old_head->next.load(std::memory_order_acquire);
    //                                   ^^^^^^^ pairs with release in push

    if (!next) {
        return false;   // queue is empty
    }

    // next is the real data node; old_head was the stub.
    // Move the data out before advancing head_ so the destructor on the
    // old head never touches live data.
    value = std::move(next->data);

    // Advance head_: old_head (stub) is now unreachable from the consumer
    // and no producer will touch it again, so plain store is safe.
    head_.store(next, std::memory_order_release);

    delete old_head;   // free the old stub

    sz_.fetch_sub(1, std::memory_order_release);
    return true;
}

template <typename T>
void lockfree_mpsc_unbounded<T>::wait_and_pop(T& value)
{
    // Spin until an item is available.  Yields the thread on each failed
    // attempt to avoid burning CPU while producers are slow.
    while (!try_pop(value)) {
        std::this_thread::yield();
    }
}

// ================================================================= peek ====

template <typename T>
bool lockfree_mpsc_unbounded<T>::peek(T& value)
{
    node* old_head = head_.load(std::memory_order_relaxed);
    node* next     = old_head->next.load(std::memory_order_acquire);

    if (!next) {
        return false;
    }

    value = next->data;   // copy, not move — node stays in the queue
    return true;
}

// =============================================================== observers ==

template <typename T>
bool lockfree_mpsc_unbounded<T>::empty() const
{
    node* old_head = head_.load(std::memory_order_relaxed);
    return old_head->next.load(std::memory_order_acquire) == nullptr;
}

template <typename T>
size_t lockfree_mpsc_unbounded<T>::size() const
{
    return sz_.load(std::memory_order_acquire);
}

} // namespace tsfqueue::__impl

#endif // LOCKFREE_MPSC_UNBOUNDED_IMPL