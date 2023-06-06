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
#ifndef EPOLL_EPOLL_CONTEXT_HPP_
#define EPOLL_EPOLL_CONTEXT_HPP_

#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <atomic>
#include <chrono>  // NOLINT
#include <cstring>
#include <optional>
#include <utility>

#include "status-code/result.hpp"
#include "stdexec.hpp"

#include "atomic_intrusive_queue.hpp"
#include "eventfd_interrupter.hpp"
#include "execution_context.hpp"
#include "intrusive_heap.hpp"
#include "intrusive_list.hpp"
#include "meta.hpp"
#include "monotonic_clock.hpp"

namespace net {
namespace __epoll {
// The usage mode is single thread single epoll. Multithreading drive one
// context is not allowed. The thread which running the context is called io
// thread, and others are called remote threads.
class epoll_context final : public execution_context {
 public:
  // The scheduler of this context. Both `schedule_at`, `schedule_after` and
  // `schedule` customization point object are supported by this scheduler.
  class scheduler;

  // The base class for all the types of operations that this context can
  // perform. Note that operations will be executed by context in the order they
  // are committed.
  struct operation_base {
    // Default constructor.
    constexpr operation_base() noexcept
        : enqueued_(false), next_(nullptr), execute_(nullptr) {}

    // Destructor.
    ~operation_base() { assert(!enqueued_); }

    // The flag determines whether the current operation is in a remote or local
    // queue.
    std::atomic_bool enqueued_;

    // The `next_` pointer points to the next operation on the operation queue,
    operation_base* next_;

    // The `execute_` pointer points to the actual function to be executed.
    void (*execute_)(operation_base*) noexcept;  // NOLINT
  };

  // The completion operation.
  struct completion_op : operation_base {};

  // The stop operation.
  struct stop_op : operation_base {
    // Default constructor.
    constexpr stop_op() noexcept {
      this->execute_ = [](operation_base* op) noexcept {
        static_cast<stop_op*>(op)->should_stop_ = true;
      };
    }

    // Inner flag which indicates this operation should be stopped.
    bool should_stop_ = false;
  };

  // The base class for the socket io operation associated with epoll.
  template <typename Receiver, typename Protocol>
  class socket_io_base_op;

  // Socket operation that accepts a new connection based on epoll.
  template <typename Receiver, typename Protocol>
  class socket_accept_op;

  // recv some operation.
  template <typename Receiver, typename Protocol, typename Buffers>
  class socket_recv_some_op;

  // The time_point type used by scheduler.
  using time_point = monotonic_clock::time_point;

  // The `schedule_at_op` is executed on `due_time`.
  struct schedule_at_base_op : operation_base {
    schedule_at_base_op(epoll_context& context, const time_point& due_time,
                        bool can_be_cancelled) noexcept
        : timer_next_(nullptr),
          timer_prev_(nullptr),
          context_(context),
          due_time_(due_time),
          can_be_cancelled_(can_be_cancelled),
          state_(0) {}

    // The operation status value.
    static constexpr uint32_t timer_elapsed = 1;
    static constexpr uint32_t cancel_pending = 2;

    schedule_at_base_op* timer_next_;
    schedule_at_base_op* timer_prev_;
    epoll_context& context_;
    time_point due_time_;
    bool can_be_cancelled_;
    std::atomic<uint32_t> state_;
  };

  // The heap of all timer operations.
  using timer_heap = intrusive_heap<schedule_at_base_op,                //
                                    &schedule_at_base_op::timer_next_,  //
                                    &schedule_at_base_op::timer_prev_,  //
                                    time_point,                         //
                                    &schedule_at_base_op::due_time_>;

  // The queue of operations.
  using operation_queue = stdexec::__intrusive_queue<&operation_base::next_>;

  // Default constructor.
  epoll_context()
      : epoll_fd_(create_epoll()),                 //
        timer_fd_(create_timer()),                 //
        interrupter_(),                            //
        timers_(),                                 //
        current_earliest_due_time_(),              //
        processed_remote_queue_submitted_(false),  //
        timers_are_dirty_(false),                  //
        local_queue_(),                            //
        remote_queue_(),                           //
        outstanding_work_(0),                      //
        stop_source_(std::in_place),               //
        is_running_(false) {
    add_timer_to_epoll();
    add_interrupter_to_epoll();
  }

