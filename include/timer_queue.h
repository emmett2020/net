#ifndef SRC_NET_TIMER_QUEUE_H_
#define SRC_NET_TIMER_QUEUE_H_

#include <vector>

#include "chrono_time_traits.h"
#include "error.h"
#include "op_queue.h"
#include "timer_queue_base.h"
#include "wait_op.h"

namespace net {

template <typename TimeTraits>
class TimerQueue : public TimerQueueBase {
 public:
  // The time type.
  using TimeType = typename TimeTraits::TimeType;

  // The duration type.
  using DurationType = typename TimeTraits::DurationType;

  // Per-timer data.
  class PerTimerData {
   public:
    PerTimerData()
        : heap_index_((std::numeric_limits<std::size_t>::max)()), next_(nullptr), prev_(nullptr) {}

   private:
    friend class TimerQueue;

    // The operations waiting on the timer.
    OpQueue<WaitOp> op_queue_;

    // The index of the timer in the heap.
    std::size_t heap_index_;

    // Pointers to adjacent timers in a linked list.
    PerTimerData* next_;
    PerTimerData* prev_;
  };

  // Constructor.
  TimerQueue() : timers_(), heap_() {}

  // Add a new timer to the queue. Returns true if this is the timer that is
  // earliest in the queue, in which case the reactor's event de-multiplexing
  // function call may need to be interrupted and restarted.
  bool EnqueueTimer(const TimeType& time, PerTimerData& timer, WaitOp* op) {
    // Enqueue the timer object.
    if (timer.prev_ == nullptr && &timer != timers_) {
      if (this->isPositiveInfinity(time)) {
        // No heap entry is required for timers that never expire.
        timer.heap_index_ = (std::numeric_limits<std::size_t>::max)();
      } else {
        // Put the new timer at the correct position in the heap. This is kDone
        // first since push_back() can throw due to allocation failure.
        timer.heap_index_ = heap_.size();
        HeapEntry entry = {time, &timer};
        heap_.push_back(entry);
        upHeap(heap_.size() - 1);
      }

      // Insert the new timer into the linked list of active timers.
      timer.next_ = timers_;
      timer.prev_ = nullptr;
      if (timers_) {
        timers_->prev_ = &timer;
      }
      timers_ = &timer;
    }

    // Enqueue the individual timer SchedulerOperation.
    timer.op_queue_.push(op);

    // Interrupt reactor only if newly added timer is first to expire.
    return timer.heap_index_ == 0 && timer.op_queue_.front() == op;
  }

  // Whether there are no timers in the queue.
  virtual bool empty() const { return timers_ == nullptr; }

  // Get the time for the timer that is earliest in the queue.
  virtual long waitDurationMsec(long max_duration) const {
    if (heap_.empty()) {
      return max_duration;
    }

    return this->toMsec(
        TimeTraits::toPosixDuration(TimeTraits::subtract(heap_[0].time_, TimeTraits::now())),
        max_duration);
  }

  // Get the time for the timer that is earliest in the queue.
  virtual long waitDurationUsec(long max_duration) const {
    if (heap_.empty()) {
      return max_duration;
    }

    return this->toUsec(
        TimeTraits::toPosixDuration(TimeTraits::subtract(heap_[0].time_, TimeTraits::now())),
        max_duration);
  }

  // Dequeue all timers not later than the current time.
  virtual void getReadyTimers(OpQueue<SchedulerOperation>& ops) {
    if (!heap_.empty()) {
      const TimeType now = TimeTraits::now();
      while (!heap_.empty() && !TimeTraits::lessThan(now, heap_[0].time_)) {
        PerTimerData* timer = heap_[0].timer_;
        while (WaitOp* op = timer->op_queue_.front()) {
          timer->op_queue_.pop();
          op->ec_ = std::error_code();
          ops.push(op);
        }
        removeTimer(*timer);
      }
    }
  }

  // Dequeue all timers.
  virtual void getAllTimers(OpQueue<SchedulerOperation>& ops) {
    while (timers_) {
      PerTimerData* timer = timers_;
      timers_ = timers_->next_;
      ops.push(timer->op_queue_);
      timer->next_ = nullptr;
      timer->prev_ = nullptr;
    }

    heap_.clear();
  }

  // Cancel and dequeue operations for the given timer.
  std::size_t cancelTimer(PerTimerData& timer, OpQueue<SchedulerOperation>& ops,
                          std::size_t max_cancelled = (std::numeric_limits<std::size_t>::max)()) {
    std::size_t num_cancelled = 0;
    if (timer.prev_ != nullptr || &timer == timers_) {
      while (WaitOp* op = (num_cancelled != max_cancelled) ? timer.op_queue_.front() : 0) {
        op->ec_ = NetError::kOperationAborted;
        timer.op_queue_.pop();
        ops.push(op);
        ++num_cancelled;
      }
      if (timer.op_queue_.empty()) {
        removeTimer(timer);
      }
    }
    return num_cancelled;
  }

