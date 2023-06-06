#ifndef SRC_NET_REACTOR_OP_H_
#define SRC_NET_REACTOR_OP_H_

#include "operation.h"
#include "system_error"

namespace net {

class ReactorOp : public SchedulerOperation {
 public:
  // The error code to be passed to the completion handler.
  std::error_code ec_;

  // The operation key used for targeted cancellation.
  void* cancellation_key_;

  // TODO: 这里是否需要改名
  // The number of bytes transferred, to be passed to the completion handler.
  std::size_t bytes_transferred_;

  // TODO: 这里要改成enum class，所有的enum都要改成enum class
  // Status returned by perform function. May be used to decide whether it is
  // worth performing more operations on the descriptor immediately.
  enum Status { kNotDone, kDone, kDoneAndExhausted };

  // Perform the operation. Returns true if it is finished.
  Status perform() { return perform_func_(this); }

 protected:
  using PerformFunc = Status (*)(ReactorOp*);

  // TODO: 这个ec没必要让子类去传递进来吧
  ReactorOp(const std::error_code& success_ec, PerformFunc perform_func, Func complete_func)
      : SchedulerOperation(complete_func),
        ec_(success_ec),
        cancellation_key_(nullptr),
        bytes_transferred_(0),
        perform_func_(perform_func) {}

 private:
  PerformFunc perform_func_;
};

}  // namespace net

#endif  // SRC_NET_REACTOR_OP_H_