  // Destructor.
  ~epoll_context() {
    remove_timer_from_epoll();
    remove_interrupter_from_epoll();
  }

  // Execute all operations submitted to this context.
  void run();

  // Request to stop the context. Note that the context may block on the
  // epoll_wait call, so we must use interrupt to wake up the context.
  void request_stop() {
    stop_source_->request_stop();
    interrupter_.interrupt();
  }

  // Whether this context have been request to stop.
  bool stop_requested() const noexcept {
    return stop_source_->stop_requested();
  }

  // Get this context associated stop token.
  stdexec::in_place_stop_token get_stop_token() const noexcept {
    return stop_source_->get_token();
  }

  // Check whether this context is running.
  bool is_running() const noexcept {
    return is_running_.load(std::memory_order_relaxed);
  }

  // CPO: get_scheduler.
  constexpr scheduler get_scheduler() noexcept;

 private:
  // The thread that calls `context.run()` is called io thread, and other
  // threads are remote threads. This function checks which thread is using the
  // context.
  bool is_running_on_io_thread() const noexcept;

  // Check whether the thread submitting an operation to the context is the io
  // thread If the operation is submitted by the io thread, the operation is
  // submitted to the local queue; otherwise, the operation is submitted to
  // remote queue.
  void schedule_impl(operation_base* op) noexcept;

  // Schedule the operation to the local queue.
  void schedule_local(operation_base* op) noexcept;

  // Move all contents from remote queue to local queue.
  void schedule_local(operation_queue ops) noexcept;

  // Schedule the operation to the remote queue.
  void schedule_remote(operation_base* op) noexcept;

  // Insert the timer operation into the queue of timers. Must be called from
  // the I/O thread.
  void schedule_at_impl(schedule_at_base_op* op) noexcept;

  // Execute all items on the local queue.
  // Won't run other items that were enqueued during the execution of the items
  // that were already enqueued. This bounds the amount of work to a finite
  // amount.
  size_t execute_local() noexcept;

  // Check if any completion queue items are available and if so add them to the
  // local queue.
  void acquire_completion_queue_items();

  // Collect the contents of the remote queue and pass them to local queue.
  // The return value represents whether the remote queue is empty before we
  // make a collection. Returns true means remote queue is emtpy before we
  // collect.
  bool try_schedule_remote_to_local() noexcept;

  // Signal the remote queue eventfd.
  // This should only be called after trying to enqueue() work to the remote
  // queue and being told that the I/O thread is inactive.
  void interrupt() { interrupter_.interrupt(); }

  // Remove timer from time heap.
  void remove_timer(schedule_at_base_op* op) noexcept;

  // Update timers.
  void update_timers() noexcept;

  // `due_time` indicates the absolute time since system startup. This timer
  // will only alarm once. If both due_time.seconds() and due_time.nanoseconds()
  // are zero, this timer would't alarm.
  void set_timer(const time_point& due_time);

  // Used to specific the epoll_event to be a timer data
  constexpr void* timers_data() const {
    return const_cast<void*>(static_cast<const void*>(&timers_));
  }

  // Create epoll file descriptor. Throws an error when creation fails.
  int create_epoll();

  // Create timer file descriptor. Throws an error when creation fails.
  int create_timer();

  // Add the timer's file descriptor to epoll. Throws an error when
  // registrations fails.
  void add_timer_to_epoll();

  // Remove the timer's file descriptor from epoll. Throws an error when remove
  // fails.
  void remove_timer_from_epoll();

  // Add the interrupter's file descriptor to epoll.  Throws an error when
  // registrations fails.
  void add_interrupter_to_epoll();

  // Remove the interrupter's file descriptor from epoll. Throws an error when
  // remove fails.
  void remove_interrupter_from_epoll();

  // The epoll file descriptor.
  exec::safe_file_descriptor epoll_fd_;

  // The timer file descriptor.
  exec::safe_file_descriptor timer_fd_;

  // Notifying a remote thread to wake up from `epoll_wait`, usually after
  // committing an operation to another thread, allows the other thread to
  // execute the operation immediately without remaining stuck in `epoll_wait`.
  eventfd_interrupter interrupter_;

  // Set of operations waiting to be executed at a specific time.
  timer_heap timers_;

