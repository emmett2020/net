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
#include <sys/socket.h>

#include "catch2/catch_test_macros.hpp"

#include "ip/tcp.hpp"
#include "meta.hpp"

using namespace std;  // NOLINT
using namespace net;  // NOLINT

TEST_CASE("[tcp must satisfied the concept of transport_protocol]",
          "[tcp.concept]") {
  CHECK(transport_protocol<net::ip::tcp>);
}

TEST_CASE("[IPv4 constructor]", "[tcp.ctor]") {
  auto proto = net::ip::tcp::v4();
  CHECK(proto.family() == AF_INET);
  CHECK(proto.type() == SOCK_STREAM);
  CHECK(proto.protocol() == IPPROTO_TCP);
}

TEST_CASE("[IPv6 constructor]", "[tcp.ctor]") {
  auto proto = net::ip::tcp::v6();
  CHECK(proto.family() == AF_INET6);
  CHECK(proto.type() == SOCK_STREAM);
  CHECK(proto.protocol() == IPPROTO_TCP);
}

TEST_CASE("[comparision of tcp]", "[tcp.operator=]") {
  CHECK(net::ip::tcp::v4() == net::ip::tcp::v4());
  CHECK(net::ip::tcp::v6() != net::ip::tcp::v4());
}
