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
#ifndef TESTS_MOCK_PROTOCOL_HPP_
#define TESTS_MOCK_PROTOCOL_HPP_

#include "basic_socket.hpp"
#include "ip/basic_endpoint.hpp"

struct mock_protocol {
  using endpoint = net::ip::basic_endpoint<mock_protocol>;
  using socket = net::basic_socket<mock_protocol>;

  static constexpr mock_protocol v6() { return mock_protocol{AF_INET6}; }

  static constexpr mock_protocol v4() { return mock_protocol{AF_INET}; }

  static constexpr mock_protocol v6_udp() {
    return mock_protocol{AF_INET6, SOCK_DGRAM, IPPROTO_IP};
  }

  static constexpr mock_protocol v4_udp() {
    return mock_protocol{AF_INET, SOCK_DGRAM, IPPROTO_IP};
  }

  constexpr mock_protocol(int family) : family_(family) {}  // NOLINT

  constexpr mock_protocol(int family, int type, int protocol)
      : family_(family), type_(type), protocol_(protocol) {}

  constexpr mock_protocol() = delete;

  constexpr int family() const { return family_; }

  constexpr int type() const { return type_; }

  constexpr int protocol() const { return protocol_; }

  // Compare two endpoints for equality.
  constexpr bool operator==(const mock_protocol&) const noexcept = default;

  // Compare endpoints for ordering.
  constexpr auto operator<=>(const mock_protocol&) const noexcept = default;

  int family_;
  int type_ = SOCK_STREAM;
  int protocol_ = IPPROTO_IP;
};

#endif  // TESTS_MOCK_PROTOCOL_HPP_
