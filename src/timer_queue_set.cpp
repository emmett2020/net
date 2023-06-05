#include "timer_queue_set.h"

namespace net {

TimerQueueSet::TimerQueueSet() : first_(nullptr) {}

void TimerQueueSet::insert(TimerQueueBase* q) {
  q->next_ = first_;
  first_ = q;
}

void TimerQueueSet::erase(TimerQueueBase* q) {
  if (first_) {
    if (q == first_) {
      first_ = q->next_;
      q->next_ = nullptr;
      return;
    }

    for (TimerQueueBase* p = first_; p->next_; p = p->next_) {
      if (p->next_ == q) {
        p->next_ = q->next_;
        q->next_ = nullptr;
        return;
      }
    }
  }
}

bool TimerQueueSet::allEmpty() const {
  for (TimerQueueBase* p = first_; p; p = p->next_) {
    if (!p->empty()) {
      return false;
    }
  }
  return true;
}

long TimerQueueSet::waitDurationMsec(long max_duration) const {
  long min_duration = max_duration;
  for (TimerQueueBase* p = first_; p; p = p->next_) {
    min_duration = p->waitDurationMsec(min_duration);
  }
  return min_duration;
}

long TimerQueueSet::waitDurationUsec(long max_duration) const {
  long min_duration = max_duration;
  for (TimerQueueBase* p = first_; p; p = p->next_) {
    min_duration = p->waitDurationUsec(min_duration);
  }
  return min_duration;
}

void TimerQueueSet::getReadyTimers(OpQueue<SchedulerOperation>& ops) {
  for (TimerQueueBase* p = first_; p; p = p->next_) {
    p->getReadyTimers(ops);
  }
}

void TimerQueueSet::getAllTimers(OpQueue<SchedulerOperation>& ops) {
  for (TimerQueueBase* p = first_; p; p = p->next_) {
    p->getAllTimers(ops);
  }
}

}  // namespace net
