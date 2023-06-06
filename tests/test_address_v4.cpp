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
#include <netinet/in.h>
#include <sys/socket.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "catch2/catch_test_macros.hpp"

#include "ip/address_v4.hpp"

using net::ip::address_v4;
using net::ip::make_address_v4;

static const std::vector<std::string> str_ip{"0.0.0.0", "127.0.0.1",
                                             "224.0.0.0", "120.121.122.123"};
static const std::vector<uint32_t> uint_ip{0, 2130706433, 3758096384,
                                           2021227131};

// These unit tests should work fine on a big-ended machine, but since I don't
// have a big-ended machine, I can't guarantee reliability. If you find a bug on
// a big-endian machine, you are welcome to submit an issue.

TEST_CASE("[default ctor should return unspecified address]",
          "[address_v4.ctor]") {
  address_v4 unspecified{};
  CHECK(unspecified.to_uint() == 0);
  CHECK(unspecified.to_bytes() == address_v4::bytes_type{0, 0, 0, 0});
  CHECK(unspecified.to_string() == "0.0.0.0");
  CHECK(unspecified.is_unspecified());
}

TEST_CASE("[construct an address from bytes of different network type]",
          "[address_v4.ctor]") {
  address_v4 unspecified{address_v4::bytes_type(0, 0, 0, 0)};
  CHECK(unspecified.to_uint() == uint_ip[0]);
  CHECK(unspecified.to_bytes() == address_v4::bytes_type{0, 0, 0, 0});
  CHECK(unspecified.to_string() == "0.0.0.0");
  CHECK(unspecified.is_unspecified());

  address_v4 loopback{address_v4::bytes_type(127, 0, 0, 1)};
  CHECK(loopback.to_uint() == uint_ip[1]);
  CHECK(loopback.to_bytes() == address_v4::bytes_type{127, 0, 0, 1});
  CHECK(loopback.to_string() == "127.0.0.1");
  CHECK(loopback.is_loopback());

  address_v4 multicast{address_v4::bytes_type(224, 0, 0, 0)};
  CHECK(multicast.to_uint() == uint_ip[2]);
  CHECK(multicast.to_bytes() == address_v4::bytes_type{224, 0, 0, 0});
  CHECK(multicast.to_string() == "224.0.0.0");
  CHECK(multicast.is_multicast());

  address_v4 normal{address_v4::bytes_type(120, 121, 122, 123)};
  CHECK(normal.to_uint() == uint_ip[3]);
  CHECK(normal.to_bytes() == address_v4::bytes_type{120, 121, 122, 123});
  CHECK(normal.to_string() == "120.121.122.123");
  CHECK((!normal.is_loopback() && !normal.is_multicast() &&
         !normal.is_unspecified()));
}

TEST_CASE("[construct an address from unsigned integer in host byte order]",
          "[address_v4.ctor]") {
  address_v4 unspecified{uint_ip[0]};
  CHECK(unspecified.to_uint() == uint_ip[0]);
  CHECK(unspecified.to_bytes() == address_v4::bytes_type{0, 0, 0, 0});
  CHECK(unspecified.to_string() == "0.0.0.0");
  CHECK(unspecified.is_unspecified());

  address_v4 loopback{uint_ip[1]};
  CHECK(loopback.to_uint() == uint_ip[1]);
  CHECK(loopback.to_bytes() == address_v4::bytes_type{127, 0, 0, 1});
  CHECK(loopback.to_string() == "127.0.0.1");
  CHECK(loopback.is_loopback());

  address_v4 multicast{uint_ip[2]};
  CHECK(multicast.to_uint() == uint_ip[2]);
  CHECK(multicast.to_bytes() == address_v4::bytes_type{224, 0, 0, 0});
  CHECK(multicast.to_string() == "224.0.0.0");
  CHECK(multicast.is_multicast());

  address_v4 normal{uint_ip[3]};
  CHECK(normal.to_uint() == uint_ip[3]);
  CHECK(normal.to_bytes() == address_v4::bytes_type{120, 121, 122, 123});
  CHECK(normal.to_string() == "120.121.122.123");
  CHECK((!normal.is_loopback() && !normal.is_multicast() &&
         !normal.is_unspecified()));
}

