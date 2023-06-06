#ifndef SRC_NET_REACTIVE_SOCKET_RECVFROM_SOME_OP_H_
#define SRC_NET_REACTIVE_SOCKET_RECVFROM_SOME_OP_H_

#include <exception>

#include "basic_socket.h"
#include "epoll_context.h"
#include "epoll_reactor.h"
#include "ip/basic_endpoint.h"
#include "reactor_op.h"
#include "socket_ops.h"
#include "stdexec/execution.hpp"

namespace net {
namespace _recvfrom_some {
namespace ex = stdexec;

template <typename Receiver, typename Protocol, typename MutableBufferSequence>
class ReactiveSocketRecvSomeFromOp : public ReactorOp {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  // TODO: message flags
  // Constructor
  ReactiveSocketRecvSomeFromOp(Receiver&& receiver, Socket&& peer,
                               const MutableBufferSequence& buffers, Endpoint&& peer_endpoint)
      : ReactorOp(std::error_code{}, &ReactiveSocketRecvSomeFromOp::doPerform,
                  &ReactiveSocketRecvSomeFromOp::doComplete),
        receiver_(std::move(receiver)),
        peer_socket_(std::move(peer)),
        peer_endpoint_(std::move(peer_endpoint)),
        buffers_(buffers) {}

  friend void tag_invoke(ex::start_t, ReactiveSocketRecvSomeFromOp& self) noexcept {
    ::printf("RecvFromOp. 'Start' executed. reactor: %p\n", &self.peer_socket_.context().reactor());
    EpollReactor::OpTypes t = self.flags_ & SocketBase<Protocol>::kMessageOutOfBand
                                  ? Reactor::kExceptOp
                                  : Reactor::kReadOp;
    // !non blocking 还要补充一些
    socket_ops::set_internal_non_blocking(self.peer_socket_.descriptorState()->descriptor(),
                                          self.peer_socket_.descriptorState()->state(), true,
                                          self.ec_);

    self.peer_socket_.context().reactor().startOp(t, self.peer_socket_.descriptorState(), &self,
                                                  false);
  }

  static Status doPerform(ReactorOp* base) {
    ReactiveSocketRecvSomeFromOp* self(static_cast<ReactiveSocketRecvSomeFromOp*>(base));
    using bufs_type = buffer_sequence_adapter<mutable_buffer, MutableBufferSequence>;
    ::printf("RecvFromOp Executed\n");

    auto state = self->peer_socket_.descriptorState();

    std::size_t addr_len = self->peer_endpoint_.capacity();
    Status result;
    if (bufs_type::is_single_buffer) {
      result = socket_ops::non_blocking_recvfrom1(
                   state->descriptor(), bufs_type::first(self->buffers_).data(),
                   bufs_type::first(self->buffers_).size(), self->flags_,
                   self->peer_endpoint_.data(), &addr_len, self->ec_, self->bytes_transferred_)
                   ? kDone
                   : kNotDone;
    } else {
      bufs_type bufs(self->buffers_);
      result = socket_ops::non_blocking_recvfrom(state->descriptor(), bufs.buffers(), bufs.count(),
                                                 self->flags_, self->peer_endpoint_.data(),
                                                 &addr_len, self->ec_, self->bytes_transferred_)
                   ? kDone
                   : kNotDone;
    }

    ::printf("After recv port: %d\n", self->peer_endpoint_.port());

    if (result && !self->ec_) {
      self->peer_endpoint_.resize(addr_len);
      self->new_endpoint_ = self->peer_endpoint_;
    }

    return result;
  }

  static void doComplete(void* owner, SchedulerOperation* base, const std::error_code& ec,
                         std::size_t /* event */) {
    auto& op = *static_cast<ReactiveSocketRecvSomeFromOp*>(base);

    if (owner) {
      if (op.ec_.value() == 0) {
        ::printf("RecvFromOp. 'set value'. bytes_transferred: %lu. ec msg: %s\n",
                 op.bytes_transferred_, op.ec_.message().c_str());
        // !TODO: 做一些收尾，比如close操作
        ex::set_value(std::move(op.receiver_), std::move(op.peer_socket_), op.bytes_transferred_,
                      op.new_endpoint_);
      } else {
        ::printf("RecvFromOp. 'set error'. bytes_transferred: %lu. ec msg: %s\n",
                 op.bytes_transferred_, op.ec_.message().c_str());

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
  const MutableBufferSequence& buffers_;
  Endpoint peer_endpoint_;
  Endpoint new_endpoint_;
  typename SocketBase<Protocol>::MessageFlags flags_{0};  // TODO: message flags
};

template <typename Protocol, typename MutableBufferSequence>
class RecvSomeFromSender {
 public:
  using Socket = BasicSocket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  using completion_signature =
      ex::completion_signatures<ex::set_value_t(Socket&&, uint64_t, Endpoint&&),
                                ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>;

  // Constructor
  RecvSomeFromSender(Socket&& peer_socket, const MutableBufferSequence& buffers,
                     Endpoint&& peer_endpoint)
      : peer_socket_(std::move(peer_socket)),
        buffers_(buffers),
        peer_endpoint_(std::move(peer_endpoint)) {}

  template <typename Sender, typename Receiver>
  friend auto tag_invoke(ex::connect_t, Sender&& self, Receiver&& receiver) {
    return ReactiveSocketRecvSomeFromOp{std::move(receiver), std::move(self.peer_socket_),
                                        self.buffers_, std::move(self.peer_endpoint_)};
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, RecvSomeFromSender&& self) { return {}; }

  template <typename Env>
  friend auto tag_invoke(ex::get_completion_signatures_t, const RecvSomeFromSender& self, Env)
      -> completion_signature;

 private:
  Socket&& peer_socket_;
  const MutableBufferSequence& buffers_;
  Endpoint peer_endpoint_;
};

class AsyncRecvSomeFrom {
 public:
  // TODO:这里有问题，模板参数都不全
  template <typename MutableBufferSequence, typename Protocol>
  constexpr auto operator()(const MutableBufferSequence& buffers,
                            ip::BasicEndpoint<Protocol> endpoint) const
      -> ex::__binder_back<AsyncRecvSomeFrom, const MutableBufferSequence&> {
    return {{}, {}, buffers};
  }

  // TODO: 这里定义一个concept，确保Predecessor执行完后传入的参数是Socket且支持context函数
  template <typename Predecessor, typename MutableBufferSequence, typename Protocol>
  ex::sender auto operator()(Predecessor&& prev, const MutableBufferSequence& buffers,
                             ip::BasicEndpoint<Protocol> peer_endpoint) const {
    return ex::let_value(std::move(prev), [&buffers, &peer_endpoint](auto&& peer_socket) {
      return RecvSomeFromSender{std::move(peer_socket), buffers, std::move(peer_endpoint)};
    });
  }

  // template <typename Predecessor, typename Protocol, typename MutableBufferSequence>
  // ex::sender auto operator()(Predecessor&& prev, const MutableBufferSequence& buffers,
  //                            typename Protocol::Endpoint peer_endpoint) const {
  //   return ex::let_value(std::move(prev), [&buffers, &peer_endpoint](auto&& peer_socket) {
  //     return RecvSomeFromSender{std::move(peer_socket), peer_endpoint, buffers};
  //   });
  // }
};

};  // namespace _recvfrom_some

// async_recv_some_from()
inline constexpr net::_recvfrom_some::AsyncRecvSomeFrom async_recv_some_from{};

}  // namespace net

#endif  // SRC_NET_REACTIVE_SOCKET_RECVFROM_SOME_OP_H_
