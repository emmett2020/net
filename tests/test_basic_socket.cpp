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
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <exception>
#include <mutex>  // NOLINT
#include <optional>
#include <thread>  // NOLINT

#include "catch2/catch_test_macros.hpp"
#include "epoll/epoll_context.hpp"
#include "ip/address_v4.hpp"
#include "ip/address_v6.hpp"
#include "socket_base.hpp"
#include "status-code/error.hpp"
#include "status-code/errored_status_code.hpp"
#include "status-code/system_code.hpp"
#include "status-code/system_error2.hpp"

#include "basic_socket.hpp"
#include "execution_context.hpp"
#include "ip/basic_endpoint.hpp"
#include "mock_protocol.hpp"

using net::basic_socket;
using net::epoll_context;
using net::execution_context;
using net::socket_base;
using net::ip::address_v4;
using net::ip::address_v6;
using net::ip::basic_endpoint;
using net::ip::make_address_v6;
using system_error2::errc;
using system_error2::system_code;

// Used to simplify the writing of constructors.
static execution_context mock_context{};

class mock_socket : public basic_socket<mock_protocol> {
  constexpr mock_socket() : basic_socket<mock_protocol>(mock_context) {}

  constexpr mock_socket(const mock_protocol& protocol, system_code& code)
      : basic_socket<mock_protocol>(mock_context, protocol, code) {}

  constexpr mock_socket(const mock_protocol& protocol, native_handle_type fd)
      : basic_socket<mock_protocol>(mock_context, protocol, fd) {}
};

// If the unit test fails because the port is occupied, you can change the value
// of the port. The first unit test indicates whether the port is occupied.
static constexpr int mock_port = 12821;

class mock_acceptor : public basic_socket<mock_protocol> {
 public:
  mock_acceptor() : basic_socket<mock_protocol>(mock_context) {
    // set reuse port
    if (open(mock_protocol::v4()).failure()) {
      return;
    }
    if (set_option(reuse_address{true}).success()) {
      close();
      return;
    }
    if (bind(mock_protocol::endpoint{address_v4::any(), mock_port}).failure()) {
      close();
      return;
    }
    if (listen().failure()) {
      close();
      return;
    }
  }
};

// !We mainly test ipv4/ipv6/tcp/udp.

TEST_CASE("Check the mock_port is not occupied", "[basic_socket.mock_port]") {
  int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  CHECK(socket_fd != -1);

  ::sockaddr_in sin{.sin_family = AF_INET, .sin_port = htons(mock_port)};
  ::inet_pton(AF_INET, "0.0.0.0", &sin.sin_addr);

  CHECK(::bind(socket_fd, (::sockaddr*)&sin, sizeof(::sockaddr)) == 0);
  CHECK(errno != EADDRINUSE);
  close(socket_fd);
}

TEST_CASE(
    "[The default constructor has four members, and the initial information "
    "should be correct]",
    "[basic_socket.ctor]") {
  mock_socket socket{};
  CHECK(socket.descriptor_ == -1);
  CHECK(socket.state_ == 0);
  CHECK(socket.protocol_ == std::nullopt);
  CHECK(&socket.context_ == &mock_context);
}

TEST_CASE("[context() should return associated context]",
          "[basic_socket.context]") {
  mock_socket socket{};
  CHECK(&socket.context() == &mock_context);
  CHECK(&socket.context_ == &mock_context);
  CHECK(&socket.context_ == &socket.context());
}

TEST_CASE("[native_handle() should return correct file descriptor]",
          "[basic_socket.native_handle]") {
  mock_socket socket{};

  // positive
  socket.descriptor_.reset(12);
  CHECK(socket.native_handle() == 12);

  // negative
  socket.descriptor_.reset(-12);
  CHECK(socket.native_handle() == -12);
}

