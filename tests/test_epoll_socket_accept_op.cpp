#include <catch2/catch_test_macros.hpp>
#include <exception>
#include <status-code/generic_code.hpp>
#include <status-code/system_code.hpp>
#include <status-code/system_error2.hpp>
#include <thread>

#include "epoll/epoll_context.hpp"
#include "epoll/socket_accept_op.hpp"
#include "exec/scope.hpp"
#include "ip/address.hpp"
#include "ip/address_v4.hpp"
#include "ip/socket_types.hpp"
#include "ip/tcp.hpp"
#include "ip/udp.hpp"
#include "stdexec.hpp"
#include "test_common/receivers.hpp"

using namespace std;
using namespace net;
using namespace net::__epoll;
using namespace stdexec;
using namespace exec;

constexpr port_type mock_port = 12312;

TEST_CASE("[constructor]", "[epoll_socket_accept_op.ctor]") {
  epoll_context ctx{};
  ip::tcp::acceptor acceptor{ctx};
  using operation = stdexec::__t<epoll_context::socket_accept_op<empty_receiver, net::ip::tcp>>;

  operation op{empty_receiver{}, acceptor};
  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.acceptor_.is_open() == false);
  CHECK(&op.context_ == &acceptor.context_);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[socket_accept_sender::__t should satisfy the concept of stdexec::sender]",
          "[epoll_socket_accept_op.sender]") {
  CHECK(stdexec::sender<stdexec::__t<accept_sender<ip::tcp>>>);
  CHECK(stdexec::sender<stdexec::__t<accept_sender<ip::udp>>>);
}

TEST_CASE("[Example: accept IPv4 request]", "[epoll_socket_accept_op.example]") {
  epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept { ctx.request_stop(); }};

  // Create acceptor.
  system_error2::system_code ec{system_error2::errc::success};
  ip::tcp::acceptor acceptor{ctx, {ip::address_v4::any(), mock_port}, ec, true};
  CHECK(acceptor.set_non_blocking(true).success());
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  exec::scope_guard on_exit_2{[&acceptor]() noexcept { CHECK(acceptor.close().success()); }};
  fmt::print("server file descriptor: {}\n", acceptor.native_handle());
  fmt::print("server listen port: {}\n", mock_port);

  // client could connect to server. Then print the informations of endpoint.
  std::jthread client_thread([&ctx] {
    std::this_thread::sleep_for(500ms);
    ip::tcp::socket client{ctx};
    CHECK(client.open(ip::tcp::v4()).success());
    CHECK(client.connect({ip::address_v4::loopback(), mock_port}).success());
    auto peer = client.peer_endpoint();
    CHECK(peer.has_value());
    auto local = client.local_endpoint();
    CHECK(local.has_value());
    fmt::print("client connected to server:\n");
    fmt::print("  client file descriptor: {}\n", client.native_handle());
    fmt::print("  peer : {}:{}\n", peer.value().address().to_string(), peer.value().port());
    fmt::print("  local: {}:{}\n", local.value().address().to_string(), local.value().port());
    std::this_thread::sleep_for(500ms);
    CHECK(client.close().success());
  });

  // Accept client tcp request.
  sender auto s =
      async_accept(acceptor)                                 //
      | then([](ip::tcp::socket &&client_socket) noexcept {  //
          CHECK(client_socket.is_open());
          auto peer = client_socket.peer_endpoint();
          CHECK(peer.has_value());
          auto local = client_socket.local_endpoint();
          CHECK(local.has_value());
          fmt::print("client connected:\n");
          fmt::print("  connected file descriptor: {}\n", client_socket.native_handle());
          fmt::print("  peer : {}:{}\n", peer.value().address().to_string(), peer.value().port());
          fmt::print("  local: {}:{}\n", local.value().address().to_string(), local.value().port());
        })
      | upon_error([](std::error_code &&ec) noexcept {  //
          fmt::print("Error handler. Should never come there in this case.");
          fmt::print("Error message: {}\n", ec.message().c_str());
          CHECK(false);
        });

  sync_wait(std::move(s));
}
