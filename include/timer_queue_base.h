#ifndef SRC_NET_TIMER_QUEUE_BASE_H_
#define SRC_NET_TIMER_QUEUE_BASE_H_

#include "op_queue.h"
#include "operation.h"

namespace net {

// TODO: noncopyable
class TimerQueueBase {
 public:
  // Constructor.
  TimerQueueBase() : next_(nullptr) {}

  // Destructor.
  virtual ~TimerQueueBase() {}

  // Whether there are no timers in the queue.
  virtual bool empty() const = 0;

  // Get the time to wait until the next timer.
  virtual long waitDurationMsec(long max_duration) const = 0;

  // Get the time to wait until the next timer.
  virtual long waitDurationUsec(long max_duration) const = 0;

  // Dequeue all ready timers.
  virtual void getReadyTimers(OpQueue<SchedulerOperation>& ops) = 0;

  // Dequeue all timers.
  virtual void getAllTimers(OpQueue<SchedulerOperation>& ops) = 0;

 private:
  friend class TimerQueueSet;

  // Next timer queue in the set.
  TimerQueueBase* next_;
};
}  // namespace net

#endif  // SRC_NET_TIMER_QUEUE_BASE_H_
