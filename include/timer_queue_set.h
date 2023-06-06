#ifndef SRC_NET_TIMER_QUEUE_SET_H_
#define SRC_NET_TIMER_QUEUE_SET_H_

#include "timer_queue_base.h"

namespace net {

class TimerQueueSet {
 public:
  // Constructor.
  TimerQueueSet();

  // Add a timer queue to the set.
  void insert(TimerQueueBase* q);

  // Remove a timer queue from the set.
  void erase(TimerQueueBase* q);

  // Determine whether all queues are empty.
  bool allEmpty() const;

  // Get the wait duration in milliseconds.
  long waitDurationMsec(long max_duration) const;

  // Get the wait duration in microseconds.
  long waitDurationUsec(long max_duration) const;

  // Dequeue all ready timers.
  void getReadyTimers(OpQueue<SchedulerOperation>& ops);

  // Dequeue all timers.
  void getAllTimers(OpQueue<SchedulerOperation>& ops);

 private:
  TimerQueueBase* first_;
};

}  // namespace net
#endif  // SRC_NET_TIMER_QUEUE_SET_H_
