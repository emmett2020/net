#ifndef SRC_NET_CONDITIONALLY_ENABLED_MUTEX_H_
#define SRC_NET_CONDITIONALLY_ENABLED_MUTEX_H_

#include "noncopyable.h"
#include "posix_mutex.h"

namespace net {

// TODO: 切换成std::ScopedLock
// TODO: 这里编译期优化，避免多线程情况下，增加一次无谓的if判断
// Mutex adapter used to conditionally enable or disable locking.
class ConditionallyEnabledMutex : private NonCopyable {
 public:
  // Helper class to lock and unlock a mutex automatically.
  class ScopedLock : private NonCopyable {
   public:
    // Tag type used to distinguish constructors.
    enum AdoptLock { kAdoptLock };

    // Constructor adopts a lock that is already held.
    ScopedLock(ConditionallyEnabledMutex& m, AdoptLock) : mutex_(m), locked_(m.enabled_) {}

    // Constructor acquires the lock.
    explicit ScopedLock(ConditionallyEnabledMutex& m) : mutex_(m) {
      if (m.enabled_) {
        mutex_.mutex_.lock();
        locked_ = true;
      } else {
        locked_ = false;
      }
    }

    // Destructor releases the lock.
    ~ScopedLock() {
      if (locked_) {
        mutex_.mutex_.unlock();
      }
    }

    // Explicitly acquire the lock.
    void lock() {
      if (mutex_.enabled_ && !locked_) {
        mutex_.mutex_.lock();
        locked_ = true;
      }
    }

    // Explicitly release the lock.
    void unlock() {
      if (locked_) {
        mutex_.unlock();
        locked_ = false;
      }
    }

    // Test whether the lock is held.
    bool locked() const { return locked_; }

    // Get the underlying mutex.
    PosixMutex& mutex() { return mutex_.mutex_; }

   private:
    friend class ConditionallyEnabledEvent;
    ConditionallyEnabledMutex& mutex_;
    bool locked_;
  };

  // Constructor.
  explicit ConditionallyEnabledMutex(bool enabled) : enabled_(enabled) {}

  // Destructor.
  ~ConditionallyEnabledMutex() {}

  // Determine whether locking is enabled.
  bool enabled() const { return enabled_; }

  // Lock the mutex.
  void lock() {
    if (enabled_) {
      mutex_.lock();
    }
  }

  // Unlock the mutex.
  void unlock() {
    if (enabled_) {
      mutex_.unlock();
    }
  }

 private:
  friend class ScopedLock;
  friend class ConditionallyEnabledEvent;
  PosixMutex mutex_;
  const bool enabled_;
};

}  // namespace net

#endif  // SRC_NET_CONDITIONALLY_ENABLED_MUTEX_H_
