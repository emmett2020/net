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
#include <stop_token>

#include <chrono>    // NOLINT
#include <concepts>  // NOLINT
#include <stdexcept>
#include <thread>  // NOLINT

#include "catch2/catch_test_macros.hpp"

#include "epoll/epoll_context.hpp"
#include "epoll/socket_io_base_op.hpp"
#include "fmt/core.h"
#include "ip/socket_types.hpp"
#include "ip/tcp.hpp"
#include "ip/udp.hpp"
#include "monotonic_clock.hpp"
#include "stdexec.hpp"
#include "stdexec/__detail/__execution_fwd.hpp"
#include "stdexec/execution.hpp"
#include "test_common/receivers.hpp"

using net::epoll_context;
using net::monotonic_clock;
using scheduler = epoll_context::scheduler;
using namespace std::chrono_literals;  // NOLINT

using exec::schedule_after;
using exec::schedule_at;
using stdexec::schedule;
using stdexec::start_detached;
using stdexec::sync_wait;
using stdexec::then;
using stdexec::when_all;

// Provides a variable of int. When this operation is performed, the value of
// this variable is increased by 1,
struct increment_operation : epoll_context::operation_base {
  explicit increment_operation(int& n)
      : epoll_context::operation_base(), n_(n) {
    execute_ = [](epoll_context::operation_base* base) noexcept {
      ++static_cast<increment_operation*>(base)->n_;
    };
  }

  int& n_;
};

struct empty_op : epoll_context::operation_base {
  empty_op() : epoll_context::operation_base() {
    execute_ = [](epoll_context::operation_base* base) noexcept {
    };
  }
};

TEST_CASE(
    "[default constructor should create descriptors and add them to epoll]",
    "[epoll_context.ctor]") {
  epoll_context ctx{};
  CHECK(ctx.epoll_fd_ != invalid_socket_fd);
  CHECK(ctx.timer_fd_ != invalid_socket_fd);
  CHECK(ctx.interrupter_.read_descriptor() != invalid_socket_fd);
  CHECK(ctx.timers_.empty());
  CHECK(ctx.current_earliest_due_time_ == std::nullopt);
  CHECK(ctx.processed_remote_queue_submitted_ == false);
  CHECK(ctx.timers_are_dirty_ == false);
  CHECK(ctx.local_queue_.empty());
  CHECK(ctx.remote_queue_.head_ == nullptr);
  CHECK(ctx.outstanding_work_ == 0);
  CHECK(ctx.stop_source_->stop_requested() == false);
  CHECK(ctx.is_running_ == false);
}

TEST_CASE("operation_base default constructor should work",
          "[epoll_context.operation_base]") {
  epoll_context::operation_base base{};
  CHECK(base.enqueued_ == false);
  CHECK(base.next_ == nullptr);
  CHECK(base.execute_ == nullptr);
}

TEST_CASE("stop_op default constructor should work",
          "[epoll_context.stop_op]") {
  epoll_context::stop_op stop_op{};
  CHECK(stop_op.enqueued_ == false);
  CHECK(stop_op.next_ == nullptr);
  CHECK(stop_op.execute_ != nullptr);
  CHECK(stop_op.should_stop_ == false);
}

TEST_CASE("executes stop_op should set should_stop flag",
          "[epoll_context.stop_op]") {
  epoll_context::stop_op stop_op{};
  stop_op.execute_(&stop_op);
  CHECK(stop_op.should_stop_ == true);
}

TEST_CASE("[default constructor of epoll_context::schedule_at_base_op]",
          "[epoll_context.schedule_at_base_op]") {
  epoll_context ctx{};
  monotonic_clock::time_point tp = monotonic_clock::now() + 100ms;
  epoll_context::schedule_at_base_op op{ctx, tp, false};
  CHECK(op.timer_next_ == nullptr);
  CHECK(op.timer_prev_ == nullptr);
  CHECK(&op.context_ == &ctx);
  CHECK(op.due_time_ == tp);
  CHECK(op.state_ == 0);
}

