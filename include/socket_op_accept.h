#ifndef NET_SRC_SOCKET_OP_ACCEPT_HPP_
#define NET_SRC_SOCKET_OP_ACCEPT_HPP_

#include <exception>

#include "basic_socket.hpp"
#include "basic_socket_acceptor.hpp"
#include "io_epoll_context.hpp"
#include "socket_op_base.hpp"
#include "socket_option.hpp"
#include "stdexec/execution.hpp"

namespace net {
namespace _accept {
namespace ex = stdexec;

template <ex::receiver Receiver, typename Protocol>
class socket_accept_op : epoll_context::socket_base_operation<Receiver, Protocol> {
 public:
  using __acceptor = basic_socket_acceptor<Protocol>;
  using __socket = basic_socket<Protocol>;
  using __endpoint = typename Protocol::endpoint;
  using __base = epoll_context::socket_base_operation<Receiver, Protocol>;

  static bool non_blocking_accept(__base* base) {
    socket_accept_op* self(static_cast<socket_accept_op*>(base));

    auto res = self->acceptor_->non_blocking_accept();
    if (!res.has_value()) {
      self->ec_ = res.error();
      return false;
    }

    self->socket_.assign(res.value(), Protocol{});
    return true;
  }

  static void complete(__base* base) {
    auto& self = *static_cast<socket_accept_op*>(base);
    if (self.ec_ == NetError::kOperationAborted) {
      set_stopped((Receiver &&) self.receiver_);
    } else if (self.ec_ == NetError::kNoError) {
      set_value((Receiver &&) self.receiver_);
    } else {
      set_error((Receiver &&) self.receiver_, (std::error_code &&) self.ec_);
    }
  }

  static constexpr typename __base::op_type otype = __base::op_type::op_read;
  static constexpr typename __base::op_vtable op_vtable{&non_blocking_accept, &complete};

  // Constructor
  socket_accept_op(Receiver&& receiver, __acceptor& acceptor, __socket& socket)
      : __base((Receiver &&) receiver, (__acceptor &&) acceptor, op_vtable, otype),
        socket_(socket) {}

 private:
  Receiver receiver_;
  __acceptor& acceptor_;
  __socket& socket_;
};

template <typename Protocol>
class AcceptSender {
 public:
  using __acceptor = basic_socket_acceptor<Protocol>;
  using __socket = basic_socket<Protocol>;
  using __endpoint = typename Protocol::endpoint;

  using completion_signature =
      ex::completion_signatures<ex::set_value_t(int), ex::set_error_t(std::exception_ptr),
                                ex::set_stopped_t()>;

  // Constructor, 能否全部constexpr化
  AcceptSender(__acceptor& acceptor, __socket& socket) : acceptor_(acceptor), socket_(socket) {}

  // unifex::connect
  template <typename Sender, ex::receiver Receiver>
    requires ex::same_as<std::remove_cvref_t<Sender>, AcceptSender>
  friend auto tag_invoke(ex::connect_t, Sender&& self, Receiver&& receiver) noexcept {
    return socket_accept_op{(Receiver &&) receiver, self.acceptor_, self.socket_};
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, const AcceptSender& self) noexcept { return {}; }

  template <typename Env>
  friend auto tag_invoke(ex::get_completion_signatures_t, const AcceptSender& self, Env)
      -> completion_signature;

 private:
  __acceptor& acceptor_;
  __socket& socket_;
};

//
struct AsyncAccept {
  template <typename Protocol>
  constexpr auto operator()(basic_socket_acceptor<Protocol>& acceptor,
                            typename Protocol::socket& socket) const {
    return AcceptSender{acceptor, socket};
  }
};
}  // namespace _accept

inline constexpr net::_accept::AsyncAccept async_accept{};
}  // namespace net

#endif  // NET_SRC_SOCKET_OP_ACCEPT_HPP_
