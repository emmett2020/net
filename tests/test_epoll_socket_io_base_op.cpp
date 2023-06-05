#include <catch2/catch_test_macros.hpp>
#include <status-code/generic_code.hpp>
#include <status-code/system_error2.hpp>
#include <thread>

#include "epoll/epoll_context.hpp"
#include "epoll/socket_io_base_op.hpp"
#include "ip/tcp.hpp"
#include "ip/udp.hpp"
#include "test_common/receivers.hpp"

using namespace std;
using namespace net;
using namespace net::__epoll;
using namespace stdexec;
using namespace exec;

template <typename Receiver>
using socket_base_op_tcp = epoll_context::socket_io_base_op<Receiver, ip::tcp>;

template <typename Receiver>
struct socket_increment_operation_tcp : stdexec::__t<socket_base_op_tcp<Receiver>> {
  using base_type = stdexec::__t<socket_base_op_tcp<Receiver>>;
  using socket_type = typename ip::tcp::socket;

  static void perform(base_type* base) noexcept {
    auto& op = *static_cast<socket_increment_operation_tcp*>(base);
    ++op.cnt;
  }
  static void complete(base_type* base) noexcept {
    auto& op = *static_cast<socket_increment_operation_tcp*>(base);
    set_value(std::move(op.receiver_));
  }
  static constexpr typename base_type::op_vtable vtable{&perform, &complete};
  socket_increment_operation_tcp(Receiver receiver, socket_type& socket)
      : base_type(std::move(receiver),           //
                  socket,                        //
                  vtable,                        //
                  base_type::op_type::op_read),  //
        cnt(0) {}

  int cnt;
};

TEST_CASE("[constructor]", "[epoll_socket_base_operation.ctor]") {
  epoll_context ctx{};
  ip::tcp::socket s{ctx};
  using operation = stdexec::__t<epoll_context::socket_io_base_op<empty_receiver, ip::tcp>>;
  using vtable = operation::op_vtable;

  vtable table;
  table.complete = [](operation*) noexcept {};
  table.perform = [](operation*) noexcept {};
  operation op{empty_receiver{}, s, table, operation::op_type::op_read};
  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.op_vtable_ == &table);
  CHECK(&op.context_ == &s.context_);
  CHECK(op.op_type_ == operation::op_type::op_read);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[CPO: `start` performed operation]", "[epoll_socket_base_operation.start]") {
  //  The operation can be performed twice at most. The operation is performed once before enqueue.
  //  If the error code is try_again or would_block after the first execution, it will be registered
  //  to epoll and then executed a second time after epoll wakes it up.
  epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept { ctx.request_stop(); }};
  ip::tcp::socket s{ctx};

  // perform operation once
  socket_increment_operation_tcp<empty_receiver> op{empty_receiver{}, s};
  start(op);
  this_thread::sleep_for(50ms);
  CHECK(op.cnt == 1);
}