  // The absolute time that the current active timer submitted to the kernel.
  std::optional<time_point> current_earliest_due_time_;

  // Whether the operation submitted by the remote thread has been processed.
  bool processed_remote_queue_submitted_;

  // Indicates whether we should update timers.
  bool timers_are_dirty_;

  // Local queue for operations that are ready to execute.
  operation_queue local_queue_;

  // Queue of operations enqueued by remote threads.
  // exec::__atomic_intrusive_queue<&operation_base::next_> remote_queue_;
  atomic_intrusive_queue<&operation_base::next_> remote_queue_;

  // The count of unfinished work.
  std::atomic<int64_t> outstanding_work_;

  // The stop source.
  std::optional<stdexec::in_place_stop_source> stop_source_;

  // Whether this context is running.
  std::atomic_bool is_running_;
};

// The scheduler with returned by `stdexec::get_schedule` customization point
// object.
class epoll_context::scheduler {
  // The envrionment of scheduler.
  struct schedule_env;

  // The timing operation which do an immediately schedule.
  template <typename ReceiverId>
  class schedule_op;

  // The timing operation which do a schedule at specific time point.
  template <typename ReceiverId>
  class schedule_at_op;

  // The sender returned by `stdexec::schedule` customization point object.
  class schedule_sender;

  // The sender returned by either `stdexec::schedule_at` or
  // `stdexec::schedule_after` customization point object.
  class schedule_at_sender;

  //                                                                                     [implement]
  struct schedule_env {
    friend auto tag_invoke(
        stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
        const schedule_env& env) noexcept -> scheduler {
      return scheduler{env.context};
    }

    explicit constexpr schedule_env(epoll_context& ctx) noexcept
        : context(ctx) {}

    epoll_context& context;
  };  // schedule_env

  template <typename ReceiverId>
  class schedule_op {
    using receiver_t = stdexec::__t<ReceiverId>;

   public:
    struct __t : private operation_base {
      using __id = schedule_op;
      using stop_token =
          stdexec::stop_token_of_t<stdexec::env_of_t<receiver_t>&>;

      constexpr __t(epoll_context& context, receiver_t r)
          : context_(context), receiver_(static_cast<receiver_t&&>(r)) {
        execute_ = &execute_impl;
      }

      friend void tag_invoke(stdexec::start_t, __t& op) noexcept {
        op.start_impl();
      }

     private:
      constexpr void start_impl() noexcept { context_.schedule_impl(this); }

      static constexpr void execute_impl(operation_base* p) noexcept {
        auto& self = *static_cast<__t*>(p);
        if constexpr (!std::unstoppable_token<stop_token>) {
          auto stop_token =
              stdexec::get_stop_token(stdexec::get_env(self.receiver_));
          if (stop_token.stop_requested()) {
            stdexec::set_stopped(static_cast<receiver_t&&>(self.receiver_));
            return;
          }
        }
        stdexec::set_value(static_cast<receiver_t&&>(self.receiver_));
      }

      friend scheduler::schedule_sender;

      epoll_context& context_;
      STDEXEC_NO_UNIQUE_ADDRESS receiver_t receiver_;
    };
  };  // schedule_op.

  class schedule_sender {
    template <typename Receiver>
    using op_t = stdexec::__t<schedule_op<stdexec::__id<Receiver>>>;

   public:
    struct __t {
      using is_sender = void;
      using __id = schedule_sender;
      using completion_signatures =
          stdexec::completion_signatures<stdexec::set_value_t(),  //
                                         stdexec::set_stopped_t()>;

      template <typename Env>
      friend auto tag_invoke(stdexec::get_completion_signatures_t,
                             const __t& self, Env&&) noexcept
          -> completion_signatures;

      friend auto tag_invoke(stdexec::get_env_t, const __t& self) noexcept
          -> schedule_env {
        return self.env_;
      }

      template <stdexec::__decays_to<__t> Sender,
                stdexec::receiver_of<completion_signatures> Receiver>
      friend auto tag_invoke(stdexec::connect_t, Sender&& self,
                             Receiver receiver) noexcept -> op_t<Receiver> {
        return {static_cast<__t&&>(self).env_.context,
                static_cast<Receiver&&>(receiver)};
      }

      explicit constexpr __t(schedule_env env) noexcept : env_(env) {}

     private:
      friend epoll_context::scheduler;

