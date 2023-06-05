#include <system_error>
#include <catch2/catch_test_macros.hpp>
#include <status-code/generic_code.hpp>
#include <status-code/system_code.hpp>
#include <status-code/system_error2.hpp>
#include <thread>

#include "buffer.hpp"
#include "buffer_sequence_adapter.hpp"
#include "epoll/epoll_context.hpp"
#include "epoll/socket_accept_op.hpp"
#include "epoll/socket_recv_some_op.hpp"
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

TEST_CASE("[construct socket_recv_some_op using std::string and ip::tcp]",
          "[epoll_socket_recv_some_op.ctor]") {
  epoll_context ctx{};
  ip::tcp::socket socket{ctx};
  std::string buf(1024, ' ');
  using operation = stdexec::__t<epoll_context::socket_recv_some_op<size_receiver,  //
                                                                    ip::tcp,        //
                                                                    mutable_buffer>>;
  using bufs_t = buffer_sequence_adapter<mutable_buffer, mutable_buffer>;
  operation op{size_receiver{}, socket, net::buffer(buf)};

  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.bytes_transferred_ == 0);
  CHECK(net::buffer(buf).data() == &buf[0]);
  CHECK(net::buffer(buf).size() == 1024);
  CHECK(bufs_t::first(op.buffers_).data() == &buf[0]);
  CHECK(bufs_t::first(op.buffers_).size() == 1024);
  CHECK(&op.context_ == &ctx);
  CHECK(op.op_type_ == operation::op_type::op_read);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[construct socket_recv_some_op using buffer sequence and ip::tcp]",
          "[epoll_socket_recv_some_op.ctor]") {
  epoll_context ctx{};
  ip::tcp::socket socket{ctx};
  std::string buf1(1024, 'a'), buf2(1024, 'b');
  std::vector<mutable_buffer> vec{buffer(buf1), buffer(buf2)};
  using operation = stdexec::__t<epoll_context::socket_recv_some_op<size_receiver,  //
                                                                    ip::tcp,        //
                                                                    std::vector<mutable_buffer>>>;

  using bufs_t = buffer_sequence_adapter<mutable_buffer, vector<mutable_buffer>>;
  operation op{size_receiver{}, socket, vec};

  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.bytes_transferred_ == 0);
  CHECK(bufs_t::first(op.buffers_).size() == 1024);
  CHECK(bufs_t(op.buffers_).count() == 2);
  CHECK(bufs_t(op.buffers_).total_size() == 2048);
  CHECK(&op.context_ == &ctx);
  CHECK(op.op_type_ == operation::op_type::op_read);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[construct socket_recv_some_op using std::string and ip::udp]",
          "[epoll_socket_recv_some_op.ctor]") {
  epoll_context ctx{};
  ip::udp::socket socket{ctx};
  std::string buf(1024, ' ');
  using operation = stdexec::__t<epoll_context::socket_recv_some_op<size_receiver,  //
                                                                    ip::udp,        //
                                                                    mutable_buffer>>;

  using bufs_t = buffer_sequence_adapter<mutable_buffer, mutable_buffer>;
  operation op{size_receiver{}, socket, net::buffer(buf)};

  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.bytes_transferred_ == 0);
  CHECK(op.buffers_.data() == &buf[0]);
  CHECK(op.buffers_.size() == 1024);
  CHECK(bufs_t::first(op.buffers_).data() == &buf[0]);
  CHECK(bufs_t::first(op.buffers_).size() == 1024);
  CHECK(&op.context_ == &ctx);
  CHECK(op.op_type_ == operation::op_type::op_read);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[complete should set correct signal to downstream receiver depends on error code]",
          "[epoll_socket_recv_some_op.complete]") {
  epoll_context ctx{};
  ip::tcp::socket socket{ctx};
  char buf[1024];
  using operation = stdexec::__t<epoll_context::socket_recv_some_op<size_receiver,  //
                                                                    ip::tcp,        //
                                                                    mutable_buffer>>;
  operation op{size_receiver{}, socket, net::buffer(buf)};

  // set_value
  op.ec_ = system_error2::errc::success;
  operation::complete(&op);

  // set_error
  op.ec_ = system_error2::errc::operation_not_supported;
  operation::complete(&op);

  // set_stopped
  op.ec_ = system_error2::errc::operation_canceled;
  operation::complete(&op);
}

TEST_CASE("[construct sender using std::string and ip::tcp::socket]", "[recv_some_sender.ctor]") {
  epoll_context ctx{};
  ip::tcp::socket socket{ctx};
  std::string buf(1024, ' ');
  stdexec::__t<recv_some_sender<ip::tcp, mutable_buffer>> s{socket, net::buffer(buf)};
  CHECK(s.socket_.is_open() == false);
  CHECK(s.buffers_.data() == &buf[0]);
  CHECK(s.buffers_.size() == 1024);
}

TEST_CASE("[recv_some_sender::__t should satisfy stdexec::sender]", "[recv_some_sender.concept]") {
  CHECK(stdexec::sender<stdexec::__t<recv_some_sender<ip::tcp, mutable_buffer>>>);
}

