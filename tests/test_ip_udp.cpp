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

#include "ip/udp.hpp"
#include "meta.hpp"

using namespace std;  // NOLINT
using namespace net;  // NOLINT

TEST_CASE("[udp must satisfied the concept of transport_protocol]",
          "[udp.concept]") {
  CHECK(transport_protocol<net::ip::udp>);
}

TEST_CASE("[IPv4 constructor]", "[udp.ctor]") {
  auto proto = net::ip::udp::v4();
  CHECK(proto.family() == AF_INET);
  CHECK(proto.type() == SOCK_DGRAM);
  CHECK(proto.protocol() == IPPROTO_UDP);
}

TEST_CASE("[IPv6 constructor]", "[udp.ctor]") {
  auto proto = net::ip::udp::v6();
  CHECK(proto.family() == AF_INET6);
  CHECK(proto.type() == SOCK_DGRAM);
  CHECK(proto.protocol() == IPPROTO_UDP);
}

TEST_CASE("[comparision of udp]", "[udp.operator=]") {
  CHECK(net::ip::udp::v4() == net::ip::udp::v4());
  CHECK(net::ip::udp::v6() != net::ip::udp::v4());
}
