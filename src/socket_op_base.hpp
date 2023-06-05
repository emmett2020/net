#ifndef NET_SRC_SOCKET_OP_BASE_HPP_
#define NET_SRC_SOCKET_OP_BASE_HPP_

#include "basic_socket.hpp"
#include "io_epoll_context.hpp"
#include "socket_option.hpp"
#include "status-code/system_code.hpp"
#include "stdexec/execution.hpp"

// TODO: !x!
// 1. 去掉定制化的NetError，我们力求精简，且只在linux上使用 , done
// 2. 仿造io_uring_context更改
// 3. 去掉其他
// 4. 加上concept限制，epoll context仅可使用epoll socket

namespace net {
template <typename Receiver, typename Protocol>
class epoll_context::socket_base_operation : private epoll_context::completion_op,
                                             private epoll_context::stop_op {
 public:
  using socket_type = typename Protocol::socket_type;
  using endpoint_type = typename Protocol::endpoint_type;
  using stop_token = stdexec::stop_token_of_t<env_of_t<Receiver>>;

  // Use these to synchronize the remote thread and the io thread.
  static constexpr std::uint32_t operation_ended = 0x00010000;
  static constexpr std::uint32_t operation_ended_mask = 0xFFFF0000;
  static constexpr std::uint32_t request_stopped = 0x1;
  static constexpr std::uint32_t request_stopped_mask = 0xFFFF;

  struct op_vtable {
    /*
     1. add_events      // 准备执行. 注册epoll等操作.
     2. perform         // 执行一次socket操作. 通过返回值确定是否继续执行.
     3. del_events      // 回收资源. 取消注册epoll等.
     4. complete        // 执行结束，notify
    */
    void (*add_events)(socket_base_operation*) noexcept = nullptr;
    bool (*perform)(socket_base_operation*) noexcept = nullptr;
    void (*del_events)(socket_base_operation*) noexcept = nullptr;
    void (*complete)(socket_base_operation*) noexcept = nullptr;
  };

  enum class op_type { op_read = 1, op_accept = 1, op_write = 2, op_connect = 2 };

  // TODO: 能不能加个ec？
  void add_events() {
    epoll_event event{};
    if constexpr (op_type_ == op_type::op_read || op_type_ == op_type::op_accept) {
      event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
    } else {
      event.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
    }
    event.data.ptr = this;
    ::epoll_ctl(context_.epoll_fd_, EPOLL_CTL_ADD, socket_.native_handle(), &event);
  }

  void remove_events() {
    epoll_event event = {};
    ::epoll_ctl(context_.epoll_fd_, EPOLL_CTL_DEL, socket_.native_handle(), &event);
  }

  // Prepare.
  socket_base_operation(Receiver&& receiver, socket_type&& socket, const op_vtable& op_vtable,
                        const op_type& op_type)
      : receiver_(static_cast<Receiver&&>(receiver)),
        socket_(static_cast<socket_type&&>(socket)),
        op_vtable_(&op_vtable),
        state_(0),
        context_(socket.context()),
        op_type_(op_type),
        ec_() {}

  // TODO: 根据p2762 start要首先执行一次perform_，然后执行不成功时再入队
  // `start` customization point object.
  // The operation is submitted to the corresponding queue based on the thread that submitted it.
  // The start operation is non-blocking and deep recursive safe.
  friend void tag_invoke(stdexec::start_t, socket_base_operation& self) noexcept {
    if (!self.context_.is_running_on_io_thread()) {
      static_cast<completion_op*>(self)->execute_ = &socket_base_operation::on_schedule_complete;
      self.context_.schedule_remote(static_cast<operation_base*>(self));
    } else {
      self.prepare();
    }
  }

  // epoll_context starts to execute this operation in the io thread.
  static void on_schedule_complete(operation_base* op) noexcept {
    static_cast<socket_base_operation*>(op)->prepare();
  }

  // This function is not thread safe, it must be executed in io thread.
  void prepare() noexcept {
    assert(context_.is_running_on_io_thread());
    assert(!static_cast<completion_op*>(this)->enqueued_.load());

    if constexpr (!stdexec::unstoppable_token<stop_token>) {
      stop_callback_.construct(stdexec::get_stop_token(stdexec::get_env(receiver_)),
                               cancel_callback{this});
    }
    op_vtable_->add_events_(this);
    static_cast<completion_op*>(this)->execute_ = wakeup;
  }

  // Event handler.
  static void wakeup(operation_base* op) noexcept {
    assert(op->enqueued_.load() == false);
    auto& self = *static_cast<socket_base_operation*>(op);

    if ((self.state_ & socket_base_operation::request_stopped_mask) != 0) {
      // Io has been cancelled by a remote thread.
      // The other thread is responsible for enqueueing the operation completion and set stopped to
      // downstream receiver and deregister peer socket fd from epoll.
      return;
    }

    // An socket operation is performed to obtain the result of this operation.
    if (!self.perform_(self)) {
      // Wait for the next epoll notification.
      return;
    }

    // Deregister request stop callback.
    self.stop_callback_.destruct();

    auto old_state =
        self.state_.fetch_add(socket_base_operation::operation_ended, std::memory_order_acq_rel);
    if ((old_state & socket_base_operation::request_stopped_mask) != 0) {
      // Io has been cancelled by a remote thread.
      // The other thread is responsible for enqueueing the operation completion and set stopped to
      // downstream receiver and deregister peer socket fd from epoll.
      return;
    }

    // Remove socket socket from epoll.
    // self.op_vtable->del_events_(self);

    // Send signals to downstream receiver.
    // self->notify_receiver_(self);
    self.op_vtable->complete_(self);
  }

  // Send the stopped signal to the downstream receiver.
  static void complete_with_stop(operation_base* op) noexcept {
    assert(static_cast<stop_op*>(op)->enqueued_ == false);

    auto& self = *static_cast<socket_base_operation*>(static_cast<stop_op*>(op));
    if (!static_cast<completion_op&>(self).enqueued_.load()) {
      if constexpr (!stdexec::unstoppable_token<stop_token>) {
        set_stopped((Receiver &&) self.receiver_);
      } else {
        // This should never be called if stop is not possible.
        assert(false);
      }
    } else {
      // The current operation has been committed to the queue, we do not offer to delete the
      // operation from the queue, so wait for the operation to be completed, then the stop
      // operation will be executed sequentially, at which point the operation will be stopped, and
      // no further completion operation will be committed to the queue.
      self.execute_ = &complete_with_stop;
      self.context_.schedule_local(static_cast<stop_op*>(op));
    }
  }

  // The remote thread requests that this operation should be stopped.
  void request_stop() noexcept {
    auto old_state = state_.fetch_add(request_stopped, std::memory_order_acq_rel);
    if ((old_state & socket_base_operation::operation_ended_mask) == 0) {
      // Io operation not yet completed.
      // Since We removed the peer fd registered on epoll. `wakeup` will not be
      // called, so we put this operation on the completion queue.
      // epoll_event event = {};
      // (void)::epoll_ctl(context_.epoll_fd_, EPOLL_CTL_DEL, socket_->fd(), &event);
      op_vtable_->del_events_(this);

      // We are responsible for scheduling the completion of this io operation.
      static_cast<stop_op*>(this)->execute_ = &complete_with_stop;
      context_.schedule_remote(static_cast<stop_op*>(this));
    }
  }

 private:
  friend epoll_context;

  struct cancel_callback {
    socket_base_operation& op_;
    void operator()() noexcept { op_.request_stop(); }
  };

  // The downstream receiver
  Receiver receiver_;

  // socket_type.
  socket_type socket_;

  const op_vtable* op_vtable_;

  // TODO: 放到io里
  // Total number of bytes transferred, to be passed to the downstream receiver when this operation
  // is successfully executed.
  uint64_t bytes_transferred_;

  // Error code, to be passed to the downstream receiver when error occurs.
  system_error2::system_code ec_;

  // Stop callback will be executed when request stopped.
  __manual_lifetime<typename stop_token::template callback_type<cancel_callback>> stop_callback_;

  // The context.
  epoll_context& context_;

  // The state to record this operation is executing io operation or has been cancelled.
  std::atomic<std::uint32_t> state_;

  // The type of this operation.
  const op_type& op_type_;
};

}  // namespace net

#endif  // NET_SRC_SOCKET_OP_BASE_HPP_
