

#ifndef SRC_NET_POSIX_EVENT_H_
#define SRC_NET_POSIX_EVENT_H_

#include <pthread.h>
#include <cassert>

#include "noncopyable.h"

namespace net {

class PosixEvent : private NonCopyable {
 public:
  // Constructor.
  PosixEvent() : state_(0) {
    ::pthread_condattr_t attr;
    int error = ::pthread_condattr_init(&attr);
    if (error == 0) {
      error = ::pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
      if (error == 0) {
        error = ::pthread_cond_init(&cond_, &attr);
      }
      ::pthread_condattr_destroy(&attr);
    }

    std::error_code ec(error, GetNetErrorCategory());
    if (ec.value() != 0) {
      throw std::system_error(ec, "event");
    }
  }

  // Destructor.
  ~PosixEvent() { ::pthread_cond_destroy(&cond_); }

  // ! Lock enforces an interface not supported by the std::scope_lock,
  // ! so Lock & PosixEvent can only be used by conditionally_enabled_mutex::scoped_lock.
  // Signal the event. (Retained for backward compatibility.)
  template <typename Lock>
  void signal(Lock& lock) {
    this->signalAll(lock);
  }

  // Signal all waiters.
  template <typename Lock>
  void signalAll(Lock& lock) {
    assert(lock.locked());
    state_ |= 1;

    // Ignore EINVAL.
    ::pthread_cond_broadcast(&cond_);
  }

  // Unlock the mutex and signal one waiter.
  template <typename Lock>
  void unlockAndSignalOne(Lock& lock) {
    assert(lock.locked());
    state_ |= 1;
    bool have_waiters = (state_ > 1);
    lock.unlock();
    if (have_waiters) {
      // Ignore EINVAL.
      ::pthread_cond_signal(&cond_);
    }
  }

  // Unlock the mutex and signal one waiter who may destroy us.
  template <typename Lock>
  void unlockAndSignalOneForDestruction(Lock& lock) {
    assert(lock.locked());
    state_ |= 1;
    bool have_waiters = (state_ > 1);
    if (have_waiters) {
      // Ignore EINVAL.
      ::pthread_cond_signal(&cond_);
    }
    lock.unlock();
  }

  // If there's a waiter, unlock the mutex and signal it.
  template <typename Lock>
  bool maybeUnlockAndSignalOne(Lock& lock) {
    assert(lock.locked());
    state_ |= 1;
    if (state_ > 1) {
      lock.unlock();
      // Ignore EINVAL.
      ::pthread_cond_signal(&cond_);
      return true;
    }
    return false;
  }

  // Reset the event.
  template <typename Lock>
  void clear(Lock& lock) {
    assert(lock.locked());
    state_ &= ~std::size_t(1);
  }

  // Wait for the event to become signalled.
  template <typename Lock>
  void wait(Lock& lock) {
    assert(lock.locked());
    while ((state_ & 1) == 0) {
      state_ += 2;
      ::pthread_cond_wait(&cond_, &lock.mutex().mutex_);  // Ignore EINVAL.
      state_ -= 2;
    }
  }

  // Timed wait for the event to become signalled.
  template <typename Lock>
  bool waitForUsec(Lock& lock, long usec) {
    assert(lock.locked());
    if ((state_ & 1) == 0) {
      state_ += 2;
      timespec ts;

      if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        ts.tv_sec += usec / 1000000;
        ts.tv_nsec += (usec % 1000000) * 1000;
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec = ts.tv_nsec % 1000000000;
        ::pthread_cond_timedwait(&cond_, &lock.mutex().mutex_,
                                 &ts);  // Ignore EINVAL.
      }
      state_ -= 2;
    }
    return (state_ & 1) != 0;
  }

 private:
  ::pthread_cond_t cond_;

  // The lowest bit of state_ indicates whether it can be awakened (or get Lock) (1 represents for
  // could be awakened). All the other bits of state_ together represent the number of threads
  // currently blocked. Since we're recording from the penultimate position, the final number of
  // threads is equal to state_ divided by 2. And there must be no remainder.
  std::size_t state_;
};

}  // namespace net

#endif  // SRC_NET_POSIX_EVENT_H_
