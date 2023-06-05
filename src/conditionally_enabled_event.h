#ifndef SRC_NET_CONDITIONALLY_ENABLED_EVENT_H_
#define SRC_NET_CONDITIONALLY_ENABLED_EVENT_H_

#include "conditionally_enabled_mutex.h"
#include "noncopyable.h"
#include "null_event.h"
#include "posix_event.h"

namespace net {

// Mutex adapter used to conditionally enable or disable locking.
class ConditionallyEnabledEvent : private NonCopyable {
 public:
  // Constructor.
  ConditionallyEnabledEvent() {}

  // Destructor.
  ~ConditionallyEnabledEvent() {}

  // Signal the event. (Retained for backward compatibility.)
  void signal(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.signal(lock);
    }
  }

  // Signal all waiters.
  void signalAll(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.signalAll(lock);
    }
  }

  // Unlock the mutex and signal one waiter.
  void unlockAndSignalOne(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.unlockAndSignalOne(lock);
    }
  }

  // Unlock the mutex and signal one waiter who may destroy us.
  void unlockAndSignalOneForDestruction(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.unlockAndSignalOne(lock);
    }
  }

  // If there's a waiter, unlock the mutex and signal it.
  bool maybeUnlockAndSignalOne(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      return event_.maybeUnlockAndSignalOne(lock);
    } else {
      return false;
    }
  }

  // Reset the event.
  void clear(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.clear(lock);
    }
  }

  // Wait for the event to become signalled.
  void wait(ConditionallyEnabledMutex::ScopedLock& lock) {
    if (lock.mutex_.enabled_) {
      event_.wait(lock);
    } else {
      NullEvent().wait(lock);
    }
  }

  // Timed wait for the event to become signalled.
  bool waitForUsec(ConditionallyEnabledMutex::ScopedLock& lock, long usec) {
    if (lock.mutex_.enabled_) {
      return event_.waitForUsec(lock, usec);
    } else {
      return NullEvent().waitForUsec(lock, usec);
    }
  }

 private:
  PosixEvent event_;
};

}  // namespace net

#endif  // SRC_NET_CONDITIONALLY_ENABLED_EVENT_H_