TEST_CASE("[protocol() should return associated protocol]",
          "[basic_socket.protocol]") {
  mock_socket socket{};
  // IPv4/TCP
  socket.protocol_ = mock_protocol::v4();
  CHECK(socket.protocol().family() == AF_INET);
  CHECK(socket.protocol().type() == SOCK_STREAM);
  CHECK(socket.protocol().protocol() == IPPROTO_IP);

  // IPv6/TCP
  socket.protocol_ = mock_protocol::v6();
  CHECK(socket.protocol().family() == AF_INET6);
  CHECK(socket.protocol().type() == SOCK_STREAM);
  CHECK(socket.protocol().protocol() == IPPROTO_IP);

  // IPv4/UDP
  socket.protocol_ = mock_protocol{AF_INET, SOCK_DGRAM, IPPROTO_IP};
  CHECK(socket.protocol().family() == AF_INET);
  CHECK(socket.protocol().type() == SOCK_DGRAM);
  CHECK(socket.protocol().protocol() == IPPROTO_IP);

  // IPv6/UDP
  socket.protocol_ = mock_protocol{AF_INET6, SOCK_DGRAM, IPPROTO_IP};
  CHECK(socket.protocol().family() == AF_INET6);
  CHECK(socket.protocol().type() == SOCK_DGRAM);
  CHECK(socket.protocol().protocol() == IPPROTO_IP);
}

TEST_CASE("[state() should return associated state]", "[basic_socket.state]") {
  mock_socket socket{};
  socket.set_state(1);
  CHECK(socket.state() == 1);
}

TEST_CASE("[move constructor should work]", "[basic_socket.ctor]") {
  mock_socket socket{};
  socket.descriptor_.reset(12);
  socket.state_ = 10;

  mock_socket socket_mv{std::move(socket)};
  CHECK(socket_mv.descriptor_ == 12);
  CHECK(socket_mv.state_ == 10);

  // The old socket's descriptor_ should be set to -1.
  CHECK(socket.descriptor_ == -1);
}

TEST_CASE("[Construct the socket using the correct protocol]",
          "[basic_socket.ctor]") {
  // We initialize system_code as failure.
  // When open succeeds, this code should be assigned success.
  system_code code{errc::protocol_error};

  // IPv4/TCP
  mock_socket socket{mock_protocol::v4(), code};
  CHECK(code.success());
  CHECK(socket.is_open());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.close().success());

  // IPv6/TCP
  mock_socket socket_v6{mock_protocol::v6(), code};
  CHECK(code.success());
  CHECK(socket_v6.is_open());
  CHECK(socket_v6.state() == stream_oriented);
  CHECK(socket_v6.protocol() == mock_protocol::v6());
  CHECK(socket_v6.close().success());

  // IPv4/UDP
  mock_socket socket_udp{mock_protocol::v4_udp(), code};
  CHECK(code.success());
  CHECK(socket_udp.is_open());
  CHECK(socket_udp.state() == datagram_oriented);
  CHECK(socket_udp.protocol() == mock_protocol::v4_udp());
  CHECK(socket_udp.close().success());

  // IPv6/UDP
  mock_socket socket_v6_udp{mock_protocol::v6_udp(), code};
  CHECK(code.success());
  CHECK(socket_v6_udp.is_open());
  CHECK(socket_v6_udp.state() == datagram_oriented);
  CHECK(socket_v6_udp.protocol() == mock_protocol::v6_udp());
  CHECK(socket_v6_udp.close().success());
}

TEST_CASE("[Construct the socket using the correct protocol and native socket]",
          "[basic_socket.ctor]") {
  // IPv4/TCP
  mock_socket socket{mock_protocol::v4(), 12};
  CHECK(socket.native_handle() == 12);
  CHECK(socket.is_open());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.close().success());

  // IPv6/TCP
  mock_socket socket_v6{mock_protocol::v6(), 12};
  CHECK(socket_v6.native_handle() == 12);
  CHECK(socket_v6.is_open());
  CHECK(socket_v6.state() == stream_oriented);
  CHECK(socket_v6.protocol() == mock_protocol::v6());
  CHECK(socket_v6.close().success());

  // IPv4/UDP
  mock_socket socket_udp{mock_protocol::v4_udp(), 12};
  CHECK(socket_udp.native_handle() == 12);
  CHECK(socket_udp.is_open());
  CHECK(socket_udp.state() == datagram_oriented);
  CHECK(socket_udp.protocol() == mock_protocol::v4_udp());
  CHECK(socket_udp.close().success());

  // IPv6/UDP
  mock_socket socket_v6_udp{mock_protocol::v6_udp(), 12};
  CHECK(socket_v6_udp.native_handle() == 12);
  CHECK(socket_v6_udp.is_open());
  CHECK(socket_v6_udp.state() == datagram_oriented);
  CHECK(socket_v6_udp.protocol() == mock_protocol::v6_udp());
  CHECK(socket_v6_udp.close().success());
}