  // Cancel and dequeue a specific SchedulerOperation for the given timer.
  void cancelTimerByKey(PerTimerData* timer, OpQueue<SchedulerOperation>& ops,
                        void* cancellation_key) {
    if (timer->prev_ != 0 || timer == timers_) {
      OpQueue<WaitOp> other_ops;
      while (WaitOp* op = timer->op_queue_.front()) {
        timer->op_queue_.pop();
        if (op->cancellation_key_ == cancellation_key) {
          op->ec_ = NetError::kOperationAborted;
          ops.push(op);
        } else {
          other_ops.push(op);
        }
      }
      timer->op_queue_.push(other_ops);
      if (timer->op_queue_.empty()) removeTimer(*timer);
    }
  }

  // Move operations from one timer to another, empty timer.
  void moveTimer(PerTimerData& target, PerTimerData& source) {
    target.op_queue_.push(source.op_queue_);

    target.heap_index_ = source.heap_index_;
    source.heap_index_ = (std::numeric_limits<std::size_t>::max)();

    if (target.heap_index_ < heap_.size()) {
      heap_[target.heap_index_].timer_ = &target;
    }

    if (timers_ == &source) {
      timers_ = &target;
    }
    if (source.prev_) {
      source.prev_->next_ = &target;
    }
    if (source.next_) {
      source.next_->prev_ = &target;
    }

    target.next_ = source.next_;
    target.prev_ = source.prev_;
    source.next_ = nullptr;
    source.prev_ = nullptr;
  }

 private:
  // Move the item at the given index up the heap to its correct position.
  void upHeap(std::size_t index) {
    while (index > 0) {
      std::size_t parent = (index - 1) / 2;
      if (!TimeTraits::lessThan(heap_[index].time_, heap_[parent].time_)) {
        break;
      }
      swapHeap(index, parent);
      index = parent;
    }
  }

  // Move the item at the given index down the heap to its correct position.
  void downHeap(std::size_t index) {
    std::size_t child = index * 2 + 1;
    while (child < heap_.size()) {
      std::size_t min_child = (child + 1 == heap_.size()
                               || TimeTraits::lessThan(heap_[child].time_, heap_[child + 1].time_))
                                  ? child
                                  : child + 1;
      if (TimeTraits::lessThan(heap_[index].time_, heap_[min_child].time_)) {
        break;
      }
      swapHeap(index, min_child);
      index = min_child;
      child = index * 2 + 1;
    }
  }

  // Swap two entries in the heap.
  void swapHeap(std::size_t index1, std::size_t index2) {
    HeapEntry tmp = heap_[index1];
    heap_[index1] = heap_[index2];
    heap_[index2] = tmp;
    heap_[index1].timer_->heap_index_ = index1;
    heap_[index2].timer_->heap_index_ = index2;
  }

  // Remove a timer from the heap and list of timers.
  void removeTimer(PerTimerData& timer) {
    // Remove the timer from the heap.
    std::size_t index = timer.heap_index_;
    if (!heap_.empty() && index < heap_.size()) {
      if (index == heap_.size() - 1) {
        timer.heap_index_ = (std::numeric_limits<std::size_t>::max)();
        heap_.pop_back();
      } else {
        swapHeap(index, heap_.size() - 1);
        timer.heap_index_ = (std::numeric_limits<std::size_t>::max)();
        heap_.pop_back();
        if (index > 0 && TimeTraits::lessThan(heap_[index].time_, heap_[(index - 1) / 2].time_)) {
          upHeap(index);
        } else {
          downHeap(index);
        }
      }
    }

    // Remove the timer from the linked list of active timers.
    if (timers_ == &timer) {
      timers_ = timer.next_;
    }
    if (timer.prev_) {
      timer.prev_->next_ = timer.next_;
    }
    if (timer.next_) {
      timer.next_->prev_ = timer.prev_;
    }
    timer.next_ = nullptr;
    timer.prev_ = nullptr;
  }

  // Determine if the specified absolute time is positive infinity.
  template <typename TimeType>
  static bool isPositiveInfinity(const TimeType&) {
    return false;
  }

  // We won't use boost.
  // template <typename T, typename TimeSystem>
  // static bool isPositiveInfinity(const boost::date_time::base_time<T, TimeSystem>& time) {
  //   return time.is_pos_infinity();
  // }

  // Helper function to convert a duration into milliseconds.
  template <typename Duration>
  long toMsec(const Duration& d, long max_duration) const {
    if (d.ticks() <= 0) {
      return 0;
    }
    int64_t msec = d.totalMilliseconds();
    if (msec == 0) {
      return 1;
    }
    if (msec > max_duration) {
      return max_duration;
    }
    return static_cast<long>(msec);
  }

  // Helper function to convert a duration into microseconds.
  template <typename Duration>
  long toUsec(const Duration& d, long max_duration) const {
    if (d.ticks() <= 0) {
      return 0;
    }
    int64_t usec = d.totalMicroseconds();
    if (usec == 0) {
      return 1;
    }
    if (usec > max_duration) {
      return max_duration;
    }
    return static_cast<long>(usec);
  }

  // The head of a linked list of all active timers.
  PerTimerData* timers_;

  struct HeapEntry {
    // The time when the timer should fire.
    TimeType time_;

    // The associated timer with enqueued operations.
    PerTimerData* timer_;
  };

  // The heap of timers, with the earliest timer at the front.
  std::vector<HeapEntry> heap_;
};

}  // namespace net

#endif  // SRC_NET_TIMER_QUEUE_H_