TEST_CASE("schedule_local() should return correct operation",
          "[epoll_context.schedule]") {
  int n = 10;
  increment_operation op{n};
  epoll_context ctx{};
  ctx.schedule_local(&op);
  CHECK(op.enqueued_ == true);
  CHECK(!ctx.local_queue_.empty());
  auto op2 = static_cast<increment_operation*>(ctx.local_queue_.pop_front());
  CHECK(ctx.local_queue_.empty());
  CHECK(op2 == &op);

  // The `enqueued_` must be to false when destructing `operation_base`.
  op.enqueued_ = false;
}

TEST_CASE(
    "schedule_local(queue) won't adjust the `enqueued_` flag of operations in "
    "queue"
    "[epoll_context.schedule]") {
  int n = 10, m = 13;
  increment_operation opn{n};
  increment_operation opm{m};
  opn.enqueued_ = true;
  opm.enqueued_ = true;
  epoll_context::operation_queue queue;
  queue.push_back(&opn);
  queue.push_back(&opm);
  epoll_context ctx;
  ctx.schedule_local(std::move(queue));
  CHECK(queue.empty());
  // The operation queue must be empty at destruct time
  (void)ctx.local_queue_.pop_front();
  (void)ctx.local_queue_.pop_front();
  CHECK(opn.enqueued_ == true);
  CHECK(opm.enqueued_ == true);
  opn.enqueued_ = false;
  opm.enqueued_ = false;
}

TEST_CASE("execute_local() executes the local committed operation",
          "[epoll_context.execute_local]") {
  int n = 10;
  increment_operation op{n};
  epoll_context ctx{};
  ctx.schedule_local(&op);
  CHECK(ctx.execute_local() == 1);
  CHECK(n == 11);
}

TEST_CASE(
    "execute_local() executes all local committed operations in the queue",
    "[epoll_context.execute_local]") {
  int n = 10, m = 13;
  increment_operation opn{n};
  increment_operation opm{m};
  opn.enqueued_ = true;
  opm.enqueued_ = true;
  epoll_context::operation_queue queue;
  queue.push_back(&opn);
  queue.push_back(&opm);

  epoll_context ctx;
  ctx.schedule_local(std::move(queue));
  ctx.execute_local();
  CHECK(n == 11);
  CHECK(m == 14);

  opn.enqueued_ = false;
  opm.enqueued_ = false;
}

TEST_CASE(
    "epoll_context::run() returns immediately if epoll_context::request_stop() "
    "called",
    "[epoll_context.run]") {
  epoll_context ctx{};
  std::jthread io_thread([&] { ctx.run(); });
  std::this_thread::sleep_for(10ms);
  CHECK(ctx.is_running());
  ctx.request_stop();
  std::this_thread::sleep_for(10ms);
  CHECK(!ctx.is_running());
  CHECK(ctx.stop_requested());
}

TEST_CASE("schedule_remote() should return correct operation",
          "[epoll_context.schedule]") {
  int n = 10;
  increment_operation op{n};
  epoll_context ctx{};
  ctx.schedule_remote(&op);
  CHECK(op.enqueued_ == true);
  auto op2 = ctx.remote_queue_.dequeue_all();
  CHECK(op2.pop_front() == &op);

  // The `enqueued_` must be to false when destructing `operation_base`.
  op.enqueued_ = false;
}

TEST_CASE(
    "try_schedule_remote_to_local() should move contents from remote queue to "
    "local queue",
    "[epoll_context.schedule]") {
  int n = 10;
  increment_operation op{n};
  epoll_context ctx{};
  ctx.schedule_remote(&op);
  CHECK(ctx.try_schedule_remote_to_local() == false);
  CHECK(!ctx.local_queue_.empty());
  CHECK(ctx.execute_local() == 1);
  CHECK(n == 11);
}

