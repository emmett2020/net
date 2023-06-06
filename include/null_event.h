

#ifndef SRC_NET_NULL_EVENT_H_
#define SRC_NET_NULL_EVENT_H_

#include <sys/select.h>
#include <unistd.h>

#include "noncopyable.h"

namespace net {

class NullEvent : private NonCopyable {
 public:
  // Constructor.
  NullEvent() {}

  // Destructor.
  ~NullEvent() {}

  // Signal the event. (Retained for backward compatibility.)
  template <typename Lock>
  void signal(Lock&) {}

  // Signal all waiters.
  template <typename Lock>
  void signalAll(Lock&) {}

  // Unlock the mutex and signal one waiter.
  template <typename Lock>
  void unlockAndSignalOne(Lock&) {}

  // Unlock the mutex and signal one waiter who may destroy us.
  template <typename Lock>
  void unlockAndSignalOneForDestruction(Lock&) {}

  // If there's a waiter, unlock the mutex and signal it.
  template <typename Lock>
  bool maybeUnlockAndSignalOne(Lock&) {
    return false;
  }

  // Reset the event.
  template <typename Lock>
  void clear(Lock&) {}

  // Wait for the event to become signalled.
  template <typename Lock>
  void wait(Lock&) {
    doWait();
  }

  // Timed wait for the event to become signalled.
  template <typename Lock>
  bool waitForUsec(Lock&, long usec) {
    doWaitForUsec(usec);
    return true;
  }

 private:
  static void doWait() { ::pause(); }
  static void doWaitForUsec(long usec) {
    timeval tv;
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    ::select(0, 0, 0, 0, &tv);
  }
};

}  // namespace net

#endif  // SRC_NET_NULL_EVENT_H_
