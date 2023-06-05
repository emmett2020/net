#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <thread>

#include "monotonic_clock.hpp"

using namespace std;
using namespace net;

TEST_CASE("[default constructor of time_point]", "[monotonic_clock.time_point]") {
  monotonic_clock::time_point tp;
  CHECK(tp.seconds() == 0);
  CHECK(tp.nanoseconds() == 0);
}

TEST_CASE("[check value of time_point::max()]", "[monotonic_clock.time_point.max]") {
  auto tp = monotonic_clock::time_point::max();
  CHECK(tp.seconds() == std::numeric_limits<std::int64_t>::max());
  CHECK(tp.nanoseconds() == 999'999'999);
}

TEST_CASE("[check value of time_point::min()]", "[monotonic_clock.time_point.min]") {
  auto tp = monotonic_clock::time_point::min();
  CHECK(tp.seconds() == std::numeric_limits<std::int64_t>::min());
  CHECK(tp.nanoseconds() == -999'999'999);
}

TEST_CASE("[now() should return current time]", "[monotonic_clock.time_point.now]") {
  constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;

  auto start = monotonic_clock::now();
  uint64_t start_nanoseconds = start.seconds() * nanoseconds_per_second + start.nanoseconds();
  std::this_thread::sleep_for(200ms);
  auto end = monotonic_clock::now();
  uint64_t end_nanoseconds = end.seconds() * nanoseconds_per_second + end.nanoseconds();
  // We allow an error of 200 microseconds
  CHECK(end_nanoseconds - start_nanoseconds >= 200'000'000);
  CHECK(end_nanoseconds - start_nanoseconds <= 200'200'000);
}

TEST_CASE("[Construct a time point based on the provided seconds and nanoseconds]",
          "[monotonic_clock.time_point.from_seconds_and_nanoseconds]") {
  auto tp1 = monotonic_clock::time_point::from_seconds_and_nanoseconds(1, 1);
  CHECK(tp1.seconds() == 1);
  CHECK(tp1.nanoseconds() == 1);

  auto tp2 = monotonic_clock::time_point::from_seconds_and_nanoseconds(0, 9'000'000'000);
  CHECK(tp2.seconds() == 9);
  CHECK(tp2.nanoseconds() == 0);
}