TEST_CASE(
    "[Operation submitted by remote threads should schedule to remote queue]",
    "[epoll_context.schedule]") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx]() { ctx.run(); });

  int n = 10;
  increment_operation op{n};
  CHECK(ctx.is_running_on_io_thread() == false);
  ctx.schedule_impl(&op);
  CHECK(ctx.remote_queue_.head_ != nullptr);
  std::this_thread::sleep_for(50ms);
  ctx.request_stop();
  CHECK(n == 11);
}

TEST_CASE(
    "is_running_on_io_thread returns true if running on io thread otherwise "
    "return false",
    "[epoll_context.is_running_on_io_thread]") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard guard{[&ctx]() noexcept {
    ctx.request_stop();
  }};
  CHECK(ctx.is_running_on_io_thread() == false);
}

TEST_CASE("this function shouldn't block when local_queue_ is not empty",
          "[epoll_context.acquire_completion_queue_items]") {
  epoll_context ctx;
  empty_op op;
  ctx.schedule_local(&op);
  ctx.acquire_completion_queue_items();
  (void)ctx.local_queue_.pop_front();
  op.enqueued_ = false;
}

TEST_CASE(
    "interrupt() should wakeup context and set "
    "processed_remote_queue_submitted flag to false",
    "[epoll_context.acquire_completion_queue_items]") {
  epoll_context ctx;
  CHECK(ctx.processed_remote_queue_submitted_ == false);
  ctx.processed_remote_queue_submitted_ = true;
  std::jthread io_thread([&ctx] {
    ctx.acquire_completion_queue_items();
    CHECK(ctx.processed_remote_queue_submitted_ == false);
  });
  ctx.interrupt();
}

TEST_CASE("timer event should wakeup context",
          "[epoll_context.acquire_completion_queue_items]") {
  epoll_context ctx;
  CHECK(ctx.processed_remote_queue_submitted_ == false);
  ctx.processed_remote_queue_submitted_ = true;
  std::jthread io_thread([&ctx] {
    ctx.acquire_completion_queue_items();
    CHECK(ctx.processed_remote_queue_submitted_ == false);
  });
  ctx.interrupt();
}

TEST_CASE(
    "set_timer sets the correct time. When this time is reached, the context "
    "can be notified, and "
    "the blocking call to the acquire_completion_queue_items can be awakened.",
    "[epoll_context.timers]") {
  epoll_context ctx;
  std::jthread io_thread([&ctx] {
    ctx.acquire_completion_queue_items();
    CHECK(ctx.timers_are_dirty_);
  });
  CHECK(ctx.timers_are_dirty_ == false);
  monotonic_clock::time_point tp = monotonic_clock::now() + 100ms;
  ctx.set_timer(tp);
}

TEST_CASE("set_timer use empty time_point", "[epoll_context.timers]") {
  epoll_context ctx;
  std::jthread io_thread([&ctx] {
    ctx.acquire_completion_queue_items();
    CHECK(ctx.timers_are_dirty_);
  });
  CHECK(ctx.timers_are_dirty_ == false);
  monotonic_clock::time_point tp = monotonic_clock::now() - 2s;
  ctx.set_timer(tp);
}

TEST_CASE("remove timers from heap", "[epoll_context.timers]") {
  epoll_context ctx;
  monotonic_clock::time_point now = monotonic_clock::now();
  epoll_context::schedule_at_base_op op{ctx, now + 10s, false};
  epoll_context::schedule_at_base_op op2{ctx, now + 11s, false};
  ctx.timers_.insert(&op);
  ctx.timers_.insert(&op2);
  CHECK(ctx.timers_.top() == &op);
  CHECK(ctx.timers_.top()->timer_next_ == &op2);
  ctx.remove_timer(&op);
  CHECK(ctx.timers_.empty() == false);
  CHECK(ctx.timers_.top() == &op2);
  ctx.remove_timer(&op2);
  CHECK(ctx.timers_.empty());
  CHECK(ctx.timers_.top() == nullptr);
}

