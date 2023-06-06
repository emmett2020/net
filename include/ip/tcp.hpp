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
#ifndef IP_TCP_HPP_
#define IP_TCP_HPP_

#include "basic_socket_acceptor.hpp"
#include "basic_stream_socket.hpp"
#include "ip/basic_endpoint.hpp"
#include "socket_option.hpp"

namespace net {
namespace ip {
// Encapsulates the flags needed for tcp.
class tcp {
 public:
  // The type of a tcp endpoint.
  using endpoint = basic_endpoint<tcp>;

  // The tcp socket type.
  using socket = basic_stream_socket<tcp>;

  // The tcp acceptor type.
  using acceptor = basic_socket_acceptor<tcp>;

  // The tcp resolver type.
  // using resolver = basic_resolver<tcp>;

  // Construct with a specific family.
  constexpr explicit tcp(int protocol_family) noexcept
      : family_(protocol_family) {}

  // Construct to represent the IPv4 tcp protocol.
  constexpr static tcp v4() noexcept { return tcp(AF_INET); }

  // Construct to represent the IPv6 tcp protocol.
  constexpr static tcp v6() noexcept { return tcp(AF_INET6); }

  // Obtain an identifier for the type of the protocol.
  constexpr int type() const noexcept { return SOCK_STREAM; }

  // Obtain an identifier for the protocol.
  constexpr int protocol() const noexcept { return IPPROTO_TCP; }

  // Obtain an identifier for the protocol family.
  constexpr int family() const noexcept { return family_; }

  // Compare two protocols for equality.
  constexpr friend bool operator==(const tcp& p1, const tcp& p2) {
    return p1.family_ == p2.family_;
  }

  // Compare two protocols for inequality.
  constexpr friend bool operator!=(const tcp& p1, const tcp& p2) {
    return p1.family_ != p2.family_;
  }

 private:
  int family_;
};

}  // namespace ip
}  // namespace net

#endif  // IP_TCP_HPP_
