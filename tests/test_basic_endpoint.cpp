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
// #include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "catch2/catch_test_macros.hpp"

#include "ip/address_v4.hpp"
#include "ip/address_v6.hpp"
#include "ip/basic_endpoint.hpp"

using net::ip::address_v4;
using net::ip::address_v6;
using net::ip::basic_endpoint;
using net::ip::make_address_v4;
using net::ip::make_address_v6;

struct mock_protocol {
  static constexpr mock_protocol v6() { return mock_protocol{AF_INET6}; }

  static constexpr mock_protocol v4() { return mock_protocol{AF_INET}; }

  explicit constexpr mock_protocol(int family) : family_(family) {}

  constexpr int family() const { return family_; }

  // Compare two endpoints for equality.
  constexpr bool operator==(const mock_protocol&) const noexcept = default;

  // Compare endpoints for ordering.
  constexpr auto operator<=>(const mock_protocol&) const noexcept = default;

  int family_;
};

TEST_CASE(
    "[default ctor should return IPv6 any address with the port whose value "
    "equals 0]",
    "[basic_endpoint.ctor]") {
  basic_endpoint<mock_protocol> endpoint;
  CHECK(endpoint.address().is_v6());
  CHECK(endpoint.address().is_unspecified());
  CHECK(endpoint.port() == 0);
}

TEST_CASE(
    "[constructor with internet protocol should return any address in default]",
    "[basic_endpoint.ctor]") {
  basic_endpoint<mock_protocol> endpoint{mock_protocol::v6(), 80};
  CHECK(endpoint.address().is_v6());
  CHECK(endpoint.address().is_unspecified());
  CHECK(endpoint.port() == 80);
}

TEST_CASE("[constructor with specific address will also set protocol]",
          "[basic_endpoint.ctor]") {
  address_v6 addr6 = make_address_v6("::ffff:1.1.1.1");
  basic_endpoint<mock_protocol> endpoint6{addr6, 80};
  CHECK(endpoint6.address().is_v6());
  CHECK(endpoint6.address().to_v6().is_v4_mapped());
  CHECK(endpoint6.port() == 80);

  address_v4 addr4 = make_address_v4("127.0.0.1");
  basic_endpoint<mock_protocol> endpoint4{addr4, 80};
  CHECK(endpoint4.address().is_v4());
  CHECK(endpoint4.address().to_v4().is_loopback());
  CHECK(endpoint4.port() == 80);
}

TEST_CASE("[copy constructor]", "[basic_endpoint.ctor]") {
  basic_endpoint<mock_protocol> ep{mock_protocol::v4(), 80};
  basic_endpoint<mock_protocol> ep_copy{ep};
  CHECK(ep == ep_copy);
  CHECK(ep_copy.port() == 80);
}

TEST_CASE("[move constructor]", "[basic_endpoint.ctor]") {
  basic_endpoint<mock_protocol> ep{mock_protocol::v4(), 80};
  basic_endpoint<mock_protocol> ep_move{std::move(ep)};
  CHECK(ep_move.port() == 80);
}

TEST_CASE("[set IPv4 native address]", "[basic_endpoint.ctor]") {
  ::sockaddr_in addr4{};
  addr4.sin_port = htons(80);
  addr4.sin_addr.s_addr = INADDR_ANY;
  addr4.sin_family = AF_INET;
  basic_endpoint<mock_protocol> endpoint{addr4};
  CHECK(endpoint.protocol().family() == AF_INET);
  CHECK(endpoint.port() == 80);
  CHECK(endpoint.address().to_v4().is_unspecified());

  addr4.sin_addr.s_addr = htonl(0x7F000001);
  basic_endpoint<mock_protocol> endpoint_loopback{addr4};
  CHECK(endpoint_loopback.protocol().family() == AF_INET);
  CHECK(endpoint_loopback.port() == 80);
  CHECK(endpoint_loopback.address().to_v4().is_loopback());
  CHECK(endpoint_loopback.address().to_v4().to_string() == "127.0.0.1");
}