TEST_CASE("[async_recv_some with different stdexec stuffs should work]",
          "[recv_some_sender.stdexec]") {
  epoll_context ctx{};
  ip::tcp::socket socket{ctx};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept { ctx.request_stop(); }};
  char buf[1024];

  auto error_handler = [](std::error_code &&ec) noexcept -> size_t { return 0u; };

  // 1. sync_wait, upon_error
  sender auto s1 = async_recv_some(socket, buffer(buf)) | upon_error(error_handler);
  sync_wait(std::move(s1));

  // 2. sync_wait, then
  sender auto s2 = async_recv_some(socket, buffer(buf))                 //
                   | then([](size_t bytes) noexcept { return bytes; })  //
                   | upon_error(error_handler);
  sync_wait(std::move(s2));

  // 3.  then, upon_error, sync_wait
  sender auto s3 = async_recv_some(socket, buffer(buf))  //
                   | then([](size_t bytes_transferred) noexcept {})
                   | upon_error([](error_code &&ec) noexcept {});
  sync_wait(std::move(s3));

  // 4.  then, upon_error, start_detached
  sender auto s4 = async_recv_some(socket, buffer(buf))  //
                   | then([](size_t bytes_transferred) noexcept {})
                   | upon_error([](error_code &&ec) noexcept {});
  start_detached(std::move(s4));
  std::this_thread::sleep_for(500ms);

  // 5. on, then, upon_error.
  sender auto s5 = stdexec::on(ctx.get_scheduler(),                  //
                               async_recv_some(socket, buffer(buf))  //
                                   | then([](size_t bytes_transferred) noexcept {})
                                   | upon_error([](error_code &&ec) noexcept {}));
  sync_wait(std::move(s5));

  // 6. when_any, then, upon_error.
  sender auto s6 = exec::when_any(async_recv_some(socket, buffer(buf))  //
                                  | then([](size_t bytes_transferred) noexcept {})
                                  | upon_error([](error_code &&ec) noexcept {}));
  sync_wait(std::move(s6));

  // 7. when_all, schedule_after, then, upon_error.
  sender auto s7 = stdexec::when_all(async_recv_some(socket, buffer(buf))  //
                                         | then([](size_t bytes_transferred) noexcept {})
                                         | upon_error([](error_code &&ec) noexcept {}),
                                     exec::schedule_after(ctx.get_scheduler(), 500ms));
  sync_wait(std::move(s7));

  // 8. repeat_effect_until, then, upon_error.
  sender auto s8 = async_recv_some(socket, buffer(buf))  //
                   | then([](size_t bytes) noexcept {    //
                       return bytes < 1000;
                     })
                   | upon_error([](error_code &&ec) noexcept { return true; })  //
                   | repeat_effect_until();
  sync_wait(std::move(s8));
}

TEST_CASE("[CPO: `start` performed operation]", "[epoll_socket_recv_some_op.start]") {
  epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_context_exit{[&ctx]() noexcept { ctx.request_stop(); }};

  // Create acceptor.
  system_error2::system_code ec{system_error2::errc::success};
  ip::tcp::acceptor acceptor{ctx, {ip::address_v4::any(), mock_port}, ec, true};
  CHECK(acceptor.set_non_blocking(true).success());
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  exec::scope_guard on_acceptor_exit{[&acceptor]() noexcept { CHECK(acceptor.close().success()); }};
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

  std::string buf(1024, ' ');
  // Accept client tcp request.
  sender auto s =
      async_accept(acceptor)                                           //
      | then([&buf, &ctx](ip::tcp::socket &&client_socket) noexcept {  //
          CHECK(client_socket.is_open());
          auto peer = client_socket.peer_endpoint();
          CHECK(peer.has_value());
          auto local = client_socket.local_endpoint();
          CHECK(local.has_value());
          fmt::print("client connected:\n");
          fmt::print("  connected file descriptor: {}\n", client_socket.native_handle());
          fmt::print("  peer : {}:{}\n", peer.value().address().to_string(), peer.value().port());
          fmt::print("  local: {}:{}\n", local.value().address().to_string(), local.value().port());

          sender auto s1 = exec::when_any(
              stdexec::on(
                  ctx.get_scheduler(),
                  async_recv_some(client_socket, buffer(buf))
                      | then([](size_t sz) noexcept { fmt::print("recv sz: {}\n", sz); })  //
                      | upon_error([](error_code &&ec) noexcept {
                          fmt::print("Error: {}\n", ec.message().c_str());
                        })),
              exec::schedule_after(ctx.get_scheduler(), 1s)
                  | then([] { fmt::print("Time out after 1s\n"); }));

          start_detached(std::move(s1));
        })
      | upon_error([](error_code &&ec) noexcept {  //
          fmt::print("Error message: {}\n", ec.message().c_str());
          CHECK(false);
        });

  sync_wait(std::move(s));
  std::this_thread::sleep_for(1.5s);
}
// TEST_CASE("[]", "[epoll_socket_recv_some_op]") {}