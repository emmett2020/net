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
#ifndef EPOLL_SOCKET_SEND_SOME_OP_HPP_
#define EPOLL_SOCKET_SEND_SOME_OP_HPP_

#include <cassert>
#include <concepts>      // NOLINT
#include <system_error>  // NOLINT

#include "status-code/system_code.hpp"

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
class epoll_context::socket_send_some_op {
  using receiver_t = stdexec::__t<ReceiverId>;
  using base_t =
      stdexec::__t<epoll_context::socket_io_base_op<ReceiverId, Protocol>>;
  using socket_t = typename Protocol::socket;
  using bufs_t = buffer_sequence_adapter<const_buffer, Buffers>;

 public:
  struct __t : public base_t {
    using __id = socket_send_some_op;

    // Constructor.
    // Note: CPO or constructor interfaces need to use `basic_socket<Protocol>`
    // instead of `Protocol::socket` as the parameter type to infer `Protocol`
    // template parameter. Subsequently, the storage and use of socket arguments
    // are converted to the actual subclass type, which is `Protocol::socket`.
    constexpr __t(receiver_t receiver, basic_socket<Protocol>& socket,
                  Buffers buffers) noexcept
        : base_t(static_cast<receiver_t&&>(receiver),  //
                 static_cast<socket_t&>(socket),       //
                 op_vtable, otype),
          bytes_transferred_(0),
          buffers_(buffers) {}

   private:
    static constexpr void non_blocking_send(base_t* base) noexcept {
      auto& self = *static_cast<__t*>(base);
      if constexpr (bufs_t::is_single_buffer) {
        auto res = self.socket_.non_blocking_send(
            bufs_t::first(self.buffers_).data(),  //
            bufs_t::first(self.buffers_).size(),  //
            0);
        if (res.has_error()) {
          self.ec_ = static_cast<system_error2::system_code&&>(res.error());
        } else {
          self.bytes_transferred_ += res.value();
        }
      } else {
        bufs_t bufs(self.buffers_);
        auto res =
            self.socket_.non_blocking_sendmsg(bufs.buffers(), bufs.count(), 0);
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
        stdexec::set_value(static_cast<receiver_t&&>(self.receiver_),
                           self.bytes_transferred_);
      } else {
        // !Compatible with the use of error_code in stdexec.
        // Should we changed status_code to error_code or just wait status_code
        // adopted by standard?
        if (self.ec_.domain() ==
            system_error2::quick_status_code_from_enum_domain<
                net::network_errc>) {
          stdexec::set_error(static_cast<receiver_t&&>(self.receiver_),
                             make_error_code(static_cast<net::network_errc>(
                                 self.ec_.value())));
        } else {
          stdexec::set_error(
              static_cast<receiver_t&&>(self.receiver_),
              make_error_code(static_cast<std::errc>(self.ec_.value())));
        }
      }
    }

    static constexpr typename base_t::op_type otype = base_t::op_type::op_write;
    static constexpr
        typename base_t::op_vtable op_vtable{&non_blocking_send, &complete};
    size_t bytes_transferred_;
    Buffers buffers_;
  };
};

template <typename Protocol, typename Buffers>
class send_some_sender {
  template <typename Receiver>
  using op_t =
      stdexec::__t<epoll_context::socket_send_some_op<stdexec::__id<Receiver>,
                                                      Protocol, Buffers>>;
  using socket_t = typename Protocol::socket;

 public:
  struct __t {
    using is_sender = void;
    using __id = send_some_sender;
    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(size_t),
                                       stdexec::set_error_t(std::error_code&&),
                                       stdexec::set_stopped_t()>;

    template <typename Env>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, __t&& self,
                           Env&&) noexcept -> completion_signatures;

    friend auto tag_invoke(stdexec::get_env_t, const __t& self) noexcept
        -> stdexec::empty_env {
      return {};
    }

    template <stdexec::__decays_to<__t> Sender,
              stdexec::receiver_of<completion_signatures> Receiver>
    friend auto tag_invoke(stdexec::connect_t, Sender&& self,
                           Receiver receiver) noexcept -> op_t<Receiver> {
      return {static_cast<Receiver&&>(receiver), self.socket_, self.buffers_};
    }

    constexpr __t(basic_socket<Protocol>& socket,  // NOLINT
                  Buffers buffers) noexcept
        : socket_(static_cast<socket_t&>(socket)), buffers_(buffers) {}

    explicit constexpr __t(const send_some_sender& other) noexcept
        : socket_(other.socket_), buffers_(other.buffers_) {}

    explicit constexpr __t(send_some_sender&& other) noexcept
        : socket_(other.socket_),
          buffers_(static_cast<Buffers&&>(other.buffers_)) {}

   private:
    socket_t& socket_;
    Buffers buffers_;
  };
};

struct async_send_some_t {
  template <transport_protocol Protocol, const_buffer_sequence Buffers>
  constexpr auto operator()(basic_socket<Protocol>& socket,
                            Buffers buffers) const noexcept
      -> stdexec::__t<send_some_sender<Protocol, Buffers>> {
    return {socket, buffers};
  }
};
}  // namespace __epoll

inline constexpr __epoll::async_send_some_t async_send_some{};
}  // namespace net

#endif  // EPOLL_SOCKET_SEND_SOME_OP_HPP_
