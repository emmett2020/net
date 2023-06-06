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

#ifndef BASIC_DATAGRAM_SOCKET_HPP_
#define BASIC_DATAGRAM_SOCKET_HPP_

#include <cstddef>

#include "basic_socket.hpp"
#include "status-code/system_code.hpp"

namespace net {
// Provides datagram-oriented socket functionality.
template <typename Protocol>
class basic_datagram_socket : public basic_socket<Protocol> {
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

  // Construct a basic_datagram_socket without opening it.
  explicit constexpr basic_datagram_socket(context_type& ctx) noexcept
      : basic_socket<protocol_type>(ctx) {}

  // This constructor creates and opens a datagram socket.
  constexpr basic_datagram_socket(context_type& ctx, const Protocol& protocol,
                                  system_error2::system_code& ec) noexcept
      : basic_socket<Protocol>(ctx, protocol, ec) {}

  // Constructor with native socket and specific protocol.
  constexpr basic_datagram_socket(context_type& ctx,
                                  const protocol_type& protocol,
                                  native_handle_type fd) noexcept
      : basic_socket<Protocol>(ctx, protocol, fd) {}

  // Construct a basic_datagram_socket, opening it and binding it to the given
  // local endpoint.
  constexpr basic_datagram_socket(context_type& ctx,
                                  const endpoint_type& endpoint) noexcept
      : basic_socket<Protocol>(ctx, endpoint) {}

  // Move-construct a basic_datagram_socket from another.
  constexpr basic_datagram_socket(basic_datagram_socket&& other) noexcept =
      default;

  // Move-assign a basic_datagram_socket from another.
  constexpr basic_datagram_socket& operator=(
      basic_datagram_socket&& other) noexcept = default;

  // Destroys the socket.
  constexpr ~basic_datagram_socket() noexcept = default;

 private:
  basic_datagram_socket(const basic_datagram_socket&) = delete;
  basic_datagram_socket& operator=(const basic_datagram_socket&) = delete;
};
}  // namespace net

#endif  // BASIC_DATAGRAM_SOCKET_HPP_