TEST_CASE("update_timers when all timers have just been alarmed",
          "[epoll_context.timers]") {
  epoll_context ctx;
  ctx.current_earliest_due_time_ = monotonic_clock::now() - 1s;
  ctx.timers_are_dirty_ = true;

  // 1. ther timer heap should be empty since all timers have just been alarmed.
  CHECK(ctx.timers_.empty());

  // 2. current_earliest_due_timer_ should still have value until we reset it in
  // `update_timers`.
  CHECK(ctx.current_earliest_due_time_.has_value());

  ctx.update_timers();

  CHECK(ctx.timers_are_dirty_ == false);
  CHECK(ctx.current_earliest_due_time_.has_value() == false);
  CHECK(ctx.timers_.empty());
}

TEST_CASE(
    "Add two timers to the context when there is no active timer. These two "
    "timers "
    "do not immediately alarm.",
    "[epoll_context.timers]") {
  epoll_context ctx;
  ctx.timers_are_dirty_ = true;
  monotonic_clock::time_point now = monotonic_clock::now();
  epoll_context::schedule_at_base_op op{ctx, now + 10s, false};
  epoll_context::schedule_at_base_op op2{ctx, now + 11s, false};
  ctx.timers_.insert(&op);
  ctx.timers_.insert(&op2);

  // 1. ther timer heap should not be empty before we updated it.
  CHECK(ctx.timers_.empty() == false);

  // 2. current_earliest_due_timer_ shouldn't have value since there is no
  // active timer.
  CHECK(ctx.current_earliest_due_time_.has_value() == false);

  ctx.update_timers();

  // 3. Since we have updated timers, we set timers_are_dirty == false
  CHECK(ctx.timers_are_dirty_ == false);

  // 4. Since there exists active timers, current_earliest_due_time shoule set
  // to the earliest time.
  CHECK(ctx.current_earliest_due_time_.has_value() == true);
  CHECK((*ctx.current_earliest_due_time_ - now).count() ==
        10 * monotonic_clock::ratio::den);

  // 5. Timers still in the heap.
  CHECK(ctx.timers_.top() == &op);
  CHECK(ctx.timers_.top()->timer_next_ == &op2);
}

TEST_CASE(
    "Add two timers to the context when there is an active timer. The first "
    "timer is going to "
    "alarm earlier than the existing timer in the context."
    "[epoll_context.timers]") {
  epoll_context ctx;
  monotonic_clock::time_point now = monotonic_clock::now();

  // There is an active timer in the context.
  epoll_context::schedule_at_base_op old_op{ctx, now + 10s, false};
  ctx.timers_.insert(&old_op);
  ctx.current_earliest_due_time_ = now + 10s;

  // Added two timers to the timer heap of context.
  // The first timer will alarm earlier than the old timer.
  ctx.timers_are_dirty_ = true;
  epoll_context::schedule_at_base_op op{ctx, now + 5s, false};
  epoll_context::schedule_at_base_op op2{ctx, now + 11s, false};
  ctx.timers_.insert(&op);
  ctx.timers_.insert(&op2);

  // 1. the order of timer in the timer heap has been adjusted.
  CHECK(ctx.timers_.empty() == false);
  CHECK(ctx.timers_.top() == &op);
  CHECK(ctx.timers_.top()->timer_next_ == &old_op);
  CHECK(ctx.timers_.top()->timer_next_->timer_next_ == &op2);

  // 2. current_earliest_due_timer_ should have value since there is an active
  // timer.
  CHECK(ctx.current_earliest_due_time_.has_value());
  CHECK(*ctx.current_earliest_due_time_ == now + 10s);

  ctx.update_timers();

  // 3. Since we have updated timers, we set timers_are_dirty == false
  CHECK(ctx.timers_are_dirty_ == false);

  // 4. The current_earliest_due_time shoule set to the earliest time.
  CHECK(ctx.current_earliest_due_time_.has_value() == true);
  CHECK(*ctx.current_earliest_due_time_ == now + 5s);

  // 5. Timers still in the heap.
  CHECK(ctx.timers_.empty() == false);
  CHECK(ctx.timers_.top() == &op);
  CHECK(ctx.timers_.top()->timer_next_ == &old_op);
  CHECK(ctx.timers_.top()->timer_next_->timer_next_ == &op2);
}

