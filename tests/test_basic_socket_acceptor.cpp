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
#include <optional>
#include <system_error>  // NOLINT

#include "catch2/catch_test_macros.hpp"
#include "status-code/system_code.hpp"

#include "basic_socket_acceptor.hpp"
#include "execution_context.hpp"
#include "mock_protocol.hpp"

TEST_CASE("[Default constructor]", "[basic_socket_acceptor.ctor]") {
  net::execution_context ctx{};
  net::basic_socket_acceptor<mock_protocol> acceptor{ctx};
  CHECK(!acceptor.is_open());
  CHECK(acceptor.state() == 0);
  CHECK(acceptor.protocol_ == std::nullopt);
}

TEST_CASE("[Construct acceptor using an endpoint]",
          "[basic_socket_acceptor.ctor]") {
  net::execution_context ctx{};
  mock_protocol::endpoint endpoint{net::ip::address_v4::any(), 80};
  system_error2::system_code ec;
  net::basic_socket_acceptor<mock_protocol> acceptor{ctx, endpoint, ec, false};
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  CHECK(acceptor.close().success());
}

TEST_CASE("[Construct acceptor using an endpoint and reuse the address]",
          "[basic_socket_acceptor.ctor]") {
  net::execution_context ctx{};
  mock_protocol::endpoint endpoint{net::ip::address_v4::any(), 80};
  system_error2::system_code ec;
  net::basic_socket_acceptor<mock_protocol> acceptor{ctx, endpoint, ec};
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  net::socket_base::reuse_address option{};
  CHECK(acceptor.get_option(option).success());
  CHECK(option.value() == 1);
  CHECK(acceptor.close().success());
}