TEST_CASE("[set IPv6 native address]", "[basic_endpoint.ctor]") {
  ::sockaddr_in6 addr6{};
  addr6.sin6_port = htons(80);
  addr6.sin6_scope_id = 5;
  memcpy(&addr6.sin6_addr.s6_addr, &in6addr_any,
         sizeof(addr6.sin6_addr.s6_addr));
  addr6.sin6_family = AF_INET6;
  basic_endpoint<mock_protocol> endpoint{addr6};
  CHECK(endpoint.protocol().family() == AF_INET6);
  CHECK(endpoint.port() == 80);
  CHECK(endpoint.address().to_v6().is_unspecified());
  CHECK(endpoint.address().to_v6().scope_id() == 5);

  addr6.sin6_addr.s6_addr[0] = 0xfe;
  addr6.sin6_addr.s6_addr[1] = 0x80;
  addr6.sin6_addr.s6_addr[12] = 0x11;
  addr6.sin6_addr.s6_addr[13] = 0x12;
  addr6.sin6_addr.s6_addr[14] = 0x13;
  addr6.sin6_addr.s6_addr[15] = 0x14;
  basic_endpoint<mock_protocol> endpoint_link_local{addr6};
  CHECK(endpoint_link_local.protocol().family() == AF_INET6);
  CHECK(endpoint_link_local.port() == 80);
  CHECK(endpoint_link_local.address().to_v6().is_link_local());
  CHECK(endpoint_link_local.address().to_v6().scope_id() == 5);
}

TEST_CASE("[native_address should return correct result]",
          "[basic_endpoint.native_address]") {
  // test IPv6 version
  basic_endpoint<mock_protocol> endpoint6{make_address_v6("::ffff:1.1.1.1"),
                                          80};
  ::sockaddr_storage storage6;
  CHECK(endpoint6.native_address(&storage6) == sizeof(::sockaddr_in6));
  ::sockaddr_in6 native_addr6 = *reinterpret_cast<::sockaddr_in6*>(&storage6);
  CHECK(native_addr6.sin6_family == AF_INET6);
  CHECK(native_addr6.sin6_scope_id == 0);
  CHECK(native_addr6.sin6_port == htons(80));
  CHECK(native_addr6.sin6_addr.s6_addr[0] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[1] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[2] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[3] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[4] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[5] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[6] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[7] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[8] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[9] == 0);
  CHECK(native_addr6.sin6_addr.s6_addr[10] == 255);
  CHECK(native_addr6.sin6_addr.s6_addr[11] == 255);
  CHECK(native_addr6.sin6_addr.s6_addr[12] == 1);
  CHECK(native_addr6.sin6_addr.s6_addr[13] == 1);
  CHECK(native_addr6.sin6_addr.s6_addr[14] == 1);
  CHECK(native_addr6.sin6_addr.s6_addr[15] == 1);

  // test IPv4 version
  basic_endpoint<mock_protocol> endpoint4{make_address_v4("127.0.0.1"), 80};
  ::sockaddr_storage storage4;
  CHECK(endpoint4.native_address(&storage4) == sizeof(::sockaddr_in));
  ::sockaddr_in native_addr4 = *reinterpret_cast<::sockaddr_in*>(&storage4);
  CHECK(native_addr4.sin_family == AF_INET);
  CHECK(native_addr4.sin_port == htons(80));
  CHECK(native_addr4.sin_addr.s_addr == htonl(0x7F000001));

  // test default ctor
  basic_endpoint<mock_protocol> endpoint_default;
  ::sockaddr_storage storage_default;
  CHECK(endpoint_default.native_address(&storage_default) ==
        sizeof(::sockaddr_in6));
  ::sockaddr_in6 native_addr_default =
      *reinterpret_cast<::sockaddr_in6*>(&storage_default);
  CHECK(native_addr_default.sin6_family == AF_INET6);
  CHECK(native_addr_default.sin6_scope_id == 0);
  CHECK(native_addr_default.sin6_port == htons(0));
  CHECK(native_addr_default.sin6_addr.s6_addr[0] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[1] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[2] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[3] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[4] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[5] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[6] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[7] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[8] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[9] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[10] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[11] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[12] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[13] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[14] == 0);
  CHECK(native_addr_default.sin6_addr.s6_addr[15] == 0);
}

TEST_CASE("operation <=> should work", "basic_endpoint.comparision") {
  basic_endpoint<mock_protocol> ep0{make_address_v6("::ffff:1.1.1.1"), 80};
  basic_endpoint<mock_protocol> ep1{make_address_v6("::ffff:1.1.1.1"), 80};
  basic_endpoint<mock_protocol> ep2{make_address_v6("::ffff:2.2.2.2"), 79};
  basic_endpoint<mock_protocol> ep3{make_address_v6("::ffff:2.2.2.2"), 78};

  CHECK(ep0 == ep1);
  CHECK(ep1 < ep2);
  CHECK(ep2 > ep3);
}

TEST_CASE("hash basic_endpoint", "basic_endpoint.hash") {
  basic_endpoint<mock_protocol> ep{make_address_v6("::ffff:1.1.1.1"), 80};
  std::unordered_map<basic_endpoint<mock_protocol>, bool> table;
  table[ep] = false;
  CHECK(table[ep] == false);
  table[ep] = true;
  CHECK(table[ep] == true);
}
