#ifndef NET_SRC_EPOLL_SOCKET_RECV_SOME_OPERATION_HPP_
#define NET_SRC_EPOLL_SOCKET_RECV_SOME_OPERATION_HPP_

#include <cassert>
#include <concepts>
#include <status-code/system_code.hpp>
#include <system_error>

#include "basic_socket.hpp"
#include "buffer.hpp"
#include "buffer_sequence_adapter.hpp"
#include "epoll/epoll_context.hpp"
#include "epoll/socket_io_base_op.hpp"
#include "meta.hpp"
#include "socket_option.hpp"
#include "stdexec.hpp"

namespace net {
namespace __epoll {

template <typename ReceiverId, typename Protocol, typename Buffers>
class epoll_context::socket_recv_some_op {
  using receiver_t = stdexec::__t<ReceiverId>;
  using base_t = stdexec::__t<epoll_context::socket_io_base_op<ReceiverId, Protocol>>;
  using socket_t = typename Protocol::socket;
  using bufs_t = buffer_sequence_adapter<mutable_buffer, Buffers>;

 public:
  struct __t : public base_t {
    using __id = socket_recv_some_op;

    // Constructor.
    // Note: CPO or constructor interfaces need to use `basic_socket<Protocol>` instead of
    // `Protocol::socket` as the parameter type to infer `Protocol` template parameter.
    // Subsequently, the storage and use of socket arguments are converted to the actual subclass
    // type, which is `Protocol::socket`.
    constexpr __t(receiver_t receiver, basic_socket<Protocol>& socket, Buffers buffers) noexcept
        : base_t(static_cast<receiver_t&&>(receiver),  //
                 static_cast<socket_t&>(socket),       //
                 op_vtable, otype),
          bytes_transferred_(0),
          buffers_(buffers) {}

   private:
    static constexpr void non_blocking_recv(base_t* base) noexcept {
      auto& self = *static_cast<__t*>(base);
      if constexpr (bufs_t::is_single_buffer) {
        auto res = self.socket_.non_blocking_recv(bufs_t::first(self.buffers_).data(),  //
                                                  bufs_t::first(self.buffers_).size(),  //
                                                  0);
        if (res.has_error()) {
          self.ec_ = static_cast<system_error2::system_code&&>(res.error());
        } else {
          self.bytes_transferred_ += res.value();
        }
      } else {
        bufs_t bufs(self.buffers_);
        auto res = self.socket_.non_blocking_recvmsg(bufs.buffers(), bufs.count(), 0);
        if (res.has_error()) {
          self.ec_ = static_cast<system_error2::system_code&&>(res.error());
        } else {
          self.bytes_transferred_ += res.value();
        }
      }
    }

    static void complete(base_t* base) noexcept {
      auto& self = *static_cast<__t*>(base);
      if (self.ec_ == errc::operation_canceled) {
        stdexec::set_stopped(static_cast<receiver_t&&>(self.receiver_));
      } else if (self.ec_ == errc::success) {
        stdexec::set_value(static_cast<receiver_t&&>(self.receiver_), self.bytes_transferred_);
      } else {
        // !Compatible with the use of error_code in stdexec.
        // Should we changed status_code to error_code or just wait status_code adopted by standard?
        if (self.ec_.domain()
            == system_error2::quick_status_code_from_enum_domain<net::network_errc>) {
          stdexec::set_error(static_cast<receiver_t&&>(self.receiver_),
                             make_error_code(static_cast<net::network_errc>(self.ec_.value())));
        } else {
          stdexec::set_error(static_cast<receiver_t&&>(self.receiver_),
                             make_error_code(static_cast<std::errc>(self.ec_.value())));
        }
      }
    }

    static constexpr typename base_t::op_type otype = base_t::op_type::op_read;
    static constexpr typename base_t::op_vtable op_vtable{&non_blocking_recv, &complete};
    size_t bytes_transferred_;
    Buffers buffers_;
  };
};

template <typename Protocol, typename Buffers>
class recv_some_sender {
  template <typename Receiver>
  using op_t =
      stdexec::__t<epoll_context::socket_recv_some_op<stdexec::__id<Receiver>, Protocol, Buffers>>;
  using socket_t = typename Protocol::socket;

 public:
  struct __t {
    using is_sender = void;
    using __id = recv_some_sender;
    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(size_t),
                                       stdexec::set_error_t(std::error_code&&),
                                       stdexec::set_stopped_t()>;

    template <typename Env>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, __t&& self, Env&&) noexcept
        -> completion_signatures;

    friend auto tag_invoke(stdexec::get_env_t, const __t& self) noexcept -> stdexec::empty_env {
      return {};
    }

    template <stdexec::__decays_to<__t> Sender,
              stdexec::receiver_of<completion_signatures> Receiver>
    friend auto tag_invoke(stdexec::connect_t, Sender&& self, Receiver receiver) noexcept
        -> op_t<Receiver> {
      return {static_cast<Receiver&&>(receiver), self.socket_, self.buffers_};
    }

    constexpr __t(basic_socket<Protocol>& socket, Buffers buffers) noexcept
        : socket_(static_cast<socket_t&>(socket)), buffers_(buffers) {}

    constexpr __t(const recv_some_sender& other) noexcept
        : socket_(other.socket_), buffers_(other.buffers_) {}

    constexpr __t(recv_some_sender&& other) noexcept
        : socket_(other.socket_), buffers_(static_cast<Buffers&&>(other.buffers_)) {}

   private:
    socket_t& socket_;
    Buffers buffers_;
  };
};

struct async_recv_some_t {
  template <transport_protocol Protocol, mutable_buffer_sequence Buffers>
  constexpr auto operator()(basic_socket<Protocol>& socket, Buffers buffers) const noexcept
      -> stdexec::__t<recv_some_sender<Protocol, Buffers>> {
    return {socket, buffers};
  }
};
}  // namespace __epoll

inline constexpr __epoll::async_recv_some_t async_recv_some{};
}  // namespace net

#endif  // NET_SRC_EPOLL_SOCKET_RECV_SOME_OPERATION_HPP_
