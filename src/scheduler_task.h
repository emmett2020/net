
#ifndef SRC_NET_SCHEDULER_TASK_H_
#define SRC_NET_SCHEDULER_TASK_H_

#include "op_queue.h"
#include "operation.h"

namespace net {

// Base class for all tasks that may be run by a scheduler.
class SchedulerTask {
 public:
  // Run the task once until interrupted or events are ready to be dispatched.
  virtual void run(long usec, OpQueue<SchedulerOperation>& ops) = 0;

  // Interrupt the task.
  virtual void interrupt() = 0;

 protected:
  // Prevent deletion through this type.
  ~SchedulerTask() {}
};

}  // namespace net

#endif  // SRC_NET_SCHEDULER_TASK_H_