TEST_CASE(
    "Add two timers to the context when there is an active timer. No timer is "
    "going to alarm "
    "earlier than the existing timer in the context."
    "[epoll_context.timers]") {
  epoll_context ctx;
  monotonic_clock::time_point now = monotonic_clock::now();

  // There is an active timer in the context.
  epoll_context::schedule_at_base_op old_op{ctx, now + 5s, false};
  ctx.timers_.insert(&old_op);
  ctx.current_earliest_due_time_ = now + 5s;

  // Added two timers to the timer heap of context.
  // No timer will alarm earlier than the old timer.
  ctx.timers_are_dirty_ = true;
  epoll_context::schedule_at_base_op op{ctx, now + 10s, false};
  epoll_context::schedule_at_base_op op2{ctx, now + 11s, false};
  ctx.timers_.insert(&op);
  ctx.timers_.insert(&op2);

  // 1. Added two timers to the heap.
  CHECK(ctx.timers_.empty() == false);
  CHECK(ctx.timers_.top() == &old_op);
  CHECK(ctx.timers_.top()->timer_next_ == &op);
  CHECK(ctx.timers_.top()->timer_next_->timer_next_ == &op2);

  // 2. current_earliest_due_timer_ should have value since there is an active
  // timer.
  CHECK(ctx.current_earliest_due_time_.has_value());
  CHECK(*ctx.current_earliest_due_time_ == now + 5s);

  ctx.update_timers();

  // 3. Since we have updated timers, we set timers_are_dirty == false
  CHECK(ctx.timers_are_dirty_ == false);

  // 4. The current_earliest_due_time shoule set to the earliest time.
  CHECK(ctx.current_earliest_due_time_.has_value() == true);
  CHECK(*ctx.current_earliest_due_time_ == now + 5s);

  // 5. Timers still in the heap.
  CHECK(ctx.timers_.empty() == false);
  CHECK(ctx.timers_.top() == &old_op);
  CHECK(ctx.timers_.top()->timer_next_ == &op);
  CHECK(ctx.timers_.top()->timer_next_->timer_next_ == &op2);
}

TEST_CASE("[default constructor of schedule_env]",
          "[epoll_context.scheduler]") {
  epoll_context ctx{};
  scheduler::schedule_env env{ctx};
  CHECK(&env.context == &ctx);
}

TEST_CASE("[default constructor of schedule_at_sender::__t]",
          "[epoll_context.scheduler]") {
  epoll_context ctx{};
  monotonic_clock::time_point tp = monotonic_clock::now() + 100ms;
  stdexec::__t<scheduler::schedule_at_sender> s{scheduler::schedule_env{ctx},
                                                tp};
  CHECK(&s.env_.context == &ctx);
  CHECK(s.due_time_ == tp);
}

TEST_CASE(
    "[schedule_at_sender::__t should satisfy the concept of stdexec::sender]",
    "[epoll_context.scheduler]") {
  epoll_context ctx{};
  CHECK(stdexec::sender<stdexec::__t<scheduler::schedule_at_sender>>);
}

TEST_CASE(
    "[connect schedule_at_sender::__t to receiver should return "
    "operation_at_operation]",
    "[epoll_context.scheduler]") {
  epoll_context ctx{};
  monotonic_clock::time_point tp = monotonic_clock::now() + 100ms;
  stdexec::__t<scheduler::schedule_at_sender> s{scheduler::schedule_env{ctx},
                                                tp};
  stdexec::__t<universal_receiver> r{};
  auto op = stdexec::connect(std::move(s), std::move(r));

  CHECK(&op.context_ == &ctx);
  CHECK(op.due_time_ == tp);
}

