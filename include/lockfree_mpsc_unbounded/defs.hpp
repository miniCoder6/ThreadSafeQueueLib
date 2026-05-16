#ifndef LOCKFREE_MPSC_UNBOUNDED_DEFS
#define LOCKFREE_MPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace tsfqueue::__impl {

template <typename T>
class lockfree_mpsc_unbounded {
  // MPSC (Multi-Producer, Single-Consumer) lock-free unbounded queue.
  //
  // Design overview
  // ---------------
  // Like the SPSC variant, we maintain a linked list with a sentinel (stub)
  // node. tail_ always points to the stub (the last *empty* slot); head_
  // always points to the first *readable* node.
  //
  // The key difference from SPSC is that multiple threads may call push()
  // concurrently, so the tail advance must be done with a CAS.  We use the
  // "helping" trick from §7.22 of the textbook: if a producer loses the race
  // to claim the stub's data slot, it helps the winner advance tail_ before
  // retrying, keeping the algorithm lock-free.
  //
  // Because there is only ONE consumer, pop() never races with itself and
  // needs no CAS on head_ — a plain store suffices.
  //
  // Memory reclamation
  // ------------------
  // Unlike the textbook's reference-counted scheme, we can get away with
  // simpler logic here because:
  //   • Only one thread reads/frees from the head side.
  //   • Producers only ever write to tail_ and to nodes they just allocated;
  //     they never dereference head_.
  // So raw node pointers are safe as long as the consumer is the only one
  // calling pop()/destructor.  No hazard pointers or epoch reclamation needed.
  //
  // Cache alignment
  // ---------------
  // head_ and tail_ are on separate cache lines to eliminate false sharing:
  // producers hammer tail_; the consumer hammers head_.  Without alignment
  // they would ping-pong the same cache line between cores on every operation.
  //
  // Why no shared_ptr?
  // ------------------
  // shared_ptr carries atomic ref-count overhead on every copy/destroy.  For
  // a queue node whose lifetime we control precisely (freed by the consumer
  // after pop, or in the destructor) that overhead buys us nothing and slows
  // every push/pop.  Raw owning pointers + disciplined lifetime rules are
  // cheaper and sufficient here.
  //
  // Why are next pointers atomic in Lockless_Node?
  // -----------------------------------------------
  // A producer writes node->next = new_stub inside push(), while the consumer
  // may be reading head_->next inside pop() at the same moment.  Without
  // atomic, that is a data race and UB even in SPSC.  In MPSC it is even more
  // critical: multiple producers may race to set the same node's next pointer.
  // std::atomic gives us the sequentially-consistent visibility we need without
  // a mutex.

private:
  using node = tsfqueue::__utils::Lockless_Node<T>;

  static_assert(std::is_default_constructible_v<T>,
                "lockfree_mpsc_unbounded requires T to be default constructible"
                " (used to value-initialise the stub node).");
  static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
                "lockfree_mpsc_unbounded requires T to be assignable.");

  alignas(64) std::atomic<node*> head_;   // consumer side
  alignas(64) std::atomic<node*> tail_;   // producer side
  std::atomic<size_t> sz_{0};

public:
  // --------------------------------------------------------------------- //
  //  Lifecycle
  // --------------------------------------------------------------------- //

  lockfree_mpsc_unbounded();
  ~lockfree_mpsc_unbounded();

  // Non-copyable, non-movable — atomics and raw owning pointers make
  // copy/move semantics non-trivial and rarely useful for a queue.
  lockfree_mpsc_unbounded(const lockfree_mpsc_unbounded&)            = delete;
  lockfree_mpsc_unbounded& operator=(const lockfree_mpsc_unbounded&) = delete;
  lockfree_mpsc_unbounded(lockfree_mpsc_unbounded&&)                 = delete;
  lockfree_mpsc_unbounded& operator=(lockfree_mpsc_unbounded&&)      = delete;

  // --------------------------------------------------------------------- //
  //  Producers  (safe to call from multiple threads simultaneously)
  // --------------------------------------------------------------------- //

  /// Copy-push: enqueue a copy of value.
  void push(const T& value);

  /// Move-push: enqueue by moving value in.
  void push(T&& value);

  /// Emplace: construct T in-place from args and enqueue.
  /// Perfect-forwarding; avoids any extra copy/move of T.
  template <typename... Args>
  void emplace(Args&&... args);

  // --------------------------------------------------------------------- //
  //  Consumer  (must be called from ONE thread only)
  // --------------------------------------------------------------------- //

  /// Blocking pop: spins until an item is available, then stores it in value.
  void wait_and_pop(T& value);

  /// Non-blocking pop: if the queue is non-empty stores the front item in
  /// value and returns true; otherwise leaves value unchanged and returns false.
  bool try_pop(T& value);

  /// Peek at the front element without removing it.
  /// Returns false if the queue is empty.
  bool peek(T& value);

  // --------------------------------------------------------------------- //
  //  Observers  (safe to call from any thread; snapshot only)
  // --------------------------------------------------------------------- //

  bool   empty() const;
  size_t size()  const;
};

} // namespace tsfqueue::__impl

#include "impl.hpp"

#endif // LOCKFREE_MPSC_UNBOUNDED_DEFS