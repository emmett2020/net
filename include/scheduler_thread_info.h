#ifndef SRC_NET_SCHEDULER_THREAD_INFO_H_
#define SRC_NET_SCHEDULER_THREAD_INFO_H_

#include "op_queue.h"
#include "thread_info_base.h"

namespace net {

struct SchedulerThreadInfo : public ThreadInfoBase {
  OpQueue<SchedulerOperation> private_op_queue;
  long private_outstanding_work;
};

}  // namespace net

#endif  // SRC_NET_SCHEDULER_THREAD_INFO_H_
