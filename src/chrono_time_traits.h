#ifndef SRC_NET_CHRONO_TIME_TRAITS_H_
#define SRC_NET_CHRONO_TIME_TRAITS_H_

#include <chrono>
#include <cstdint>

namespace net {

// Helper template to compute the greatest common divisor.
template <int64_t v1, int64_t v2>
struct Gcd {
  enum { value = Gcd<v2, v1 % v2>::value };
};

template <int64_t v1>
struct Gcd<v1, 0> {
  enum { value = v1 };
};

// Adapts std::chrono clocks for use with a deadline timer.
template <typename Clock, typename WaitTraits1>
struct ChronoTimeTraits {
  // The clock type.
  using ClockType = Clock;

  // The duration type of the clock.
  using DurationType = typename ClockType::duration;

  // The time point type of the clock.
  using TimeType = typename ClockType::time_point;

  // The period of the clock.
  using PeriodType = typename DurationType::period;

  // Get the current time.
  static TimeType now() { return ClockType::now(); }

  // Add a duration to a time.
  static TimeType add(const TimeType& t, const DurationType& d) {
    const TimeType epoch;
    if (t >= epoch) {
      if ((TimeType::max)() - t < d) {
        return (TimeType::max)();
      }
    } else {
      // t < epoch
      if (-(t - (TimeType::min)()) > d) {
        return (TimeType::min)();
      }
    }

    return t + d;
  }

  // Subtract one time from another.
  static DurationType subtract(const TimeType& t1, const TimeType& t2) {
    const TimeType epoch;
    if (t1 >= epoch) {
      if (t2 >= epoch) {
        return t1 - t2;
      } else if (t2 == (TimeType::min)()) {
        return (DurationType::max)();
      } else if ((TimeType::max)() - t1 < epoch - t2) {
        return (DurationType::max)();
      } else {
        return t1 - t2;
      }
    } else {
      // t1 < epoch
      if (t2 < epoch) {
        return t1 - t2;
      } else if (t1 == (TimeType::min)()) {
        return (DurationType::min)();
      } else if ((TimeType::max)() - t2 < epoch - t1) {
        return (DurationType::min)();
      } else {
        return -(t2 - t1);
      }
    }
  }

  // Test whether one time is less than another.
  static bool lessThan(const TimeType& t1, const TimeType& t2) { return t1 < t2; }

  // Implement just enough of the posix_time::time_duration interface to supply
  // what the timer_queue requires.
  class PosixTimeDuration {
   public:
    explicit PosixTimeDuration(const DurationType& d) : d_(d) {}

    int64_t ticks() const { return d_.count(); }

    int64_t totalSeconds() const { return durationCast<1, 1>(); }

    int64_t totalMilliseconds() const { return durationCast<1, 1000>(); }

    int64_t totalMicroseconds() const { return durationCast<1, 1000000>(); }

   private:
    template <int64_t Num, int64_t Den>
    int64_t durationCast() const {
      const int64_t num1 = PeriodType::num / Gcd<PeriodType::num, Num>::value;
      const int64_t num2 = Num / Gcd<PeriodType::num, Num>::value;

      const int64_t den1 = PeriodType::den / Gcd<PeriodType::den, Den>::value;
      const int64_t den2 = Den / Gcd<PeriodType::den, Den>::value;

      const int64_t num = num1 * den2;
      const int64_t den = num2 * den1;

      if (num == 1 && den == 1) {
        return ticks();
      } else if (num != 1 && den == 1) {
        return ticks() * num;
      } else if (num == 1 && PeriodType::den != 1) {
        return ticks() / den;
      } else {
        return ticks() * num / den;
      }
    }

    DurationType d_;
  };

  // Convert to POSIX duration type.
  static PosixTimeDuration toPosixDuration(const DurationType& d) {
    return PosixTimeDuration(WaitTraits1::toWaitDuration(d));
  }
};

}  // namespace net

#endif  // SRC_NET_CHRONO_TIME_TRAITS_H_
