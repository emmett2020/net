#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ip/address.hpp"

using namespace net::ip;

static constexpr address_v6::bytes_type v4_mapped{0, 0, 0,    0,    0, 0, 0,    0,
                                                  0, 0, 0xff, 0xff, 0, 0, 0xfe, 0xff};
static constexpr address_v6::bytes_type multicast{0xff, 0, 0,    0,    0,    0,    0,    0,
                                                  0,    0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};

TEST_CASE("[default ctor should return unspecified address v6]", "[address.ctor]") {
  address unspecified_addr{};
  CHECK(unspecified_addr.is_v6());
  CHECK(unspecified_addr.is_unspecified());
}

TEST_CASE("[constructor using address_v6]", "[address.ctor]") {
  address addr{address_v6{multicast}};
  CHECK(addr.is_multicast());
  CHECK(addr.is_v6());
}

TEST_CASE("[constructor using address_v4]", "[address.ctor]") {
  address addr{make_address_v4("127.0.0.1")};
  CHECK(addr.is_loopback());
  CHECK(addr.is_v4());
}

TEST_CASE("[copy constructor]", "[address.ctor]") {
  address addr{address_v6{multicast}};
  address addr_copy{addr};
  CHECK(addr_copy.to_string() == "ff00::910:1112:1314");
  CHECK(addr == addr_copy);
  CHECK(addr.is_v6());
}

TEST_CASE("[move constructor]", "[address.ctor]") {
  address addr{address_v6{multicast}};
  address addr6_copy{std::move(addr)};
  CHECK(addr6_copy.to_string() == "ff00::910:1112:1314");
  CHECK(addr.is_v6());
}

TEST_CASE("[throw exception when try to get wrong version]", "[address.ctor]") {
  address addr_v6{address_v6{multicast}};
  CHECK((addr_v6.is_v6() && !addr_v6.is_v4()));
  CHECK(addr_v6.to_v6().is_multicast());
  REQUIRE_THROWS_AS((addr_v6.to_v4().is_unspecified()), std::bad_variant_access);

  address addr_v4{make_address_v4("127.0.0.1")};
  CHECK((addr_v4.is_v4() && !addr_v4.is_v6()));
  CHECK(addr_v4.to_v4().is_loopback());
  REQUIRE_THROWS_AS((addr_v4.to_v6().is_loopback()), std::bad_variant_access);
}

TEST_CASE("[operation <=> should work]", "[address.comparision]") {
  CHECK(address{address_v6{multicast}} == address{address_v6{multicast}});
  CHECK(address{address_v6{v4_mapped}} < address{address_v6{multicast}});
}

TEST_CASE("[native_address should return sockaddr_storage]", "[address.native_address]") {
  address normal{address_v6{multicast}};
  CHECK(normal.is_v6());

  ::sockaddr_storage storage;
  socklen_t size = normal.native_address(&storage, 80);
  CHECK(size == sizeof(::sockaddr_in6));
  auto addr = *reinterpret_cast<::sockaddr_in6 *>(&storage);
  CHECK(addr.sin6_family == AF_INET6);
  CHECK(addr.sin6_port == htons(80));
}

TEST_CASE("hash address should work", "address.hash") {
  std::unordered_map<address, bool> addresses;
  addresses[make_address_v6(multicast)] = false;
  CHECK(addresses[make_address_v6(multicast)] == false);

  addresses[make_address_v6(multicast)] = true;
  CHECK(addresses[make_address_v6(multicast)] == true);
}