      schedule_env env_;
    };
  };  // schedule_sender.

  template <typename ReceiverId>
  class schedule_at_op {
    using receiver_t = stdexec::__t<ReceiverId>;
    using time_point = epoll_context::time_point;
    using stop_token = stdexec::stop_token_of_t<stdexec::env_of_t<receiver_t>&>;

    struct __t : epoll_context::schedule_at_base_op {
      using __id = schedule_at_op;

      constexpr __t(epoll_context& context, const time_point& due_time,
                    receiver_t r) noexcept
          : epoll_context::schedule_at_base_op(
                context, due_time,
                stdexec::get_stop_token(stdexec::get_env(r)).stop_possible()),
            receiver_(static_cast<receiver_t&&>(r)) {}

      friend void tag_invoke(stdexec::start_t, __t& op) noexcept {
        op.start_impl();
      }

     private:
      constexpr void start_impl() noexcept {
        if (context_.is_running_on_io_thread()) {
          start_local();
        } else {
          start_remote();
        }
      }

      static constexpr void on_schedule_complete(operation_base* op) noexcept {
        static_cast<__t*>(op)->start_local();
      }

      static constexpr void complete_with_stop(operation_base* op) noexcept {
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          stdexec::set_stopped(
              static_cast<receiver_t&&>(static_cast<__t*>(op)->receiver_));
        } else {
          // This should never be called if stop is not possible.
          assert(false);
        }
      }

      // Executed when the timer gets to the front of the ready-to-run queue.
      static constexpr void complete_with_value(operation_base* op) noexcept {
        auto& timer_op = *static_cast<__t*>(op);
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          auto stop_token =
              stdexec::get_stop_token(stdexec::get_env(timer_op.receiver_));
          timer_op.stop_callback_.__destruct();
          if (stop_token.stop_requested()) {
            complete_with_stop(op);
            return;
          }
        }
        stdexec::set_value(static_cast<receiver_t>(timer_op.receiver_));
      }

      constexpr void start_local() noexcept {
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          auto stop_token =
              stdexec::get_stop_token(stdexec::get_env(receiver_));
          if (stop_token.stop_requested()) {
            // Stop already requested. Don't bother adding the timer.
            execute_ = &__t::complete_with_stop;
            context_.schedule_local(this);
            return;
          }
        }

        execute_ = &__t::complete_with_value;
        context_.schedule_at_impl(this);

        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          stop_callback_.__construct(
              stdexec::get_stop_token(stdexec::get_env(receiver_)),
              cancel_callback{*this});
        }
      }

      constexpr void start_remote() noexcept {
        execute_ = &__t::on_schedule_complete;
        context_.schedule_remote(this);
      }

