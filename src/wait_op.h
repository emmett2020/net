
#ifndef SRC_NET_WAIT_OP_H_
#define SRC_NET_WAIT_OP_H_

#include "operation.h"

namespace net {

class WaitOp : public SchedulerOperation {
 public:
  // The error code to be passed to the completion handler.
  std::error_code ec_;

  // The SchedulerOperation key used for targeted cancellation.
  void* cancellation_key_;

 protected:
  WaitOp(Func func) : SchedulerOperation(func), cancellation_key_(nullptr) {}
};

}  // namespace net

#endif  // SRC_NET_WAIT_OP_H_
