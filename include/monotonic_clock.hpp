/*
 * Copyright (c) 2023 Runner-2019
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MONOTONIC_CLOCK_HPP_
#define MONOTONIC_CLOCK_HPP_

#include <chrono>  // NOLINT
#include <cstdint>
#include <limits>
#include <ratio>  // NOLINT

namespace net {

// A std::chrono-like clock type that wraps the Linux CLOCK_MONOTONIC clock
// type. The time points of this clock cannot decrease as physical time moves
// forward and the time between ticks of this clock is constant.
class monotonic_clock {
 public:
  // An arithmetic type representing the number of ticks of the duration.
  using rep = int64_t;

  // Representing the tick period of the duration.
  using ratio = std::ratio<1, 10'000'000>;  // 100ns

  // Measure the time since epoch.
  using duration = std::chrono::duration<rep, ratio>;

  // Steady clock flag, always true.
  static constexpr bool is_steady = true;

  class time_point {
   public:
    // measure the time since epoch
    using duration = monotonic_clock::duration;

    // Default constructor.
    constexpr time_point() noexcept : seconds_(0), nanoseconds_(0) {}

    // Copy constructor.
    constexpr time_point(const time_point&) noexcept = default;

    // Copy assign.
    constexpr time_point& operator=(const time_point&) noexcept = default;

    // Move constructor.
    constexpr time_point(time_point&&) noexcept = default;

    // Move assign.
    constexpr time_point& operator=(time_point&&) noexcept = default;

    // Return the maximum value of time_point.
    static constexpr time_point max() noexcept {
      time_point tp;
      tp.seconds_ = std::numeric_limits<int64_t>::max();
      tp.nanoseconds_ = 999'999'999;
      return tp;
    }

    // Return the minimum value of time_point.
    static constexpr time_point min() noexcept {
      time_point tp;
      tp.seconds_ = std::numeric_limits<int64_t>::min();
      tp.nanoseconds_ = -999'999'999;
      return tp;
    }

    // Construct a time point based on the provided seconds and nanoseconds
    static time_point from_seconds_and_nanoseconds(
        int64_t seconds, int64_t nanoseconds) noexcept;

    // The seconds part.
    constexpr int64_t seconds() const noexcept { return seconds_; }

    // The nanoseconds part.
    constexpr int64_t nanoseconds() const noexcept { return nanoseconds_; }

    template <typename Rep, typename Ratio>
    time_point& operator+=(const std::chrono::duration<Rep, Ratio>& d) noexcept;

    template <typename Rep, typename Ratio>
    time_point& operator-=(const std::chrono::duration<Rep, Ratio>& d) noexcept;

    friend duration operator-(const time_point& a,
                              const time_point& b) noexcept {
      return duration((a.seconds_ - b.seconds_) * 10'000'000 +
                      (a.nanoseconds_ - b.nanoseconds_) / 100);
    }

    template <typename Rep, typename Ratio>
    friend time_point operator+(const time_point& a,
                                std::chrono::duration<Rep, Ratio> d) noexcept {
      time_point tp = a;
      tp += d;
      return tp;
    }

    template <typename Rep, typename Ratio>
    friend time_point operator-(const time_point& a,
                                std::chrono::duration<Rep, Ratio> d) noexcept {
      time_point tp = a;
      tp -= d;
      return tp;
    }

    friend bool operator==(const time_point& a, const time_point& b) noexcept {
      return a.seconds_ == b.seconds_ && a.nanoseconds_ == b.nanoseconds_;
    }

    friend bool operator!=(const time_point& a, const time_point& b) noexcept {
      return !(a == b);
    }

    friend bool operator<(const time_point& a, const time_point& b) noexcept {
      return (a.seconds_ == b.seconds_) ? (a.nanoseconds_ < b.nanoseconds_)
                                        : (a.seconds_ < b.seconds_);
    }

    friend bool operator>(const time_point& a, const time_point& b) noexcept {
      return b < a;
    }

    friend bool operator<=(const time_point& a, const time_point& b) noexcept {
      return !(b < a);
    }

    friend bool operator>=(const time_point& a, const time_point& b) noexcept {
      return !(a < b);
    }

   private:
    // Transfer the extra nanoseconds to seconds
    constexpr void normalize() noexcept;

    int64_t seconds_;
    int64_t nanoseconds_;
  };

  // Get the current time.
  static time_point now() noexcept;
};

inline monotonic_clock::time_point
monotonic_clock::time_point::from_seconds_and_nanoseconds(
    int64_t seconds, int64_t nanoseconds) noexcept {
  time_point tp;
  tp.seconds_ = seconds;
  tp.nanoseconds_ = nanoseconds;
  tp.normalize();
  return tp;
}

template <typename Rep, typename Ratio>
monotonic_clock::time_point& monotonic_clock::time_point::operator+=(
    const std::chrono::duration<Rep, Ratio>& d) noexcept {
  const auto whole_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(d);
  const auto remainder_nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(d - whole_seconds);
  seconds_ += whole_seconds.count();
  nanoseconds_ += remainder_nanoseconds.count();
  normalize();
  return *this;
}

template <typename Rep, typename Ratio>
monotonic_clock::time_point& monotonic_clock::time_point::operator-=(
    const std::chrono::duration<Rep, Ratio>& d) noexcept {
  const auto whole_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(d);
  const auto remainder_nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(d - whole_seconds);
  seconds_ -= whole_seconds.count();
  nanoseconds_ -= remainder_nanoseconds.count();
  normalize();
  return *this;
}

inline constexpr void monotonic_clock::time_point::normalize() noexcept {
  constexpr int64_t nanoseconds_per_second = 1'000'000'000;
  auto extra_seconds = nanoseconds_ / nanoseconds_per_second;
  seconds_ += extra_seconds;
  nanoseconds_ -= extra_seconds * nanoseconds_per_second;
  if (seconds_ < 0 && nanoseconds_ > 0) {
    seconds_ += 1;
    nanoseconds_ -= nanoseconds_per_second;
  } else if (seconds_ > 0 && nanoseconds_ < 0) {
    seconds_ -= 1;
    nanoseconds_ += nanoseconds_per_second;
  }
}

inline monotonic_clock::time_point monotonic_clock::now() noexcept {
  timespec ts;
  ::clock_gettime(CLOCK_MONOTONIC, &ts);
  return time_point::from_seconds_and_nanoseconds(ts.tv_sec, ts.tv_nsec);
}

}  // namespace net

#endif  // MONOTONIC_CLOCK_HPP_
