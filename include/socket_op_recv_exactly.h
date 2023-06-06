#ifndef SRC_NET_REACTIVE_SOCKET_RECV_EXACTLY_OP_H_
#define SRC_NET_REACTIVE_SOCKET_RECV_EXACTLY_OP_H_

#include <exception>
#include <stdexec/execution.hpp>

#include "basic_socket.h"
#include "epoll_context.h"
#include "epoll_reactor.h"
#include "reactor_op.h"
#include "socket_ops.h"

namespace net {
namespace ex = stdexec;

namespace _recv_exactly {
template <typename Receiver, typename Protocol, typename MutableBufferSequence>
class ReactiveSocketRecvExactlyOp : public ReactorOp {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  // TODO: message flags
  // Constructor
  ReactiveSocketRecvExactlyOp(Receiver&& receiver, Socket&& peer,
                              const MutableBufferSequence& buffer, uint64_t bytes_to_read)
      : ReactorOp(std::error_code{}, &ReactiveSocketRecvExactlyOp::doPerform,
                  &ReactiveSocketRecvExactlyOp::doComplete),
        receiver_(std::move(receiver)),
        peer_socket_(std::move(peer)),
        buffer_(buffer),
        bytes_to_read_(bytes_to_read) {}

  friend void tag_invoke(ex::start_t, ReactiveSocketRecvExactlyOp& self) noexcept {
    ::printf("RecvOp. 'Start' executed. reactor: %p\n", &self.peer_socket_.context().reactor());

    if (self.bytes_to_read_ > self.buffer_.size()) {
      // set_error
    }

    // 考虑到实现复杂度，recv_exactly_op 暂时仅支持单buffer
    using bufs_type = buffer_sequence_adapter<mutable_buffer, MutableBufferSequence>;
    if (!bufs_type::is_single_buffer) {
      // set_error
    }

    EpollReactor::OpTypes t = self.flags_ & SocketBase<Protocol>::kMessageOutOfBand
                                  ? Reactor::kExceptOp
                                  : Reactor::kReadOp;

    // !这里可以改成Protocol::SetDescriptorOptionForRecv()
    // !non blocking 还要补充一些
    socket_ops::set_internal_non_blocking(self.peer_socket_.descriptorState()->descriptor(),
                                          self.peer_socket_.descriptorState()->state(), true,
                                          self.ec_);

    self.peer_socket_.context().reactor().startOp(t, self.peer_socket_.descriptorState(), &self,
                                                  false);
  }

  static Status doPerform(ReactorOp* base) {
    ReactiveSocketRecvExactlyOp* self(static_cast<ReactiveSocketRecvExactlyOp*>(base));
    using bufs_type = buffer_sequence_adapter<mutable_buffer, MutableBufferSequence>;

    auto state = self->peer_socket_.descriptorState();

    Status result = Status::kNotDone;
    uint64_t bytes_transferred = 0;
    // char temp[1024 * 8];  // TODO: 这个值应该调整

    fmt::print("recv1\n");

    // !recv exactly 补充上多buffer
    auto left_buffer = buffer(static_cast<char*>(self->buffer_.data()) + self->bytes_transferred_,
                              self->bytes_to_read_ - self->bytes_transferred_);
    bool ok = socket_ops::non_blocking_recv1(
        state->descriptor(), bufs_type::first(left_buffer).data(),
        bufs_type::first(left_buffer).size(), self->flags_,
        (state->state() & socket_ops::kStreamOriented) != 0, self->ec_, bytes_transferred);

    if (ok) {
      // 只保留这么多，其他的舍弃
      if (self->bytes_transferred_ + bytes_transferred >= self->bytes_to_read_) {
        self->bytes_transferred_ = self->bytes_to_read_;
        result = kDone;
      } else {
        self->bytes_transferred_ += bytes_transferred;
        result = kNotDone;
      }
    }

    if (result == kDone) {
      if ((state->state() & socket_ops::kStreamOriented) != 0) {
        if (self->bytes_transferred_ == 0) {
          result = kDoneAndExhausted;
        }
      }
    }

    fmt::print("bytes transferred: {}\n", self->bytes_transferred_);
    ::printf("RecvOp. do perform end. result: %d\n", result);
    return result;
  }

