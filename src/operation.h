#ifndef SRC_NET_OPERATION_H_
#define SRC_NET_OPERATION_H_
#include "net_error.hpp"
#include "op_queue.h"

namespace net {

// Base class for all operations. A function pointer is used instead of virtual
// functions to avoid the associated overhead.
class SchedulerOperation {
 public:
  // TODO: 改成start
  void complete(void* owner, const std::error_code& ec, std::size_t bytes_transferred) {
    func_(owner, this, ec, bytes_transferred);
  }

  void destroy() { func_(nullptr, this, std::error_code(), 0); }

  unsigned int taskResult() { return task_result_; }

 protected:
  using Func = void (*)(void*, SchedulerOperation*, const std::error_code&, std::size_t);

  SchedulerOperation(Func func) : next_(nullptr), func_(func), task_result_(0) {}

  // Prevents deletion through this type.
  ~SchedulerOperation() {}

  // Passed into bytes transferred.
  unsigned int task_result_;

 private:
  friend class OpQueueAccess;
  SchedulerOperation* next_;
  Func func_;
};
}  // namespace net

#endif  // SRC_NET_OPERATION_H_