TEST_CASE("[start() should put timer into context.timers_]",
          "[epoll_context.scheduler]") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // Notify context 50ms later.
  // The operation will be placed in the timer heap for 50ms after start.
  monotonic_clock::time_point tp = monotonic_clock::now() + 50ms;
  stdexec::__t<scheduler::schedule_at_sender> s{scheduler::schedule_env{ctx},
                                                tp};
  stdexec::__t<universal_receiver> r{};
  auto op = stdexec::connect(std::move(s), std::move(r));

  stdexec::start(op);
  std::this_thread::sleep_for(20ms);

  CHECK(ctx.timers_.top() == &op);

  // schedule_at_impl sets the timer need update flag, then context::run()
  // updates the timer and sets the need update timer flag to false.
  CHECK(ctx.timers_are_dirty_ == false);
}

TEST_CASE("[schedule_at_op works]", "[epoll_context.scheduler]") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // The context will wakeup after 50ms.
  monotonic_clock::time_point tp = monotonic_clock::now() + 50ms;
  stdexec::__t<scheduler::schedule_at_sender> s{scheduler::schedule_env{ctx},
                                                tp};
  stdexec::__t<universal_receiver> r{};
  auto op = stdexec::connect(std::move(s), std::move(r));

  stdexec::start(op);
  std::this_thread::sleep_for(100ms);
}

TEST_CASE("[request_stop() should cancel the timer]",
          "[epoll_context.scheduler]") {
  // TODO: we need a stoppable receiver.
}

TEST_CASE(
    "CPO Example: `schedule` should schedule operation to the queue of context "
    "immediately",
    "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // Get thread ids of io_thread and remote thread.
  auto io_thread_id = io_thread.get_id();
  auto remote_id = std::this_thread::get_id();
  CHECK(io_thread_id != remote_id);

  auto s = stdexec::schedule(ctx.get_scheduler())  //
           | stdexec::then([&io_thread_id] {
               CHECK(std::this_thread::get_id() == io_thread_id);
             });
  stdexec::sync_wait(std::move(s));
}

TEST_CASE(
    "CPO Exapmle: `schedule_at` should schedule operation to queue at a "
    "specific absolute time",
    "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  auto s = exec::schedule_at(ctx.get_scheduler(), start + 100ms)  //
           | stdexec::then([&start] {
               int elapsed = (monotonic_clock::now() - start).count();
               // We allow a certain margin of error.
               CHECK(elapsed <= 0.101 * monotonic_clock::ratio::den);
               CHECK(elapsed >= 0.1 * monotonic_clock::ratio::den);
             });
  stdexec::sync_wait(std::move(s));
}

TEST_CASE(
    "CPO Example: `schedule_after` should schedule operation to queue after a "
    "relative time",
    "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  auto s = exec::schedule_after(ctx.get_scheduler(), 100ms)  //
           | stdexec::then([&start] {
               int elapsed = (monotonic_clock::now() - start).count();
               // We allow a certain margin of error.
               CHECK(elapsed <= 0.101 * monotonic_clock::ratio::den);
               CHECK(elapsed >= 0.1 * monotonic_clock::ratio::den);
             });
  stdexec::sync_wait(std::move(s));
}

TEST_CASE("`schedule_after` 0s", "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  auto s = exec::schedule_after(ctx.get_scheduler(), 0s)  //
           | stdexec::then([&start] {
               int elapsed = (monotonic_clock::now() - start).count();
               // We allow a certain margin of error.
               CHECK(elapsed <= 0.001 * monotonic_clock::ratio::den);
               CHECK(elapsed >= 0 * monotonic_clock::ratio::den);
             });
  stdexec::sync_wait(std::move(s));
}