TEST_CASE("[Open() already opened descriptor should return an error]",
          "[basic_socket.open]") {
  mock_socket socket{};
  socket.descriptor_.reset(12);
  CHECK(socket.open(mock_protocol::v4()) == errc::bad_file_descriptor);
}

TEST_CASE(
    "[Calling open() with an invalid protocol argument should return an error]",
    "[basic_socket.open]") {
  mock_socket socket{};
  auto protocol = mock_protocol::v4();

  // there we assigned an unsupported family to protocol
  protocol.family_ = 1024;
  CHECK(socket.open(protocol) == errc::address_family_not_supported);
}

TEST_CASE(
    "[Both state_ and protocol_ should be set correctly when open() executes "
    "successfully]",
    "[basic_socket.open]") {
  mock_socket socket{};

  // IPv4/TCP
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.is_open());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.close().success());

  // IPv6/TCP
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.is_open());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v6());
  CHECK(socket.close().success());

  // IPv4/UDP
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.is_open());
  CHECK(socket.state() == datagram_oriented);
  CHECK(socket.protocol() == mock_protocol::v4_udp());
  CHECK(socket.close().success());

  // IPv6/UDP
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.is_open());
  CHECK(socket.state() == datagram_oriented);
  CHECK(socket.protocol() == mock_protocol::v6_udp());
  CHECK(socket.close().success());
}

TEST_CASE("[Open a closed file descriptor using the same protocol type]",
          "[basic_socket.open]") {
  mock_socket socket{};

  // IPv4/TCP
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.close().success());
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.close().success());

  // IPv6/TCP
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.close().success());
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.close().success());

  // IPv4/UDP
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.close().success());
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.close().success());

  // IPv6/UDP
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.close().success());
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.close().success());
}

TEST_CASE("[Open a closed file descriptor using different protocol type]",
          "[basic_socket.open]") {
  mock_socket socket{};

  // IPv4/TCP -> IPv6/TCP
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.is_open());
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.close().success());
  CHECK(socket.is_open() == false);

  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.is_open());
  CHECK(socket.protocol() == mock_protocol::v6());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.close().success());
  CHECK(socket.is_open() == false);

  // IPv4/TCP -> IPv4/UDP
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.is_open());
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.close().success());
  CHECK(socket.is_open() == false);

  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.is_open());
  CHECK(socket.protocol() == mock_protocol::v4_udp());
  CHECK(socket.state() == datagram_oriented);
  CHECK(socket.close().success());
  CHECK(socket.is_open() == false);
}

TEST_CASE(
    "[Setting non-blocking mode for an fd that is not opened should returns an "
    "error]",
    "[basic_socket.nonblocking]") {
  mock_socket socket{};

  // error descriptor
  auto res = socket.set_non_blocking(true);
  CHECK(res.failure());
  CHECK(res == errc::bad_file_descriptor);
}

TEST_CASE("[For different protocols, verify that set_non_blocking() works]",
          "[basic_socket.nonblocking]") {
  mock_socket socket{};

  // IPv4/TCP
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.set_non_blocking(true).success());
  CHECK(socket.is_non_blocking());
  CHECK(socket.set_non_blocking(false).success());
  CHECK(socket.is_non_blocking() == false);
  CHECK(socket.close().success());

  // IPv6/TCP
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.set_non_blocking(true).success());
  CHECK(socket.is_non_blocking());
  CHECK(socket.set_non_blocking(false).success());
  CHECK(socket.is_non_blocking() == false);
  CHECK(socket.close().success());

  // IPv4/UDP
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.set_non_blocking(true).success());
  CHECK(socket.is_non_blocking());
  CHECK(socket.set_non_blocking(false).success());
  CHECK(socket.is_non_blocking() == false);
  CHECK(socket.close().success());

  // IPv6/UDP
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.set_non_blocking(true).success());
  CHECK(socket.is_non_blocking());
  CHECK(socket.set_non_blocking(false).success());
  CHECK(socket.is_non_blocking() == false);
  CHECK(socket.close().success());
}

