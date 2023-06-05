#ifndef NET_SRC_EPOLL_SOCKET_IO_BASE_OPERATION_HPP_
#define NET_SRC_EPOLL_SOCKET_IO_BASE_OPERATION_HPP_

#include <cassert>
#include <concepts>
#include <status-code/system_code.hpp>

#include "basic_socket.hpp"
#include "epoll/epoll_context.hpp"
#include "socket_option.hpp"
#include "stdexec.hpp"

namespace net::__epoll {
// Base class for socket I/O operations
template <typename ReceiverId, typename Protocol>
class epoll_context::socket_io_base_op {
  using receiver_t = stdexec::__t<ReceiverId>;

 public:
  struct __t : public stdexec::__immovable,
               private epoll_context::stop_op,
               private epoll_context::completion_op {
    using __id = socket_io_base_op;
    using socket_t = typename Protocol::socket;
    using endpoint_t = typename Protocol::endpoint;
    using stop_token = stdexec::stop_token_of_t<stdexec::env_of_t<receiver_t>>;

    // Use theses to synchronize the remote thread and the io thread.
    static constexpr std::uint32_t operation_ended = 0x00010000;
    static constexpr std::uint32_t operation_ended_mask = 0xFFFF0000;
    static constexpr std::uint32_t request_stopped = 0x1;
    static constexpr std::uint32_t request_stopped_mask = 0xFFFF;

    // Subclasses should provide these necessary functions.
    struct op_vtable {
      // The core implementation of this operation.
      // The `ec_` should be assigned to indicate whether the operation is complete.
      void (*perform)(__t*) noexcept = nullptr;

      // This operation is complete. Notify the downstream receiver based on the value of `ec_`.
      void (*complete)(__t*) noexcept = nullptr;
    };

    // The operation type of the subclass.
    enum class op_type { op_read = 1, op_write = 2, op_connect = 2 };

    struct cancel_callback {
      __t& op_;
      void operator()() noexcept { op_.request_stop(); }
    };

    // The data members.
    receiver_t receiver_;
    socket_t& socket_;
    epoll_context& context_;
    const op_vtable* op_vtable_;
    const op_type op_type_;
    std::atomic<std::uint32_t> state_;
    system_error2::system_code ec_;
    exec::__manual_lifetime<typename stop_token::template callback_type<cancel_callback>>
        stop_callback_;

    // Constructor.
    __t(receiver_t receiver, basic_socket<Protocol>& socket, const op_vtable& vtable,
        op_type otype) noexcept
        : receiver_(static_cast<receiver_t&&>(receiver)),
          socket_(static_cast<socket_t&>(socket)),
          context_(static_cast<epoll_context&>(socket.context())),
          op_vtable_(&vtable),
          op_type_(otype),
          state_(0),
          ec_(errc::success),
          stop_callback_() {}

    // `start` customization point object.
    // The operation is submitted to the corresponding queue based on the thread that submitted it.
    // The start operation is non-blocking and deep recursive safe.
    friend void tag_invoke(stdexec::start_t, __t& self) noexcept { self.start_impl(); }

    constexpr void start_impl() noexcept {
      if (!context_.is_running_on_io_thread()) {
        static_cast<completion_op*>(this)->execute_ = &__t::on_schedule_complete;
        context_.schedule_remote(static_cast<completion_op*>(this));
      } else {
        perform();
      }
    }

    // epoll_context starts to execute this operation in the io thread.
    static void on_schedule_complete(operation_base* op) noexcept {
      static_cast<__t*>(static_cast<completion_op*>(op))->perform();
    }

    // This function is not thread safe, it must be executed in io thread.
    constexpr void perform() noexcept {
      assert(context_.is_running_on_io_thread());
      assert(!static_cast<completion_op*>(this)->enqueued_.load());

      // According to P2762, the operation should be performed once in first.
      // P2762: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2762r0.pdf
      op_vtable_->perform(this);
      if (ec_ == errc::resource_unavailable_try_again || ec_ == errc::operation_would_block) {
        ec_ = errc::success;
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          stop_callback_.__construct(stdexec::get_stop_token(stdexec::get_env(receiver_)),
                                     cancel_callback{*this});
        }
        add_events();
        static_cast<completion_op*>(this)->execute_ = wakeup;
        return;
      }

      // Operation has been cancelled by a remote thread.
      auto old_state = state_.fetch_add(operation_ended, std::memory_order_acq_rel);
      if ((old_state & request_stopped_mask) != 0) {
        // The other thread is responsible for enqueueing the operation completion and set stopped
        // to downstream receiver and deregister peer socket fd from epoll.
        return;
      }

      op_vtable_->complete(this);
    }