TEST_CASE("[copy constructor]", "[address_v4.ctor]") {
  address_v4 normal{uint_ip[3]};
  address_v4 normal_copy{normal};
  CHECK(normal_copy.to_uint() == uint_ip[3]);
  CHECK(normal_copy.to_bytes() == address_v4::bytes_type{120, 121, 122, 123});
  CHECK(normal_copy.to_string() == "120.121.122.123");
  CHECK((!normal_copy.is_loopback() && !normal_copy.is_multicast() &&
         !normal_copy.is_unspecified()));
  CHECK(normal == normal_copy);
}

TEST_CASE("[move constructor]", "[address_v4.ctor]") {
  address_v4 normal{uint_ip[3]};
  address_v4 normal_copy{std::move(normal)};
  CHECK(normal_copy.to_uint() == uint_ip[3]);
  CHECK(normal_copy.to_bytes() == address_v4::bytes_type{120, 121, 122, 123});
  CHECK(normal_copy.to_string() == "120.121.122.123");
  CHECK((!normal_copy.is_loopback() && !normal_copy.is_multicast() &&
         !normal_copy.is_unspecified()));
}

TEST_CASE("[operation <=> should work]", "[address_v4.comparision]") {
  CHECK(address_v4{111} == address_v4{111});
  CHECK(address_v4{111} < address_v4{112});
  CHECK(address_v4{111} > address_v4{110});
}

TEST_CASE("[native_address should return sockaddr_storage]",
          "[address_v4.native_address]") {
  address_v4 normal{uint_ip[3]};
  ::sockaddr_storage storage;
  socklen_t size = normal.native_address(&storage, 80);
  CHECK(size == sizeof(::sockaddr_in));
  auto addr = *reinterpret_cast<::sockaddr_in*>(&storage);
  CHECK(addr.sin_family == AF_INET);
  CHECK(addr.sin_port == htons(80));
  CHECK(addr.sin_addr.s_addr == htonl(uint_ip[3]));
}

TEST_CASE("address_v4::loopback() should return loopback address",
          "[address_v4.loopback]") {
  CHECK(address_v4::loopback().is_loopback());
  CHECK(address_v4::loopback().to_string() == "127.0.0.1");

  ::sockaddr_storage storage;
  ::socklen_t size = address_v4::loopback().native_address(&storage, 80);
  ::sockaddr_in addr = *reinterpret_cast<::sockaddr_in*>(&storage);
  CHECK(addr.sin_family == AF_INET);
  CHECK(addr.sin_addr.s_addr == htonl(address_v4::loopback().to_uint()));
  CHECK(addr.sin_port == ntohs(80));
}

TEST_CASE("[pass well-formed address to make_address_v4 should work]",
          "[address_v4.make_address_v4]") {
  CHECK(make_address_v4("127.0.0.1").is_loopback());

  address_v4 v4 = make_address_v4("127.0.0.1");
  CHECK(v4.to_string() == "127.0.0.1");

  v4 = make_address_v4("120.121.122.123");
  CHECK(v4.to_string() == "120.121.122.123");

  v4 = make_address_v4(std::string_view{"120.121.122.123"});
  CHECK(v4.to_string() == "120.121.122.123");

  v4 = make_address_v4(std::string{"120.121.122.123"});
  CHECK(v4.to_string() == "120.121.122.123");

  v4 = make_address_v4(uint_ip[3]);
  CHECK(v4.to_string() == "120.121.122.123");

  v4 = make_address_v4(address_v4::bytes_type{120, 121, 122, 123});
  CHECK(v4.to_string() == "120.121.122.123");
}

TEST_CASE(
    "[pass ill-formed address to make_address_v4 should return unspecified]",
    "[address_v4.make_address_v4]") {
  CHECK(make_address_v4("300.0.0.0").is_unspecified());
  CHECK(make_address_v4("300.0.0").is_unspecified());
  CHECK(make_address_v4("300.0.0.0.0").is_unspecified());
}

TEST_CASE("hash address_v4", "address_v4.hash") {
  std::unordered_map<address_v4, bool> addresses;
  addresses[make_address_v4("120.121.122.123")] = false;
  addresses[make_address_v4("1.1.1.1")] = false;
  CHECK(addresses[make_address_v4("120.121.122.123")] == false);
  CHECK(addresses[make_address_v4("1.1.1.1")] == false);

  addresses[make_address_v4("120.121.122.123")] = true;
  addresses[make_address_v4("1.1.1.1")] = true;
  CHECK(addresses[make_address_v4("120.121.122.123")] == true);
  CHECK(addresses[make_address_v4("1.1.1.1")] == true);
}