TEST_CASE(
    "[Both state_ and protocol_ should be set correctly when assign() executes "
    "successfully]",
    "[basic_socket.assign]") {
  mock_socket socket{};

  // IPv4/TCP
  socket.assign(mock_protocol::v4(), 12);
  CHECK(socket.is_open());
  CHECK(socket.descriptor_ == 12);
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v4());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);

  // IPv6/TCP
  socket.assign(mock_protocol::v6(), 12);
  CHECK(socket.is_open());
  CHECK(socket.descriptor_ == 12);
  CHECK(socket.state() == stream_oriented);
  CHECK(socket.protocol() == mock_protocol::v6());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);

  // IPv4/UDP
  socket.assign(mock_protocol::v4_udp(), 12);
  CHECK(socket.is_open());
  CHECK(socket.descriptor_ == 12);
  CHECK(socket.state() == datagram_oriented);
  CHECK(socket.protocol() == mock_protocol::v4_udp());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);

  // IPv6/UDP
  socket.assign(mock_protocol::v6_udp(), 12);
  CHECK(socket.is_open());
  CHECK(socket.descriptor_ == 12);
  CHECK(socket.state() == datagram_oriented);
  CHECK(socket.protocol() == mock_protocol::v6_udp());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);
}

TEST_CASE("[close an already opened socket should work]",
          "[basic_socket.close]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.is_open());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);
}

TEST_CASE("[close an already closed socket should return an error]",
          "[basic_socket.close]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.is_open());
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);
  CHECK(socket.close().failure());
  CHECK(socket.close().failure());
  CHECK(socket.close().failure());
}

TEST_CASE(
    "[state_ and protocol_ are not reset even if the close() function executes "
    "successfully]",
    "[basic_socket.close]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  socket.set_state(12);
  CHECK(socket.state() == 12);
  CHECK(socket.close().success());
  CHECK(socket.descriptor_ == -1);
  CHECK(socket.state() == 12);
  CHECK(socket.protocol() == mock_protocol::v4());
}

TEST_CASE("[shutdown an unopened file descriptor should return an error]",
          "[basic_socket.shutdown]") {
  mock_socket socket{};
  CHECK(socket.shutdown(socket_base::shutdown_type::shutdown_send) ==
        errc::bad_file_descriptor);
}

TEST_CASE(
    "[shutdown an opened but unconnected file descriptor should return an "
    "error]",
    "[basic_socket.shutdown]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.shutdown(socket_base::shutdown_type::shutdown_send) ==
        errc::not_connected);
}

// TEST_CASE("[shutdown() resets the file descriptor when executed
// successfully]",
//           "[basic_socket.shutdown]") {
//   mock_socket socket{};
//   CHECK(socket.open(mock_protocol::v4()).success());
//   CHECK(socket.shutdown(socket_base::shutdown_type::shutdown_send) ==
//   errc::not_connected);
// }

TEST_CASE("[bind() using unopened descriptor should return an error]",
          "[basic_socket.bind]") {
  mock_socket socket{};
  CHECK(socket.bind(mock_protocol::endpoint{}) == errc::bad_file_descriptor);
}

TEST_CASE(
    "[An error should be returned if the protocol type of the file descriptor "
    "is inconsistent with "
    "the endpoint type provided by bind()]",
    "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{make_address_v6("::ffff:1.1.1.1"),
                                   mock_port};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(endpoint) == errc::address_family_not_supported);
}

TEST_CASE("[Binding to an IPv4/TCP endpoint should work]",
          "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{address_v4::any(), mock_port};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(endpoint).success());
  CHECK(socket.close().success());
}

TEST_CASE("[Binding to an IPv6/TCP endpoint should work]",
          "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{address_v6::any(), mock_port};
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.bind(endpoint).success());
  CHECK(socket.close().success());
}

TEST_CASE("[Binding to an IPv4/UDP endpoint should work]",
          "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{address_v4::any(), mock_port};
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.bind(endpoint).success());
  CHECK(socket.close().success());
}

TEST_CASE("[Binding to an IPv6/UDP endpoint should work]",
          "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{address_v6::any(), mock_port};
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.bind(endpoint).success());
  CHECK(socket.close().success());
}

TEST_CASE(
    "[open() a socket of IPv4 but bind() this socket to endpoint of IPv6 "
    "should return error]",
    "[basic_socket.bind]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint{address_v6::any(), mock_port};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(endpoint).failure());
}

