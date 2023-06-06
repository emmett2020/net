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
#include <chrono>  // NOLINT
#include <exception>
#include <thread>  // NOLINT

#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include "status-code/generic_code.hpp"
#include "status-code/system_code.hpp"
#include "status-code/system_error2.hpp"
#include "stdexec.hpp"

#include "epoll/epoll_context.hpp"
#include "epoll/socket_accept_op.hpp"
#include "ip/address.hpp"
#include "ip/address_v4.hpp"
#include "ip/socket_types.hpp"
#include "ip/tcp.hpp"
#include "ip/udp.hpp"
#include "test_common/receivers.hpp"

using net::epoll_context;
using net::__epoll::accept_sender;
using stdexec::sync_wait;

constexpr port_type mock_port = 12312;

using namespace std::chrono_literals;  // NOLINT

TEST_CASE("[constructor]", "[epoll_socket_accept_op.ctor]") {
  epoll_context ctx{};
  net::ip::tcp::acceptor acceptor{ctx};
  using operation = stdexec::__t<
      epoll_context::socket_accept_op<empty_receiver, net::ip::tcp>>;

  operation op{empty_receiver{}, acceptor};
  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.acceptor_.is_open() == false);
  CHECK(&op.context_ == &acceptor.context_);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE(
    "[socket_accept_sender::__t should satisfy the concept of stdexec::sender]",
    "[epoll_socket_accept_op.sender]") {
  CHECK(stdexec::sender<stdexec::__t<accept_sender<net::ip::tcp>>>);
  CHECK(stdexec::sender<stdexec::__t<accept_sender<net::ip::udp>>>);
}

TEST_CASE("[Example: accept IPv4 request]",
          "[epoll_socket_accept_op.example]") {
  epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // Create acceptor.
  system_error2::system_code ec{system_error2::errc::success};
  net::ip::tcp::endpoint ep{net::ip::address_v4::any(), mock_port};
  net::ip::tcp::acceptor acceptor{ctx, ep, ec, true};
  CHECK(acceptor.set_non_blocking(true).success());
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  exec::scope_guard on_exit_2{[&acceptor]() noexcept {
    CHECK(acceptor.close().success());
  }};
  fmt::print("server file descriptor: {}\n", acceptor.native_handle());
  fmt::print("server listen port: {}\n", mock_port);

  // client could connect to server. Then print the informations of endpoint.
  std::jthread client_thread([&ctx] {
    std::this_thread::sleep_for(500ms);
    net::ip::tcp::socket client{ctx};
    CHECK(client.open(net::ip::tcp::v4()).success());
    CHECK(
        client.connect({net::ip::address_v4::loopback(), mock_port}).success());
    auto peer = client.peer_endpoint();
    CHECK(peer.has_value());
    auto local = client.local_endpoint();
    CHECK(local.has_value());
    fmt::print("client connected to server:\n");
    fmt::print("  client file descriptor: {}\n", client.native_handle());
    fmt::print("  peer : {}:{}\n", peer.value().address().to_string(),
               peer.value().port());
    fmt::print("  local: {}:{}\n", local.value().address().to_string(),
               local.value().port());
    std::this_thread::sleep_for(500ms);
    CHECK(client.close().success());
  });

  // Accept client tcp request.
  stdexec::sender auto s =
      net::async_accept(acceptor)                                          //
      | stdexec::then([](net::ip::tcp::socket&& client_socket) noexcept {  //
          CHECK(client_socket.is_open());
          auto peer = client_socket.peer_endpoint();
          CHECK(peer.has_value());
          auto local = client_socket.local_endpoint();
          CHECK(local.has_value());
          fmt::print("client connected:\n");
          fmt::print("  connected file descriptor: {}\n",
                     client_socket.native_handle());
          fmt::print("  peer : {}:{}\n", peer.value().address().to_string(),
                     peer.value().port());
          fmt::print("  local: {}:{}\n", local.value().address().to_string(),
                     local.value().port());
        }) |
      stdexec::upon_error([](std::error_code&& ec) noexcept {  //
        fmt::print("Error handler. Should never come there in this case.");
        fmt::print("Error message: {}\n", ec.message().c_str());
        CHECK(false);
      });

  sync_wait(std::move(s));
}
