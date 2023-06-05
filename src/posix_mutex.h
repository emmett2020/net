#ifndef SRC_NET_POSIX_MUTEX_H_
#define SRC_NET_POSIX_MUTEX_H_

#include <pthread.h>

#include "net_error.h"
#include "noncopyable.h"

namespace net {

class PosixEvent;

class PosixMutex : private NonCopyable {
 public:
  // Constructor.
  PosixMutex() {
    int error = ::pthread_mutex_init(&mutex_, nullptr);
    if (error) {
      std::error_code ec(error, GetNetErrorCategory());
      throw std::system_error(ec, "mutex");
    }
  }

  // Destructor.
  ~PosixMutex() {
    // Ignore EBUSY.
    ::pthread_mutex_destroy(&mutex_);
  }

  // Lock the mutex.
  void lock() {
    // Ignore EINVAL.
    ::pthread_mutex_lock(&mutex_);
  }

  // Unlock the mutex.
  void unlock() {
    // Ignore EINVAL.
    ::pthread_mutex_unlock(&mutex_);
  }

 private:
  friend class PosixEvent;
  ::pthread_mutex_t mutex_;
};

}  // namespace net

#endif  // SRC_NET_POSIX_MUTEX_H_
