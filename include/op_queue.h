#ifndef SRC_NET_OP_QUEUE_H_
#define SRC_NET_OP_QUEUE_H_

// #include "noncopyable.h"

namespace net {

template <typename Operation>
class OpQueue;

class OpQueueAccess {
 public:
  template <typename Operation>
  static Operation* next(Operation* o) {
    return static_cast<Operation*>(o->next_);
  }

  template <typename Operation1, typename Operation2>
  static void next(Operation1*& o1, Operation2* o2) {
    o1->next_ = o2;
  }

  template <typename Operation>
  static void destroy(Operation* o) {
    o->destroy();
  }

  template <typename Operation>
  static Operation*& front(OpQueue<Operation>& q) {
    return q.front_;
  }

  template <typename Operation>
  static Operation*& back(OpQueue<Operation>& q) {
    return q.back_;
  }
};

// TODO: noncopyable
template <typename Operation>
class OpQueue {
 public:
  // Constructor.
  OpQueue() : front_(nullptr), back_(nullptr) {}

  // Destructor destroys all operations.
  ~OpQueue() {
    while (Operation* op = front_) {
      pop();
      OpQueueAccess::destroy(op);
    }
  }

  // Get the operation at the front of the queue.
  Operation* front() { return front_; }

  // Pop an operation from the front of the queue.
  void pop() {
    if (front_) {
      Operation* tmp = front_;
      front_ = OpQueueAccess::next(front_);
      if (front_ == nullptr) {
        back_ = nullptr;
      }
      OpQueueAccess::next(tmp, static_cast<Operation*>(nullptr));
    }
  }

  // Push an operation on to the back of the queue.
  void push(Operation* h) {
    OpQueueAccess::next(h, static_cast<Operation*>(nullptr));
    if (back_) {
      OpQueueAccess::next(back_, h);
      back_ = h;
    } else {
      front_ = back_ = h;
    }
  }

  // Push all operations from another queue on to the back of the queue. The
  // source queue may contain operations of a derived type.
  template <typename OtherOperation>
  void push(OpQueue<OtherOperation>& q) {
    if (Operation* other_front = OpQueueAccess::front(q)) {
      if (back_) {
        OpQueueAccess::next(back_, other_front);
      } else {
        front_ = other_front;
      }
      back_ = OpQueueAccess::back(q);
      OpQueueAccess::front(q) = nullptr;
      OpQueueAccess::back(q) = nullptr;
    }
  }

  // Whether the queue is empty.
  bool empty() const { return front_ == nullptr; }

  // Test whether an operation is already enqueued.
  bool isEnqueued(Operation* o) const { return OpQueueAccess::next(o) != nullptr || back_ == o; }

 private:
  friend class OpQueueAccess;

  // The front of the queue.
  Operation* front_;

  // The back of the queue.
  Operation* back_;
};

}  // namespace net

#endif  // SRC_NET_OP_QUEUE_H_
