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

#include <iostream>
#include <string>
#include <system_error>  // NOLINT
#include <thread>        // NOLINT
#include <unordered_map>
#include <vector>

#include "buffer.hpp"
#include "epoll/epoll_context.hpp"
#include "epoll/socket_accept_op.hpp"
#include "epoll/socket_io_base_op.hpp"
#include "epoll/socket_recv_some_op.hpp"
#include "epoll/socket_send_some_op.hpp"
#include "ip/tcp.hpp"
#include "net_error.hpp"

#include "exec/repeat_effect_until.hpp"
#include "fmt/format.h"
#include "status-code/generic_code.hpp"
#include "status-code/result.hpp"
#include "status-code/system_code.hpp"
#include "status-code/system_error2.hpp"
#include "stdexec.hpp"
#include "stdexec/execution.hpp"

using namespace std::chrono_literals;  // NOLINT
namespace ex = stdexec;
using net::async_recv_some;
using net::async_send_some;
using system_error2::errc;
using system_error2::system_code;

constexpr port_type port = 12312;

inline uint64_t simple_uuid() {
  static uint64_t n = 0;
  return n++;
}

struct client {
  std::optional<net::ip::tcp::socket> socket;
  std::string buf;
};

int main(int argc, char* argv[]) {
  // Prepare context.
  net::epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard context_guard{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // Prepare acceptor.
  system_code code{errc::success};
  net::ip::tcp::endpoint ep{net::ip::address_v4::any(), port};
  net::ip::tcp::acceptor acceptor{ctx, ep, code, true};
  acceptor.set_non_blocking(true);
  exec::scope_guard acceptor_guard{[&acceptor]() noexcept {
    acceptor.close();
  }};
  fmt::print("Server listen: {}, fd: {}\n", port, acceptor.native_handle());

  // Prepare sockets and buffer containers.
  std::unordered_map<uint64_t, client> clients;

  // clang-format off
  // Echo whatever received from client.
  ex::sender auto s =
      net::async_accept(acceptor)
      | ex::then([&clients, &ctx](net::ip::tcp::socket&& sock) noexcept {
          uint64_t uuid = simple_uuid();
          fmt::print("client uuid: {}, fd: {}\n", uuid, sock.native_handle());

          clients[uuid].socket = std::move(sock);
          clients[uuid].buf.resize(1024);
          auto& socket = clients[uuid].socket.value();
          auto& buf = clients[uuid].buf;

          ex::sender auto s1 = exec::repeat_effect_until(ex::on(
              ctx.get_scheduler(),
              async_recv_some(socket, net::buffer(buf))
                  | ex::let_value([&buf, &socket](size_t sz) noexcept {
                      net::const_buffer const_buf = net::buffer(buf, sz);
                      return async_send_some(socket, const_buf)
                            | ex::then([](size_t sz) noexcept {
                              return false;
                            });
                    })
                  | ex::upon_error([&socket](auto&&) noexcept {
                      fmt::print("close: {}.\n", socket.native_handle());
                      socket.close();
                      return true;
                    })));

          ex::start_detached(std::move(s1));
          return false;
        })
      | ex::upon_error([](std::error_code&& ec) noexcept {
          fmt::print("Error: {}\n", ec.message().c_str());
          return true;
        })
      | exec::repeat_effect_until();
  // clang-format on

  ex::sync_wait(std::move(s));

  return 0;
}