TEST_CASE("[listen() with invalid descriptor should return an error]",
          "[basic_socket.listen]") {
  mock_socket socket{};
  CHECK(socket.listen() == errc::bad_file_descriptor);
  socket.descriptor_.reset(12);
  CHECK(socket.listen() == errc::bad_file_descriptor);
}

TEST_CASE("[listen() to the IPv4/TCP endpoint should work]",
          "[basic_socket.listen]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(mock_protocol::endpoint{address_v4::any(), mock_port})
            .success());
  CHECK(socket.listen().success());
  CHECK(socket.close().success());
}

TEST_CASE(
    "[listen() to the IPv4/UDP endpoint should return "
    "errc::operation_not_supported]",
    "[basic_socket.listen]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4_udp()).success());
  CHECK(socket.bind(mock_protocol::endpoint{address_v4::any(), mock_port})
            .success());
  CHECK(socket.listen().failure());
  CHECK(socket.listen() == errc::operation_not_supported);
  CHECK(socket.close().success());
}

TEST_CASE("[listen() to the IPv6/TCP endpoint should work]",
          "[basic_socket.listen]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.bind(mock_protocol::endpoint{address_v6::any(), mock_port})
            .success());
  CHECK(socket.listen().success());
  CHECK(socket.close().success());
}

TEST_CASE("[listen() to the IPv6/UDP endpoint should work]",
          "[basic_socket.listen]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v6_udp()).success());
  CHECK(socket.bind(mock_protocol::endpoint{address_v6::any(), mock_port})
            .success());
  CHECK(socket.listen().failure());
  CHECK(socket.listen() == errc::operation_not_supported);
  CHECK(socket.close().success());
}

TEST_CASE("[poll_xxx with invalid fd should return an error]",
          "[basic_socket.poll_xxx]") {
  mock_socket socket{};
  CHECK(socket.poll_read(-1) == errc::bad_file_descriptor);
  CHECK(socket.poll_write(-1) == errc::bad_file_descriptor);
  CHECK(socket.poll_connect(-1) == errc::bad_file_descriptor);
  CHECK(socket.poll_error(-1) == errc::bad_file_descriptor);
}

TEST_CASE("[poll_xxx with 1000ms should executed successful]",
          "[basic_socket.poll_xxx]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.poll_read(1000).success());
  CHECK(socket.poll_write(1000).success());
  CHECK(socket.poll_connect(1000).success());
  CHECK(socket.poll_error(1000).success());
}

TEST_CASE("[getsockopt() using invalid descriptor should return an error]",
          "[basic_socket.option]") {
  mock_socket socket{};
  int32_t opt = 0;
  ::socklen_t opt_len = 4;
  auto res = socket.getsockopt(1, 1, &opt, &opt_len);
  CHECK(res == errc::bad_file_descriptor);
}

TEST_CASE("[use getsockopt() to get connection aborted option]",
          "[basic_socket.option]") {
  mock_socket socket{};
  socket.descriptor_.reset(12);

  // unset.
  int optval = 0;
  ::socklen_t opt_len = sizeof(int);
  auto res =
      socket.getsockopt(custom_socket_option_level,
                        enable_connection_aborted_option, &optval, &opt_len);
  CHECK(res.success());
  CHECK(socket.state() == 0);
  CHECK(optval == 0);
  CHECK(opt_len == 4);

  // set.
  socket.set_state(enable_connection_aborted_state);
  res = socket.getsockopt(custom_socket_option_level,
                          enable_connection_aborted_option, &optval, &opt_len);
  CHECK(res.success());
  CHECK(optval == 1);
}

TEST_CASE("[setsockopt() use invalid descriptor should return an error]",
          "[basic_socket.option]") {
  mock_socket socket{};
  int32_t opt = 1;
  auto res = socket.setsockopt(1, 1, &opt, sizeof(opt));
  CHECK(res == errc::bad_file_descriptor);
}

TEST_CASE("[set enable connection aborted option]", "[basic_socket.option]") {
  mock_socket socket{};
  socket.descriptor_.reset(12);
  // set
  int32_t optval = 1;
  auto res = socket.setsockopt(custom_socket_option_level,
                               enable_connection_aborted_option, &optval,
                               sizeof(optval));
  CHECK(res.success());
  CHECK(socket.state() & enable_connection_aborted_state);

  // unset
  optval = 0;
  res = socket.setsockopt(custom_socket_option_level,
                          enable_connection_aborted_option, &optval,
                          sizeof(optval));
  CHECK((socket.state() & enable_connection_aborted_state) == 0);
}

