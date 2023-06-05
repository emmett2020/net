#ifndef NET_SOCKET_READ_OP1_H_
#define NET_SOCKET_READ_OP1_H_

#include <exception>

#include "basic_socket.h"
#include "epoll_context.h"
#include "epoll_reactor.h"
#include "io_epoll_context.hpp"
#include "reactor_op.h"
// #include "socket_io_base_op.hpp"
#include "socket_ops.h"
#include "stdexec/execution.hpp"

namespace net {
template <typename Receiver, typename Protocol, typename MutableBuffer>
class socket_recv_some_op : public epoll_context::socket_base_operation<Receiver, Protocol> {
 public:
  using Base = epoll_context::socket_base_operation<Receiver, Protocol>;
  using Socket = basic_socket<Protocol>;

  // non_blocking_recv.
  static bool non_blocking_recv(Base* base) {
    socket_recv_some_op* self(static_cast<socket_recv_some_op*>(base));
    using bufs_type = buffer_sequence_adapter<mutable_buffer, MutableBuffer>;

    IoResult res{0};
    if (bufs_type::is_single_buffer) {
      res = self->socket_.non_blocking_recv(bufs_type::first(self->buffers_).data(),
                                            bufs_type::first(self->buffers_).size(),  //
                                            self->flags_);
    } else {
      bufs_type bufs(self->buffers_);
      res = self->socket_.non_blocking_recvmsg(bufs, bufs.count(), self->flags_);
    }

    if (!res.has_value()) {
      return false;
    }
    self->bytes_transferred_ += *res;
    return true;
  }

  static void complete(Base* base) {
    auto& self = *static_cast<socket_recv_some_op*>(base);
    if (self.ec_ == NetError::kOperationAborted) {
      set_stopped((Receiver &&) self.receiver_);
    } else if (self.ec_ == NetError::kNoError) {
      set_value((Receiver &&) self.receiver_, self.bytes_transferred_);
    } else {
      set_error((Receiver &&) self.receiver_, (std::error_code &&) self.ec_);
    }
  }

  static constexpr typename Base::op_type otype = Base::op_type::op_read;
  static constexpr typename Base::op_vtable op_vtable{&non_blocking_recv, &complete};

  // Constructor
  socket_recv_some_op(Receiver&& receiver, Socket&& socket, const MutableBuffer& buffers)
      : Base((Receiver &&) receiver, (Socket &&) socket, op_vtable, otype),
        buffers_(buffers),
        bytes_transferred_(0) {}

 private:
  size_t bytes_transferred_;
  const MutableBuffer& buffers_;
};

template <typename Protocol, typename Context, typename MutableBuffer>
class RecvSomeSender {
 public:
  using Socket = basic_socket<Protocol>;
  using Endpoint = typename Protocol::Endpoint;

  using completion_signatures =
      stdexec::completion_signatures<stdexec::set_value_t(uint64_t),
                                     stdexec::set_error_t(std::exception_ptr),
                                     stdexec::set_stopped_t()>;

  // Constructor
  RecvSomeSender(Socket&& peer_socket, const MutableBuffer& buffers)
      : socket_((Socket &&) peer_socket), buffers_(buffers) {}

  template <typename Sender, typename Receiver>
  friend auto tag_invoke(stdexec::connect_t, Sender&& self, Receiver&& receiver) {
    return socket_recv_some_op{(Receiver &&) receiver, (Socket &&) self.socket_, self.buffers_};
  }

  friend stdexec::empty_env tag_invoke(stdexec::get_env_t, RecvSomeSender&& self) { return {}; }

  template <typename Env>
  friend auto tag_invoke(stdexec::get_completion_signatures_t, const RecvSomeSender& self, Env)
      -> completion_signatures;

 private:
  Socket&& socket_;
  const MutableBuffer& buffers_;
};

//
class AsyncRecvSome {
 public:
  template <typename Protocol, typename Context, typename MutableBuffer>
  constexpr auto operator()(basic_socket<Protocol>&& peer_socket,
                            const MutableBuffer& buffers) const
      -> stdexec::__binder_back<AsyncRecvSome, Protocol, Context, const MutableBuffer&> {
    return {{}, {}, {}, buffers};
  }

  // TODO: 这里定义一个concept，确保Predecessor执行完后传入的参数是Socket且支持context函数
  template <stdexec::sender Predecessor, typename Protocol, typename Context,
            typename MutableBuffer>
  stdexec::sender auto operator()(Predecessor&& prev,  //
                                  basic_socket<Protocol>&& peer_socket,
                                  const MutableBuffer& buffers) const {
    return stdexec::let_value((Predecessor &&) prev, [&peer_socket, &buffers] {
      return RecvSomeSender{(basic_socket<Protocol> &&) peer_socket, buffers};
    });
  }
};

inline constexpr net::AsyncRecvSome async_recv_some1{};

}  // namespace net

#endif  // NET_SOCKET_READ_OP1_H_
