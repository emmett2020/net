#ifndef SRC_NET_POSIX_SIGNAL_BLOCKER_H_
#define SRC_NET_POSIX_SIGNAL_BLOCKER_H_

#include <pthread.h>
#include <signal.h>

#include <csignal>

#include "noncopyable.h"

namespace net {

class PosixSignalBlocker : private NonCopyable {
 public:
  // Constructor blocks all signals for the calling thread.
  PosixSignalBlocker() : blocked_(false) {
    sigset_t new_mask;
    sigfillset(&new_mask);
    blocked_ = (pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask_) == 0);
  }

  // Destructor restores the previous signal mask.
  ~PosixSignalBlocker() {
    if (blocked_) {
      pthread_sigmask(SIG_SETMASK, &old_mask_, 0);
    }
  }

  // Block all signals for the calling thread.
  void block() {
    if (!blocked_) {
      sigset_t new_mask;
      sigfillset(&new_mask);
      blocked_ = (pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask_) == 0);
    }
  }

  // Restore the previous signal mask.
  void unblock() {
    if (blocked_) blocked_ = (pthread_sigmask(SIG_SETMASK, &old_mask_, 0) != 0);
  }

 private:
  // Have signals been blocked.
  bool blocked_;

  // The previous signal mask.
  sigset_t old_mask_;
};

using SignalBlocker = PosixSignalBlocker;

}  // namespace net

#endif  // SRC_NET_POSIX_SIGNAL_BLOCKER_H_