  static void doComplete(void* owner, SchedulerOperation* base, const std::error_code& ec,
                         std::size_t /* event */) {
    auto& op = *static_cast<ReactiveSocketRecvExactlyOp*>(base);

    if (owner) {
      if (op.ec_.value() == 0) {
        ::printf("RecvOp. 'set value'. bytes_transferred: %lu. ec msg: %s\n", op.bytes_transferred_,
                 op.ec_.message().c_str());
        // !TODO: 做一些收尾，比如close操作
        ex::set_value(std::move(op.receiver_), std::move(op.peer_socket_), op.bytes_transferred_);
      } else {
        ::printf("RecvOp. 'set error'. bytes_transferred: %lu. ec msg: %s\n", op.bytes_transferred_,
                 op.ec_.message().c_str());

        // TODO: set_error会抛异常，不符合我们的场景，用户读数据很容易失败，不用抛异常再去try-catch
        // std::exception_ptr eptr = std::current_exception();
        ex::set_error(std::move(op.receiver_), std::make_exception_ptr(std::move(op.ec_)));
        // unifex::set_done(std::move(op.receiver_));
      }
    }
  }

 private:
  Receiver receiver_;
  Socket&& peer_socket_;
  const MutableBufferSequence& buffer_;
  typename SocketBase<Protocol>::MessageFlags flags_{0};  // TODO: message flags
  uint64_t bytes_to_read_;
};

template <typename Protocol, typename MutableBufferSequence>
class RecvExactlySender {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  using completion_signature =
      ex::completion_signatures<ex::set_value_t(Socket&&, uint64_t),
                                ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>;

  // Constructor
  RecvExactlySender(Socket&& peer_socket, const MutableBufferSequence& buffer,
                    uint64_t bytes_to_read)
      : peer_socket_(std::move(peer_socket)), buffer_(buffer), bytes_to_read_(bytes_to_read) {}

  // unifex::connect
  template <typename Sender, typename Receiver>
  friend auto tag_invoke(ex::connect_t, Sender&& self, Receiver&& receiver) {
    return ReactiveSocketRecvExactlyOp{std::move(receiver), std::move(self.peer_socket_),
                                       self.buffer_, self.bytes_to_read_};
  }

  template <typename Self, typename Env>
  friend ex::empty_env tag_invoke(ex::get_env_t, Self&&, Env) {
    return {};
  }

  template <typename Env>
  friend auto tag_invoke(ex::get_completion_signatures_t, const RecvExactlySender& self, Env)
      -> completion_signature;

 private:
  Socket&& peer_socket_;
  const MutableBufferSequence& buffer_;
  uint64_t bytes_to_read_;
};

// TODO: 调用方有责任保证buffer的长度大于需要读入的长度
class AsyncRecvExactly {
 public:
  template <typename MutableBufferSequence>
  constexpr auto operator()(const MutableBufferSequence& buffer, uint64_t bytes_to_read) const
      -> ex::__binder_back<AsyncRecvExactly, const MutableBufferSequence&, uint64_t> {
    return {{}, {}, {buffer, bytes_to_read}};
  }

  template <typename Predecessor, typename MutableBufferSequence>
  constexpr ex::sender auto operator()(Predecessor&& pre, const MutableBufferSequence& buffer,
                                       uint64_t bytes_to_read) const {
    return ex::let_value(std::move(pre), [&buffer, bytes_to_read](auto&& peer_socket) {
      return RecvExactlySender{std::move(peer_socket), buffer, bytes_to_read};
    });
  }
};

}  // namespace _recv_exactly
inline constexpr net::_recv_exactly::AsyncRecvExactly async_recv_exactly{};

}  // namespace net

#endif  // SRC_NET_REACTIVE_SOCKET_RECV_OP_H_