      static constexpr void remove_timer_and_complete_with_stopped(
          operation_base* op) noexcept {
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          auto& timer_op = *static_cast<__t*>(op);
          assert(timer_op.context_.is_running_on_io_thread());
          timer_op.stop_callback_.__destruct();

          auto state = timer_op.state_.load(std::memory_order_relaxed);
          if ((state & schedule_at_base_op::timer_elapsed) == 0) {
            // Timer not yet removed from the timer heap. Do that now.
            timer_op.context_.remove_timer(&timer_op);
          }

          stdexec::set_stopped(static_cast<receiver_t&&>(timer_op.receiver_));
        } else {
          assert(false);
        }
      }

      constexpr void request_stop() noexcept {
        if (context_.is_running_on_io_thread()) {
          request_stop_local();
        } else {
          request_stop_remote();
        }
      }

      constexpr void request_stop_local() noexcept {
        assert(context_.is_running_on_io_thread());
        stop_callback_.__destruct();
        execute_ = &__t::complete_with_stop;

        auto state = this->state_.load(std::memory_order_relaxed);
        if ((state & schedule_at_base_op::timer_elapsed) == 0) {
          // Timer not yet elapsed. Remove timer from list of timers and enqueue
          // cancellation.
          context_.remove_timer(this);
          context_.schedule_local(this);
        } else {
          // Timer already elapsed and added to ready-to-run queue.
        }
      }

      constexpr void request_stop_remote() noexcept {
        auto oldState = state_.fetch_add(schedule_at_base_op::cancel_pending,
                                         std::memory_order_acq_rel);
        if ((oldState & schedule_at_base_op::timer_elapsed) == 0) {
          // Timer had not yet elapsed.
          // We are responsible for scheduling the completion of this timer
          // operation.
          execute_ = &remove_timer_and_complete_with_stopped;
          this->context_.schedule_remote(this);
        }
      }

      friend scheduler::schedule_at_sender;

      struct cancel_callback {
        __t& op_;

        void operator()() noexcept { op_.request_stop(); }
      };

      STDEXEC_NO_UNIQUE_ADDRESS receiver_t receiver_;
      exec::__manual_lifetime<
          typename stop_token::template callback_type<cancel_callback>>
          stop_callback_;
    };
  };  // schedule_at_op.

  class schedule_at_sender {
    template <typename Receiver>
    using op_t = stdexec::__t<schedule_at_op<stdexec::__id<Receiver>>>;
    using time_point = epoll_context::time_point;

   public:
    struct __t {
      using is_sender = void;
      using __id = schedule_at_sender;
      using completion_signatures =
          stdexec::completion_signatures<stdexec::set_value_t(),
                                         stdexec::set_stopped_t()>;

      template <typename Env>
      friend auto tag_invoke(stdexec::get_completion_signatures_t,
                             const __t& self, Env&&) noexcept
          -> completion_signatures;

      friend auto tag_invoke(stdexec::get_env_t, const __t& self) noexcept
          -> schedule_env {
        return self.env_;
      }

      template <stdexec::__decays_to<__t> Sender,
                stdexec::receiver_of<completion_signatures> Receiver>
      friend auto tag_invoke(stdexec::connect_t, Sender&& self,
                             Receiver receiver) noexcept -> op_t<Receiver> {
        return {static_cast<__t&&>(self).env_.context,  //
                static_cast<__t&&>(self).due_time_,     //
                static_cast<Receiver&&>(receiver)};
      }

      constexpr __t(schedule_env env, const time_point& due_time) noexcept
          : env_(env), due_time_(due_time) {}

     private:
      friend epoll_context::scheduler;

      schedule_env env_;
      time_point due_time_;
    };
  };  // schedule_at_sender

 public:
  // Constructors.
  explicit constexpr scheduler(epoll_context& context) noexcept
      : context_(&context) {}

  constexpr scheduler(const scheduler&) noexcept = default;

  constexpr scheduler& operator=(const scheduler&) = default;

  constexpr ~scheduler() = default;

  friend auto tag_invoke(exec::now_t, const scheduler&) noexcept -> time_point {
    return monotonic_clock::now();
  }

  friend auto tag_invoke(stdexec::schedule_t, const scheduler& sched) noexcept
      -> stdexec::__t<schedule_sender> {
    return stdexec::__t<schedule_sender>{schedule_env{*sched.context_}};
  }

  friend auto tag_invoke(exec::schedule_at_t,     //
                         const scheduler& sched,  //
                         const time_point& due_time) noexcept
      -> stdexec::__t<schedule_at_sender> {
    return {schedule_env{*sched.context_}, due_time};
  }

  friend auto tag_invoke(exec::schedule_after_t,  //
                         const scheduler& sched,  //
                         std::chrono::nanoseconds duration) noexcept
      -> stdexec::__t<schedule_at_sender> {
    return {schedule_env{*sched.context_}, monotonic_clock::now() + duration};
  }

 private:
  friend bool operator==(scheduler a, scheduler b) noexcept {
    return a.context_ == b.context_;
  }

  friend bool operator!=(scheduler a, scheduler b) noexcept {
    return a.context_ != b.context_;
  }

  friend epoll_context;

  epoll_context* context_;
};

inline constexpr epoll_context::scheduler
epoll_context::get_scheduler() noexcept {
  return scheduler{*this};
}

inline int epoll_context::create_epoll() {
  int fd = ::epoll_create(1);
  if (fd < 0) {
    // If creating an epoll fails, it makes more sense to throw an exception to
    // terminate the program than to give a solution
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_create(1)"};
  }
  return fd;
}

inline int epoll_context::create_timer() {
  int fd = ::timerfd_create(CLOCK_MONOTONIC, 0);
  if (fd < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "timer_fd create"};
  }
  return fd;
}

