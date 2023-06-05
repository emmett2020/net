#ifndef NET_INTRUSIVE_LIST_H_
#define NET_INTRUSIVE_LIST_H_

#include <utility>

namespace net {

template <typename T, T* T::*Next, T* T::*Prev>
class intrusive_list {
 public:
  intrusive_list() noexcept : head_(nullptr), tail_(nullptr) {}

  intrusive_list(intrusive_list&& other) noexcept
      : head_(std::exchange(other.head_, nullptr)), tail_(std::exchange(other.tail_, nullptr)) {}

  ~intrusive_list() { assert(empty()); }

  intrusive_list& operator=(const intrusive_list&) = delete;
  intrusive_list& operator=(intrusive_list&&) = delete;

  [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

  void swap(intrusive_list& other) noexcept {
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
  }

  void push_back(T* item) noexcept {
    item->*Prev = tail_;
    item->*Next = nullptr;
    if (tail_ == nullptr) {
      head_ = item;
    } else {
      tail_->*Next = item;
    }
    tail_ = item;
  }

  void push_front(T* item) noexcept {
    item->*Prev = nullptr;
    item->*Next = head_;
    if (head_ == nullptr) {
      tail_ = item;
    } else {
      head_->*Prev = item;
    }
    head_ = item;
  }

  [[nodiscard]] T* pop_front() noexcept {
    assert(!empty());
    T* item = head_;
    head_ = item->*Next;
    if (head_ != nullptr) {
      head_->*Prev = nullptr;
    } else {
      tail_ = nullptr;
    }
    return item;
  }

  [[nodiscard]] T* pop_back() noexcept {
    assert(!empty());
    T* item = tail_;
    tail_ = item->*Prev;
    if (tail_ != nullptr) {
      tail_->*Next = nullptr;
    } else {
      head_ = nullptr;
    }
    return item;
  }

  void remove(T* item) noexcept {
    assert(!empty());
    auto* prev = item->*Prev;
    auto* next = item->*Next;
    if (prev != nullptr) {
      prev->*Next = next;
    } else {
      head_ = next;
    }
    if (next != nullptr) {
      next->*Prev = prev;
    } else {
      tail_ = prev;
    }
  }

  void append(intrusive_list other) noexcept {
    if (empty()) {
      swap(other);
    } else if (!other.empty()) {
      tail_->*Next = other.head_;
      tail_->*Next->*Prev = tail_;
      tail_ = other.tail_;
      other.head_ = nullptr;
      other.tail_ = nullptr;
    }
  }

 private:
  T* head_;
  T* tail_;
};

}  // namespace net

#endif
