#ifndef SRC_NET_WAIT_TRAITS_H_
#define SRC_NET_WAIT_TRAITS_H_

namespace net {

// Wait traits suitable for use with the basic_waitable_timer class template.
template <typename Clock>
struct WaitTraits {
  // Convert a clock duration into a duration used for waiting.
  static typename Clock::duration toWaitDuration(const typename Clock::duration& d) { return d; }

  // Convert a clock duration into a duration used for waiting.
  static typename Clock::duration toWaitDuration(const typename Clock::time_point& t) {
    typename Clock::time_point now = Clock::now();
    if (now + (Clock::duration::max)() < t) {
      return (Clock::duration::max)();
    }
    if (now + (Clock::duration::min)() > t) {
      return (Clock::duration::min)();
    }
    return t - now;
  }
};

}  // namespace net

#endif  // SRC_NET_WAIT_TRAITS_H_
