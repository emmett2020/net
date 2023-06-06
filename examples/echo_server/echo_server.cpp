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
#include "exec/repeat_effect_until.hpp"
#include "fmt/format.h"
#include "ip/tcp.hpp"
#include "net_error.hpp"
#include "status-code/generic_code.hpp"
#include "status-code/result.hpp"
#include "status-code/system_code.hpp"
#include "status-code/system_error2.hpp"
#include "stdexec.hpp"

using namespace std::chrono_literals;  // NOLINT

using system_error2::system_code;

constexpr port_type mock_port = 12312;

inline uint64_t simple_uuid() {
  static uint64_t n = 0;
  return n++;
}

struct client {
  std::optional<net::ip::tcp::socket> socket;
};

int main(int argc, char* argv[]) {
  std::cout << "main thread: " << std::this_thread::get_id() << std::endl;

  net::epoll_context ctx{};
  std::jthread io_thread([&]() {
    std::cout << "io thread: " << std::this_thread::get_id() << std::endl;
    std::cout << "start to run context." << std::endl;
    ctx.run();
    std::cout << "stopped to run context." << std::endl;
  });
  exec::scope_guard on_context_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};

  // Create acceptor.
  system_error2::system_code ec{system_error2::errc::success};
  net::ip::tcp::endpoint ep{net::ip::address_v4::any(), mock_port};
  net::ip::tcp::acceptor acceptor{ctx, ep, ec, true};
  acceptor.set_non_blocking(true).success();
  exec::scope_guard on_acceptor_exit{[&acceptor]() noexcept {
    acceptor.close().success();
  }};

  fmt::print("server file descriptor: {}\n", acceptor.native_handle());
  fmt::print("server listen port: {}\n", mock_port);

  std::unordered_map<uint64_t, std::optional<net::ip::tcp::socket>> sockets;
  std::unordered_map<uint64_t, std::string> bufs;

  // Accept client tcp request.
  stdexec::sender auto s =
      net::async_accept(acceptor)  //
      | stdexec::then([&bufs, &ctx, &sockets](
                          net::ip::tcp::socket&& client_socket) noexcept {  //
          auto peer = client_socket.peer_endpoint();
          auto local = client_socket.local_endpoint();
          assert(peer.has_value() && local.has_value());
          uint64_t uuid = simple_uuid();

          fmt::print("client connected: {}\n", client_socket.is_open());
          fmt::print("connected descriptor: {}, uuid:{}\n",
                     client_socket.native_handle(), uuid);
          fmt::print("peer : {}:{}\n", peer.value().address().to_string(),
                     peer.value().port());
          fmt::print("local: {}:{}\n", local.value().address().to_string(),
                     local.value().port());

          sockets[uuid] = std::move(client_socket);
          bufs[uuid].resize(1024, 'x');
          std::string& buf = bufs[uuid];

          stdexec::sender auto s1 = exec::repeat_effect_until(stdexec::on(
              ctx.get_scheduler(),
              net::async_recv_some(sockets[uuid].value(), net::buffer(buf))  //
                  | stdexec::then([&buf](size_t sz) noexcept {               //
                      fmt::print("recv sz: {}\n", sz);
                      fmt::print("{}", buf.substr(0, sz));
                      return false;
                    })  //
                  | stdexec::upon_error(
                        [&sockets, uuid](std::error_code&& ec) noexcept {  //
                          fmt::print("Error: {}\n", ec.message().c_str());
                          fmt::print("Close: {}\n",
                                     sockets[uuid].value().close().success());
                          return true;
                        })));

          stdexec::start_detached(std::move(s1));
          return false;
        }) |
      stdexec::upon_error([](std::error_code&& ec) noexcept {  //
        fmt::print("Error message: {}\n", ec.message().c_str());
        return true;
      }) |
      exec::repeat_effect_until();

  stdexec::sync_wait(std::move(s));
  std::this_thread::sleep_for(150s);

  return 0;
}