TEST_CASE("[set reuse_addr option should work]", "[basic_socket.option]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.set_option(mock_socket::reuse_address{true}).success());
  CHECK(socket.set_option(mock_socket::reuse_address{false}).success());
  CHECK(socket.close().success());
}

TEST_CASE("[local_endpoint() will return correct local endpoint informations]",
          "[basic_socket.local_endpoint]") {
  mock_socket socket{};
  mock_protocol::endpoint local_ep{address_v4::any(), mock_port};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(local_ep).success());
  CHECK(socket.local_endpoint().has_value());
  CHECK(socket.local_endpoint().value() == local_ep);

  // v6
  mock_protocol::endpoint local_ep_v6{address_v6::any(), mock_port};
  CHECK(socket.close().success());
  CHECK(socket.open(mock_protocol::v6()).success());
  CHECK(socket.bind(local_ep_v6).success());
  CHECK(socket.local_endpoint().has_value());
  CHECK(socket.local_endpoint().value() == local_ep_v6);
}

TEST_CASE(
    "[peer_endpoint() should return errc::not_connected when no client "
    "connected in]",
    "[basic_socket.getpeername]") {
  mock_socket socket{};
  mock_protocol::endpoint ep{address_v4::any(), mock_port};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(ep).success());
  CHECK(socket.getpeername().has_error());
  CHECK(socket.getpeername().error() == errc::not_connected);
}

// TEST_CASE("[peer_endpoint() will return correct peer endpoint informations]",
//           "[basic_socket.getpeername]") {
//   std::thread server_thread([]() {
//     mock_acceptor acceptor{};
//     CHECK(acceptor.is_open());
//     auto res = acceptor.accept();
//     CHECK(res.has_value());
//     CHECK(res.value().native_handle() != invalid_socket_fd);

//     CHECK(res.value().peer_endpoint().has_value());
//     CHECK(res.value().peer_endpoint().value().address().is_v4());
//     CHECK(acceptor.close().success());
//   });

//   std::this_thread::sleep_for(100ms);
//   mock_socket client_socket;
//   client_socket.open(mock_protocol::v4());
//   auto res =
//   client_socket.connect(mock_protocol::endpoint{address_v4::loopback(),
//   mock_port}); CHECK(res.success()); std::this_thread::sleep_for(100ms);
//   CHECK(client_socket.close().success());
//   server_thread.join();
// }

TEST_CASE("[use ioctl() to set FIONBIO ok]", "[basic_socket.ioctl]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  // TODO: add IoControlCommand
}

TEST_CASE("gethostname should work", "[basic_socket.gethostname]") {
  mock_socket socket{};
  char name[1024];
  CHECK(socket.gethostname(name, 1024).success());
}

TEST_CASE("[accept() with invalid local descriptor should return an error]",
          "[basic_socket.accept]") {
  mock_socket socket{};
  CHECK(socket.accept().error() == errc::bad_file_descriptor);
}

TEST_CASE("[connect() with invalid local descriptor should return an error]",
          "[basic_socket.connect]") {
  mock_socket socket{};
  mock_protocol::endpoint endpoint;
  CHECK(socket.connect(endpoint) == errc::bad_file_descriptor);
}

// TEST_CASE("accept() should work when clients connect().",
// "[basic_socket.accept]") {
//   std::thread server_thread([]() {
//     mock_acceptor acceptor{};
//     CHECK(acceptor.is_open());
//     auto res = acceptor.accept();
//     CHECK(res.has_value());
//     CHECK(res.value().native_handle() != invalid_socket_fd);
//     CHECK(acceptor.close().success());
//   });

//   std::this_thread::sleep_for(100ms);
//   mock_socket client_socket;
//   client_socket.open(mock_protocol::v4());
//   auto res =
//   client_socket.connect(mock_protocol::endpoint{address_v4::loopback(),
//   mock_port}); CHECK(res.success()); CHECK(client_socket.close().success());
//   server_thread.join();
// }

TEST_CASE("[sync_accept() could execute successfully]",
          "[basic_socket.accept]") {
  mock_socket socket{};
}