inline void epoll_context::add_timer_to_epoll() {
  epoll_event event = {.events = EPOLLIN | EPOLLERR,
                       .data = {.ptr = timers_data()}};
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &event) < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_ctl_add timer_fd"};
  }
}

inline void epoll_context::remove_timer_from_epoll() {
  epoll_event event = {};
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, timer_fd_, &event) < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_ctl_del timer_fd"};
  }
}

inline void epoll_context::add_interrupter_to_epoll() {
  epoll_event ev = {.events = EPOLLIN | EPOLLERR | EPOLLET,
                    .data = {.ptr = &interrupter_}};
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, interrupter_.read_descriptor(),
                  &ev) < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_ctl_add interrupter_fd"};
  }
}

inline void epoll_context::remove_interrupter_from_epoll() {
  epoll_event event = {};
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, interrupter_.read_descriptor(),
                  &event) < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_ctl_del interrupter_fd"};
  }
}

// !!!Stores the address of the context owned by the current thread
static thread_local epoll_context* current_thread_context;

static constexpr uint32_t epoll_event_max_count = 256;

inline size_t epoll_context::execute_local() noexcept {
  if (local_queue_.empty()) {
    return 0;
  }
  size_t count = 0;
  auto pending = std::move(local_queue_);
  while (!pending.empty()) {
    auto* item = pending.pop_front();
    assert(item->enqueued_);
    item->enqueued_ = false;
    std::exchange(item->next_, nullptr);
    item->execute_(item);
    ++count;
  }
  return count;
}

inline void epoll_context::acquire_completion_queue_items() {
  epoll_event events[epoll_event_max_count];  
  int wait_timeout = local_queue_.empty() ? -1 : 0;
  int result =
      ::epoll_wait(epoll_fd_, events, epoll_event_max_count, wait_timeout);
  if (result < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "epoll_wait_return_error"};
  }

  // temporary queue of newly completed items.
  operation_queue completion_queue;
  for (int i = 0; i < result; ++i) {
    if (events[i].data.ptr == &interrupter_) {
      // No need to read the eventfd to clear the signal since we're leaving the
      // descriptor in a ready-to-read state and relying on edge-triggered
      // notifications. Skip processing this item and let the run loop check for
      // the remote-queued items next time.
      processed_remote_queue_submitted_ = false;
    } else if (events[i].data.ptr == timers_data()) {
      current_earliest_due_time_.reset();
      timers_are_dirty_ = true;

      // Read the timerfd to clear the signal.
      size_t buffer;
      if (::read(timer_fd_, &buffer, sizeof(buffer)) < 0) {
        throw std::system_error{static_cast<int>(errno), std::system_category(),
                                "read_timerfd"};
      }
    } else {
      auto& completion_state =
          *reinterpret_cast<completion_op*>(events[i].data.ptr);
      assert(completion_state.enqueued_.load() == false);
      completion_state.enqueued_ = true;
      completion_queue.push_back(&completion_state);
    }
  }
  schedule_local(std::move(completion_queue));
}

inline void epoll_context::run() {
  // Only one thread of execution is allowed to drive the io context.
  bool expected_running = false;
  if (!is_running_.compare_exchange_strong(expected_running, true,
                                           std::memory_order_relaxed)) {
    throw std::runtime_error(
        "epoll_context::run() called on a running context");
  }
  exec::scope_guard set_not_running{[&]() noexcept {  //
    is_running_.store(false, std::memory_order_relaxed);
  }};

  auto* old_context = std::exchange(current_thread_context, this);
  exec::scope_guard g{[=]() noexcept {
    std::exchange(current_thread_context, old_context);
  }};

  size_t executed_cnt;
  while (true) {
    executed_cnt = execute_local();
    if (stop_source_->stop_requested()) {
      // Should we cancel all operations in this context or just ignored?
      break;
    }
    if (timers_are_dirty_) {
      update_timers();
    }
    if (!processed_remote_queue_submitted_) {
      // This flag will be true if we have collected some items from remote
      // queue to local queue. So we have cleared remote queue. Just wait the
      // next time's interrupt.
      processed_remote_queue_submitted_ = try_schedule_remote_to_local();
    }
    acquire_completion_queue_items();
  }
}

inline bool epoll_context::is_running_on_io_thread() const noexcept {
  return this == current_thread_context;
}