    // Handle epoll event.
    static void wakeup(operation_base* op) noexcept {
      assert(op->enqueued_.load() == false);

      auto& self = *static_cast<__t*>(static_cast<completion_op*>(op));
      self.stop_callback_.__destruct();
      self.remove_events();

      // Operation has been cancelled by a remote thread.
      auto old_state = self.state_.fetch_add(operation_ended, std::memory_order_acq_rel);
      if ((old_state & request_stopped_mask) != 0) {
        return;
      }

      // An socket operation is performed to obtain the result of this operation.
      self.op_vtable_->perform(&self);
      self.op_vtable_->complete(&self);
    }

    // Send the stopped signal to the downstream receiver.
    static void complete_with_stop(operation_base* op) noexcept {
      assert(static_cast<stop_op*>(op)->enqueued_ == false);

      auto& self = *static_cast<__t*>(static_cast<stop_op*>(op));
      if (!static_cast<completion_op&>(self).enqueued_.load()) {
        if constexpr (!stdexec::unstoppable_token<stop_token>) {
          stdexec::set_stopped(static_cast<receiver_t&&>(self.receiver_));
        } else {
          // This should never be called if stop is not possible.
          assert(false);
        }
      } else {
        // The current operation has been committed to the queue, we do not offer to delete the
        // operation from the queue, so wait for the operation to be completed, then the stop
        // operation will be executed sequentially, at which point the operation will be stopped,
        // and no further completion operation will be committed to the queue.
        static_cast<stop_op&>(self).execute_ = &complete_with_stop;
        self.context_.schedule_local(static_cast<stop_op*>(op));
      }
    }

    // The remote thread requests that this operation should be stopped.
    void request_stop() noexcept {
      auto old_state = state_.fetch_add(request_stopped, std::memory_order_acq_rel);
      if ((old_state & operation_ended_mask) == 0) {
        // Io operation not yet completed.
        // Note that we removed the peer descriptor registered on epoll, `wakeup` won't be called.
        remove_events();

        // We are responsible for scheduling the completion of this io operation.
        static_cast<stop_op*>(this)->execute_ = &complete_with_stop;
        context_.schedule_remote(static_cast<stop_op*>(this));
      }
    }

    // Register epoll events. Returns whether the execution was successful
    constexpr bool add_events() {
      epoll_event event{.data = {.ptr = static_cast<completion_op*>(this)}};
      if (op_type_ == op_type::op_read) {
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
      } else {
        event.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
      }
      if (::epoll_ctl(context_.epoll_fd_, EPOLL_CTL_ADD, socket_.native_handle(), &event) != 0) {
        ec_ = system_error2::posix_code::current();
        return false;
      }
      return true;
    }

    // Unregister epoll events. Returns whether the execution was successful
    constexpr bool remove_events() {
      epoll_event event = {};
      if (::epoll_ctl(context_.epoll_fd_, EPOLL_CTL_DEL, socket_.native_handle(), &event) != 0) {
        ec_ = system_error2::posix_code::current();
        return false;
      }
      return true;
    }
  };
};

}  // namespace net::__epoll

#endif  // NET_SRC_EPOLL_SOCKET_IO_BASE_OPERATION_HPP_