TEST_CASE("[non_blocking_accept() with non blocking descriptor won't block]",
          "[basic_socket.accept]") {
  mock_socket socket{};
  CHECK(socket.open(mock_protocol::v4()).success());
  CHECK(socket.bind(mock_protocol::endpoint{address_v4::any(), mock_port})
            .success());
  CHECK(socket.listen().success());
  socket.set_non_blocking(true);
  CHECK(socket.non_blocking_accept().has_error());
  CHECK(socket.close().success());
}

TEST_CASE("[non_blocking_accept() could execute successfully]",
          "[basic_socket.accept]") {
  mock_socket socket{};
}

TEST_CASE("[sync_connect() should execute successfully]",
          "[basic_socket.connect]") {
  mock_socket socket{};
  socket.sync_connect(basic_endpoint<mock_protocol>{});
}

TEST_CASE("[non_blocking_connect() should execute successfully]",
          "[basic_socket.connect]") {
  mock_socket socket{};
  socket.non_blocking_connect(basic_endpoint<mock_protocol>{});
}

TEST_CASE("[recvxxx could compile ok]", "[basic_socket.recv]") {
  mock_socket socket{};
  iovec bufs;
  // recvmsg related
  CHECK(socket.recvmsg(&bufs, 1, 0).error() == errc::bad_file_descriptor);
  CHECK(socket.sync_recvmsg(&bufs, 1, 0).error() == errc::bad_file_descriptor);
  socket.non_blocking_recvmsg(&bufs, 1, 0);

  // recvmsg from related
  mock_protocol::endpoint endpoint;
  ::sockaddr_storage storage;
  int size = endpoint.native_address(&storage);
  socket.recvmsg_from(&bufs, 1, 0, reinterpret_cast<::sockaddr_in*>(&storage),
                      &size);
  socket.sync_recvmsg_from(&bufs, 1, 0,
                           reinterpret_cast<::sockaddr_in*>(&storage), &size);
  socket.non_blocking_recvmsg_from(
      &bufs, 1, 0, reinterpret_cast<::sockaddr_in*>(&storage), &size);

  std::string s;
  s.resize(1024);
  // recv
  socket.recv(s.data(), 1024, 0);
  socket.sync_recv(s.data(), 1024, 0);
  socket.non_blocking_recv(s.data(), 1024, 0);

  // recvfrom.
  auto sz = (uint64_t)size;
  socket.recvfrom(s.data(), 1024, 0, reinterpret_cast<::sockaddr_in*>(&storage),
                  &sz);
  socket.sync_recvfrom(s.data(), 1024, 0,
                       reinterpret_cast<::sockaddr_in*>(&storage), &sz);
  socket.non_blocking_recvfrom(s.data(), 1024, 0,
                               reinterpret_cast<::sockaddr_in*>(&storage), &sz);
}

TEST_CASE("[send_xxx could compile ok]", "[basic_socket.send]") {
  mock_socket socket{};
  iovec bufs;
  // sendmsg
  socket.sendmsg(&bufs, 1, 0);
  socket.sync_sendmsg(&bufs, 1, 0);
  socket.non_blocking_sendmsg(&bufs, 1, 0);

  // sendmsg_to related
  mock_protocol::endpoint endpoint;
  ::sockaddr_storage storage;
  ::socklen_t size = endpoint.native_address(&storage);
  socket.sendmsg_to(&bufs, 1, 0, reinterpret_cast<::sockaddr_in*>(&storage),
                    size);
  socket.sync_sendmsg_to(&bufs, 1, 0,
                         reinterpret_cast<::sockaddr_in*>(&storage), size);
  socket.non_blocking_sendmsg_to(
      &bufs, 1, 0, reinterpret_cast<::sockaddr_in*>(&storage), size);

  std::string s;
  s.resize(1024);

  // send related
  socket.send(s.data(), s.size(), 0);
  socket.sync_send(s.data(), s.size(), 0);
  socket.non_blocking_send(s.data(), s.size(), 0);

  // sendto
  socket.sendto(s.data(), s.size(), 0,
                reinterpret_cast<::sockaddr_in*>(&storage), size);
  socket.sync_sendto(s.data(), s.size(), 0,
                     reinterpret_cast<::sockaddr_in*>(&storage), size);
  socket.non_blocking_sendto(s.data(), s.size(), 0,
                             reinterpret_cast<::sockaddr_in*>(&storage), size);
}
