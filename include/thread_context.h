#ifndef NET_THREAD_CONTEXT_H_
#define NET_THREAD_CONTEXT_H_

#include "call_stack.h"
#include "thread_info_base.h"

namespace net {

// Base class for things that manage threads (scheduler, win_iocp_io_context).
class ThreadContext {
 public:
  // Obtain a pointer to the top of the thread call stack. Returns null when
  // not running inside a thread context.
  static ThreadInfoBase* topOfThreadCallStack();

 protected:
  // Per-thread call stack to track the state of each thread in the context.
  using ThreadCallStack = CallStack<ThreadContext, ThreadInfoBase>;
};

inline ThreadInfoBase* ThreadContext::topOfThreadCallStack() { return ThreadCallStack::top(); }

}  // namespace net

#endif  // NET_THREAD_CONTEXT_H_