inline void epoll_context::schedule_impl(operation_base* op) noexcept {
  assert(op != nullptr);
  if (is_running_on_io_thread()) {
    schedule_local(op);
  } else {
    schedule_remote(op);
  }
}

inline void epoll_context::schedule_local(operation_base* op) noexcept {
  assert(op->execute_ != nullptr);
  assert(!op->enqueued_);
  op->enqueued_ = true;
  local_queue_.push_back(op);
}

inline void epoll_context::schedule_local(operation_queue ops) noexcept {
  // Do not adjust the enqueued flag, which is still true because the ops will
  // immediately be transferred from the remote queue to the local queue.
  local_queue_.append(std::move(ops));
}

inline void epoll_context::schedule_remote(operation_base* op) noexcept {
  assert(!op->enqueued_.load());
  op->enqueued_ = true;
  if (remote_queue_.enqueue(op)) {
    // We were the first to queue an item and the I/O thread is not
    // going to check the queue until we notify it that new items
    // have been enqueued remotely by writing to the eventfd.
    interrupter_.interrupt();
  }
}

inline bool epoll_context::try_schedule_remote_to_local() noexcept {
  auto queued_items = remote_queue_.try_mark_inactive_or_dequeue_all();
  if (!queued_items.empty()) {
    schedule_local(std::move(queued_items));
    return false;
  }
  return true;
}

inline void epoll_context::schedule_at_impl(schedule_at_base_op* op) noexcept {
  assert(op);
  assert(is_running_on_io_thread());
  timers_.insert(op);
  if (timers_.top() == op) {
    timers_are_dirty_ = true;
  }
}

inline void epoll_context::remove_timer(schedule_at_base_op* op) noexcept {
  assert(!timers_.empty());
  if (timers_.top() == op) {
    timers_are_dirty_ = true;
  }
  timers_.remove(op);
}

inline void epoll_context::update_timers() noexcept {
  // Reap any elapsed timers.
  if (!timers_.empty()) {
    time_point now = monotonic_clock::now();
    while (!timers_.empty() && timers_.top()->due_time_ <= now) {
      schedule_at_base_op* op = timers_.pop();
      if (op->can_be_cancelled_) {
        auto old_state = op->state_.fetch_add(
            schedule_at_base_op::timer_elapsed, std::memory_order_acq_rel);
        if ((old_state & schedule_at_base_op::cancel_pending) != 0) {
          // Timer has been cancelled by a remote thread.
          // The other thread is responsible for enqueueing is operation onto
          // the remote_queue_.
          continue;
        }
      }

      // Otherwise, we are responsible for enqueuing the timer onto the
      // ready-to-run queue.
      schedule_local(op);
    }
  }

  // Check if we need to cancel or start some new OS timers.
  if (timers_.empty()) {
    // If there is no timing operation currently, we should reset the timing
    // time for timerfd
    if (current_earliest_due_time_.has_value()) {
      current_earliest_due_time_.reset();
      set_timer(time_point{});
    }
  } else {
    const auto earliest_due_time = timers_.top()->due_time_;
    if (current_earliest_due_time_) {
      constexpr auto threshold = std::chrono::microseconds(1);
      if (earliest_due_time < (*current_earliest_due_time_ - threshold)) {
        // An earlier time has been scheduled.
        // Cancel the old timer before submitting a new one.
        current_earliest_due_time_.reset();
        set_timer(earliest_due_time);
        current_earliest_due_time_ = earliest_due_time;
      }
    } else {
      // No active timer, submit a new timer
      set_timer(earliest_due_time);
      current_earliest_due_time_ = earliest_due_time;
    }
  }

  // Since we have updated_times, we set this flag to false.
  timers_are_dirty_ = false;
}

inline void epoll_context::set_timer(const time_point& due_time) {
  ::itimerspec time = {
      .it_interval = {.tv_sec = 0, .tv_nsec = 0},
      .it_value = {.tv_sec = due_time.seconds(),
                   .tv_nsec = static_cast<int64_t>(due_time.nanoseconds())}};

  if (::timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &time, nullptr) < 0) {
    throw std::system_error{static_cast<int>(errno), std::system_category(),
                            "set_timer"};
  }
}
};  // namespace __epoll

using epoll_context = __epoll::epoll_context;
}  // namespace net

#endif  // EPOLL_EPOLL_CONTEXT_HPP_