TEST_CASE("`schedule_at 10000-times` ", "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  std::jthread other_thread([&ctx]() {
    constexpr int timers = 10000;
    auto start = monotonic_clock::now();
    for (int i = 0; i < timers; ++i) {
      auto s = exec::schedule_at(ctx.get_scheduler(), start + 100ms)  //
               | stdexec::then([&start, &i] {
                   int elapsed = (monotonic_clock::now() - start).count();
                   // We allow a certain margin of error.
                   CHECK(elapsed <= 20 * monotonic_clock::ratio::den);
                   CHECK(elapsed >= 0.1 * monotonic_clock::ratio::den);
                 });
      stdexec::sync_wait(std::move(s));
    }
  });
}

TEST_CASE("`schedule 10000-times` ", "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  std::jthread other_thread([&ctx, &start]() {
    constexpr int timers = 10000;
    for (int i = 0; i < timers; ++i) {
      auto s = stdexec::schedule(ctx.get_scheduler())  //
               | stdexec::then([&i] {});
      stdexec::sync_wait(std::move(s));
    }
    auto end = monotonic_clock::now();
    CHECK((end - start).count() <= 10 * monotonic_clock::ratio::den);
  });
}

TEST_CASE("`schedule 10000-times use repeat_effect_until` ",
          "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  std::jthread other_thread([&ctx, &start]() {
    int i = 0;
    constexpr int timers = 10000;
    auto s = stdexec::schedule(ctx.get_scheduler())           //
             | stdexec::then([&i] { return ++i == timers; })  //
             | exec::repeat_effect_until();
    stdexec::sync_wait(std::move(s));

    CHECK(i == timers);
  });
}

TEST_CASE("`schedule 10000-times use when_all` ", "epoll_context.scheduler") {
  epoll_context ctx{};
  std::jthread io_thread([&ctx] { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  auto start = monotonic_clock::now();
  std::jthread other_thread([&ctx, &start]() {
    constexpr int timers = 10000;
    for (int i = 0; i < timers; ++i) {
      auto task = stdexec::schedule(ctx.get_scheduler());
      stdexec::sender auto s = stdexec::when_all(
          task, task, task, task, task, task, task, task, task, task, task,
          task, task, task, task, task, task, task, task, task, task, task,
          task, task, task, task, task, task, task, task, task, task, task,
          task, task, task, task, task, task, task, task, task, task, task);
      stdexec::sync_wait(std::move(s));
    }
  });
}

TEST_CASE(
    "schedule operations to same context from multiple threads is thread safe",
    "[epoll_context.scheduler]") {
  epoll_context context;
  auto scheduler = context.get_scheduler();
  std::jthread io_thread{[&] {
    context.run();
  }};
  exec::scope_guard guard{[&]() noexcept {
    context.request_stop();
  }};

  auto fn = [&] {
    CHECK(io_thread.get_id() == std::this_thread::get_id());
  };

  {
    std::jthread thread1{[&] {
      for (int i = 0; i < 1000; ++i) {
        sync_wait(when_all(schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn),  //
                           schedule(scheduler) | then(fn)));
      }
    }};

    std::jthread thread2{[&] {
      for (int i = 0; i < 1000; ++i) {
        auto tp = monotonic_clock::now() + 1us;
        start_detached(when_all(schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn),  //
                                schedule_at(scheduler, tp) | then(fn)));
      }
    }};

    std::jthread thread3{[&] {
      for (int i = 0; i < 1000; ++i) {
        sync_wait(when_all(schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn),  //
                           schedule_after(scheduler, 1us) | then(fn)));
      }
    }};
  }
}

TEST_CASE("CPO example: now should return current time point",
          "epoll_context.timers") {
  epoll_context ctx{};
  auto sched = ctx.get_scheduler();
  // TODO: This line couldn't compile currently.
  // auto tp = exec::now(sched);
}
