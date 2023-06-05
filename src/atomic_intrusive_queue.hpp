

#ifndef NET_SRC_ATOMIC_INTRUSIVE_QUEUE_HPP_
#define NET_SRC_ATOMIC_INTRUSIVE_QUEUE_HPP_

#include <atomic>
#include <cassert>
#include <stdexec/__detail/__intrusive_queue.hpp>
#include <utility>

namespace net {
using stdexec::__intrusive_queue;

template <auto Next>
class atomic_intrusive_queue;

// An intrusive queue that supports multiple threads concurrently enqueueing items to the queue and
// a single consumer dequeuing items from the queue. The consumer always dequeues all items at once,
// resetting the queue back to empty. The consumer can also mark itself as inactive. The next
// producer to enqueue an item will then be notified that the consumer is inactive by the
// return-value of enqueue(). This producer is  then responsible for waking up the consumer.
template <typename Item, Item* Item::*Next>
class atomic_intrusive_queue<Next> {
 public:
  atomic_intrusive_queue() noexcept : head_(nullptr) {}

  explicit atomic_intrusive_queue(bool initially_active) noexcept
      : head_(initially_active ? nullptr : inactive_value()) {}

  ~atomic_intrusive_queue() {
    // Check that all items in this queue have been dequeued.
    // Not doing so is likely a bug in the code.
    assert(head_.load(std::memory_order_relaxed) == nullptr
           || head_.load(std::memory_order_relaxed) == inactive_value());
  }

  // Disable move/copy construction/assignment
  atomic_intrusive_queue(const atomic_intrusive_queue&) = delete;
  atomic_intrusive_queue(atomic_intrusive_queue&&) = delete;
  atomic_intrusive_queue& operator=(const atomic_intrusive_queue&) = delete;
  atomic_intrusive_queue& operator=(atomic_intrusive_queue&&) = delete;

  // Returns true if previous state was inactive, this operation successfully marked it as active.
  // Returns false if the previous state was active.
  [[nodiscard]] bool try_mark_active() noexcept {
    void* old_value = inactive_value();
    return head_.compare_exchange_strong(old_value, nullptr, std::memory_order_acquire,
                                         std::memory_order_relaxed);
  }

  // Either enqueue an item to the queue if the producer is active, otherwise if the producer is
  // inactive then mark the queue as active but do not enqueue the item. The assumption is that the
  // caller then becomes the producer and will process item directly.  Returns true if the item was
  // enqueued. Returns false if the item was not enqueued and the queue was transitioned from
  // inactive to active.
  [[nodiscard]] bool enqueue_or_mark_active(Item* item) noexcept {
    const void* inactive = inactive_value();
    void* old_value = head_.load(std::memory_order_relaxed);
    void* new_value = nullptr;
    do {
      if (old_value == inactive) {
        new_value = nullptr;
      } else {
        item->*Next = static_cast<Item*>(old_value);
        new_value = item;
      }
    } while (!head_.compare_exchange_weak(old_value, new_value, std::memory_order_acq_rel));
    return old_value != inactive;
  }

  // Enqueue an item to the queue. Returns true if the producer is inactive and needs to be woken
  // up. The calling thread has responsibility for waking up the producer.
  [[nodiscard]] bool enqueue(Item* item) noexcept {
    void* const inactive = inactive_value();
    void* old_value = head_.load(std::memory_order_relaxed);
    do {
      item->*Next = (old_value == inactive) ? nullptr : static_cast<Item*>(old_value);
    } while (!head_.compare_exchange_weak(old_value, item, std::memory_order_acq_rel));
    return old_value == inactive;
  }

  // Dequeue all items. Resetting the queue back to empty.
  // Not valid to call if the producer is inactive.
  [[nodiscard]] __intrusive_queue<Next> dequeue_all() noexcept {
    void* value = head_.load(std::memory_order_relaxed);
    if (value == nullptr) {
      // Queue is empty, return empty queue.
      return {};
    }
    assert(value != inactive_value());
    value = head_.exchange(nullptr, std::memory_order_acquire);
    assert(value != inactive_value());
    assert(value != nullptr);

    return __intrusive_queue<Next>::make_reversed(static_cast<Item*>(value));
  }

  [[nodiscard]] bool try_mark_inactive() noexcept {
    void* const inactive = inactive_value();
    void* old_value = head_.load(std::memory_order_relaxed);
    if (old_value == nullptr) {
      if (head_.compare_exchange_strong(old_value,                  //
                                        inactive,                   //
                                        std::memory_order_release,  //
                                        std::memory_order_relaxed)) {
        // Successfully marked as inactive
        return true;
      }
    }

    // The queue was
    assert(old_value != nullptr);
    assert(old_value != inactive);
    return false;
  }

  // Atomically either mark the producer as inactive if the queue was empty
  // or dequeue pending items from the queue.
  // Not valid to call if the producer is already marked as inactive.
  [[nodiscard]] __intrusive_queue<Next> try_mark_inactive_or_dequeue_all() noexcept {
    if (try_mark_inactive()) {
      return {};
    }

    void* old_value = head_.exchange(nullptr, std::memory_order_acquire);
    assert(old_value != nullptr);
    assert(old_value != inactive_value());

    return __intrusive_queue<Next>::make_reversed(static_cast<Item*>(old_value));
  }

 private:
  void* inactive_value() const noexcept {
    // Pick some pointer that is not nullptr and that is
    // guaranteed to not be the address of a valid item.
    return const_cast<void*>(static_cast<const void*>(&head_));
  }

  std::atomic<void*> head_;
};

}  // namespace net

#endif
