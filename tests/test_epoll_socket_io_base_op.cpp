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
#include <thread>  // NOLINT

#include "catch2/catch_test_macros.hpp"
#include "status-code/generic_code.hpp"
#include "status-code/system_error2.hpp"

#include "epoll/epoll_context.hpp"
#include "epoll/socket_io_base_op.hpp"
#include "ip/tcp.hpp"
#include "ip/udp.hpp"
#include "test_common/receivers.hpp"

using namespace std::chrono_literals;  // NOLINT

using net::epoll_context;

template <typename Receiver>
using socket_base_op_tcp =
    epoll_context::socket_io_base_op<Receiver, net::ip::tcp>;

template <typename Receiver>
struct socket_increment_operation_tcp
    : stdexec::__t<socket_base_op_tcp<Receiver>> {
  using base_type = stdexec::__t<socket_base_op_tcp<Receiver>>;
  using socket_type = typename net::ip::tcp::socket;

  static void perform(base_type* base) noexcept {
    auto& op = *static_cast<socket_increment_operation_tcp*>(base);
    ++op.cnt;
  }

  static void complete(base_type* base) noexcept {
    auto& op = *static_cast<socket_increment_operation_tcp*>(base);
    stdexec::set_value(std::move(op.receiver_));
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
  net::ip::tcp::socket s{ctx};
  using operation = stdexec::__t<
      epoll_context::socket_io_base_op<empty_receiver, net::ip::tcp>>;
  using vtable = operation::op_vtable;

  vtable table;
  table.complete = [](operation*) noexcept {  // NOLINT
  };
  table.perform = [](operation*) noexcept {  // NOLINT
  };
  operation op{empty_receiver{}, s, table, operation::op_type::op_read};
  (void)op.receiver_;
  CHECK(op.socket_.is_open() == false);
  CHECK(op.op_vtable_ == &table);
  CHECK(&op.context_ == &s.context_);
  CHECK(op.op_type_ == operation::op_type::op_read);
  CHECK(op.ec_ == system_error2::errc::success);
}

TEST_CASE("[CPO: `start` performed operation]",
          "[epoll_socket_base_operation.start]") {
  //  The operation can be performed twice at most. The operation is performed
  //  once before enqueue. If the error code is try_again or would_block after
  //  the first execution, it will be registered to epoll and then executed a
  //  second time after epoll wakes it up.
  epoll_context ctx{};
  std::jthread io_thread([&]() { ctx.run(); });
  exec::scope_guard on_exit{[&ctx]() noexcept {
    ctx.request_stop();
  }};
  net::ip::tcp::socket s{ctx};

  // perform operation once
  socket_increment_operation_tcp<empty_receiver> op{empty_receiver{}, s};
  stdexec::start(op);
  std::this_thread::sleep_for(50ms);
  CHECK(op.cnt == 1);
}
