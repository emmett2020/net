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
#ifndef BASIC_SOCKET_ACCEPTOR_HPP_
#define BASIC_SOCKET_ACCEPTOR_HPP_

#include <utility>

#include "basic_socket.hpp"
#include "execution_context.hpp"
#include "ip/socket_types.hpp"
#include "socket_base.hpp"
#include "status-code/system_code.hpp"

namespace net {

// Provides the ability to accept new connections.
template <typename Protocol>
class basic_socket_acceptor : public basic_socket<Protocol> {
 public:
  // The native handle type.
  using native_handle_type = socket_base::native_handle_type;

  // The protocol type.
  using protocol_type = Protocol;

  // The endpoint type.
  using endpoint_type = typename protocol_type::endpoint;

  // The socket type.
  using socket_type = typename protocol_type::socket;

  // The context_type
  using context_type = typename basic_socket<protocol_type>::context_type;

  // Construct an acceptor without opening it.
  explicit constexpr basic_socket_acceptor(context_type& ctx) noexcept
      : basic_socket<protocol_type>(ctx) {}

  // Move copy
  constexpr basic_socket_acceptor(basic_socket_acceptor&& other) noexcept =
      default;

  // Move-assign.
  constexpr basic_socket_acceptor& operator=(
      basic_socket_acceptor&& other) noexcept = default;

  // Construct an acceptor opened on the given endpoint.
  constexpr basic_socket_acceptor(context_type& ctx,
                                  const endpoint_type& endpoint,
                                  system_error2::system_code& ec,
                                  bool reuse_addr = true) noexcept
      : basic_socket<Protocol>(ctx) {
    if (ec = basic_socket<Protocol>::open(endpoint.protocol()); ec.failure()) {
      return;
    }

    if (reuse_addr) {
      if (ec = basic_socket<Protocol>::set_option(
              socket_base::reuse_address{true});
          ec.failure())
        return;
    }

    if (ec = basic_socket<Protocol>::bind(endpoint); ec.failure()) {
      return;
    }

    if (ec = basic_socket<Protocol>::listen(max_listen_connections);
        ec.failure()) {
      return;
    }
  }

  // Destroys the acceptor.
  constexpr ~basic_socket_acceptor() noexcept = default;

 private:
  basic_socket_acceptor(const basic_socket_acceptor&) = delete;
  basic_socket_acceptor& operator=(const basic_socket_acceptor&) = delete;
};

}  // namespace net

#endif  // BASIC_SOCKET_ACCEPTOR_HPP_
