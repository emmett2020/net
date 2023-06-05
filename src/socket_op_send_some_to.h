#ifndef SRC_NET_REACTIVE_SOCKET_SEND_SOME_TO_OP_H_
#define SRC_NET_REACTIVE_SOCKET_SEND_SOME_TO_OP_H_

#include <exception>

#include "basic_socket.h"
#include "buffer_sequence_adapter.h"
#include "epoll_context.h"
#include "epoll_reactor.h"
#include "ip/udp.h"
#include "reactor_op.h"
#include "socket_base.h"
#include "socket_ops.h"
#include "stdexec/execution.hpp"

namespace net {
namespace ex = stdexec;
namespace _sendto_some {
template <typename Receiver, typename Protocol, typename ConstBufferSequence>
class ReactiveSocketSendSomeToOp : public ReactorOp {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  // TODO: message flags
  // Constructor
  ReactiveSocketSendSomeToOp(Receiver&& receiver, Socket&& peer, const ConstBufferSequence& buffers,
                             Endpoint&& peer_endpoint)
      : ReactorOp(std::error_code{}, &ReactiveSocketSendSomeToOp::doPerform,
                  &ReactiveSocketSendSomeToOp::doComplete),
        receiver_(std::move(receiver)),
        peer_socket_(std::move(peer)),
        peer_endpoint_(std::move(peer_endpoint)),
        buffers_(buffers) {}

  friend void tag_invoke(ex::start_t, ReactiveSocketSendSomeToOp& self) noexcept {
    ::printf("SendToOp. 'Start' executed. reactor: %p\n", &self.peer_socket_.context().reactor());

    // !non blocking 还要补充一些
    socket_ops::set_internal_non_blocking(self.peer_socket_.descriptorState()->descriptor(),
                                          self.peer_socket_.descriptorState()->state(), true,
                                          self.ec_);

    self.peer_socket_.context().reactor().startOp(
        Reactor::kWriteOp, self.peer_socket_.descriptorState(), &self, false);
  }

  static Status doPerform(ReactorOp* base) {
    ReactiveSocketSendSomeToOp* self(static_cast<ReactiveSocketSendSomeToOp*>(base));
    using bufs_type = buffer_sequence_adapter<const_buffer, ConstBufferSequence>;

    auto state = self->peer_socket_.descriptorState();
    std::size_t len = self->peer_endpoint_.capacity();

    Status result;
    if (bufs_type::is_single_buffer) {
      fmt::print("sendto1\n");
      result = socket_ops::non_blocking_sendto1(
                   state->descriptor(), bufs_type::first(self->buffers_).data(),
                   bufs_type::first(self->buffers_).size(), self->flags_,
                   self->peer_endpoint_.data(), len, self->ec_, self->bytes_transferred_)
                   ? kDone
                   : kNotDone;
      if (result == kDone) {
        if ((state->state() & socket_ops::kStreamOriented) != 0) {
          if (self->bytes_transferred_ < bufs_type::first(self->buffers_).size()) {
            result = kDoneAndExhausted;
          }
        }
      }
    } else {
      fmt::print("sendto\n");
      bufs_type bufs(self->buffers_);
      result = socket_ops::non_blocking_sendto(state->descriptor(), bufs.buffers(), bufs.count(),
                                               self->flags_, self->peer_endpoint_.data(), len,
                                               self->ec_, self->bytes_transferred_)
                   ? kDone
                   : kNotDone;

      if (result == kDone) {
        if ((state->state() & socket_ops::kStreamOriented) != 0) {
          if (self->bytes_transferred_ < bufs.total_size()) {
            result = kDoneAndExhausted;
          }
        }
      }
    }

    ::printf("SendToOp. do perform end. result: %d\n", result);
    return result;
  }

  static void doComplete(void* owner, SchedulerOperation* base, const std::error_code& ec,
                         std::size_t /* bytes_transferred */) {
    auto& op = *static_cast<ReactiveSocketSendSomeToOp*>(base);

    if (owner) {
      ::printf("SendToOp. 'set value'. bytes_transferred: %lu. ec msg: %s\n", op.bytes_transferred_,
               ec.message().c_str());
      // !TODO: 做一些收尾，比如close操作

      if (op.ec_.value() == 0) {
        ex::set_value(std::move(op.receiver_), std::move(op.peer_socket_), op.bytes_transferred_);
      } else {
        ex::set_error(std::move(op.receiver_), std::make_exception_ptr(std::move(op.ec_)));
      }
    }
  }

 private:
  Receiver receiver_;
  Socket&& peer_socket_;
  const ConstBufferSequence& buffers_;
  Endpoint peer_endpoint_;
  typename SocketBase<Protocol>::MessageFlags flags_{0};  // TODO: message flags
};

template <typename Protocol, typename ConstBufferSequence>
class SendSomeToSender {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  using completion_signature =
      ex::completion_signatures<ex::set_value_t(Socket&&, uint64_t),
                                ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>;

  // Constructor
  SendSomeToSender(Socket&& peer_socket, const ConstBufferSequence& buffers,
                   Endpoint&& peer_endpoint)
      : peer_socket_(std::move(peer_socket)),
        buffers_(buffers),
        peer_endpoint_(std::move(peer_endpoint)) {}

  // unifex::connect
  template <typename Sender, typename Receiver>
  friend auto tag_invoke(ex::connect_t, Sender&& self, Receiver&& receiver) {
    return ReactiveSocketSendSomeToOp{std::move(receiver), std::move(self.peer_socket_),
                                      self.buffers_, std::move(self.peer_endpoint_)};
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, SendSomeToSender&& self) { return {}; }

  template <typename Env>
  friend auto tag_invoke(ex::get_completion_signatures_t, const SendSomeToSender& self, Env)
      -> completion_signature;

 private:
  Socket&& peer_socket_;
  const ConstBufferSequence& buffers_;
  Endpoint peer_endpoint_;
};

class AsyncSendToSome {
 public:
  // TODO:这里有问题，模板参数都不全
  template <typename ConstBufferSequence, typename Protocol>
  constexpr auto operator()(const ConstBufferSequence& buffers,
                            ip::BasicEndpoint<Protocol> endpoint) const
      -> ex::__binder_back<AsyncSendToSome, const ConstBufferSequence&> {
    return {{}, {}, buffers};
  }

  template <typename Predecessor, typename ConstBufferSequence, typename Protocol>
  ex::sender auto operator()(Predecessor&& prev, const ConstBufferSequence& buffers,
                             ip::BasicEndpoint<Protocol> peer_endpoint) const {
    return ex::let_value(std::move(prev), [&buffers, &peer_endpoint](auto&& peer_socket) {
      return SendSomeToSender{std::move(peer_socket), buffers, std::move(peer_endpoint)};
    });
  }
};

}  // namespace _sendto_some

inline constexpr net::_sendto_some::AsyncSendToSome async_send_to_some{};

}  // namespace net

#endif  // #define SRC_NET_REACTIVE_SOCKET_SEND_SOME_TO_OP_H_
