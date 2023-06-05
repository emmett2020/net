#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ip/address_v6.hpp"

using namespace net::ip;

static constexpr address_v6::bytes_type unspecified{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static constexpr address_v6::bytes_type loopback{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static constexpr address_v6::bytes_type link_local{0xfe, 0x80, 0, 0, 0,    0,    0,    0,
                                                   0,    0,    0, 0, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type site_local{0xfe, 0xc0, 0, 0, 0,    0,    0,    0,
                                                   0,    0,    0, 0, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type v4_mapped{0, 0, 0,    0,    0, 0, 0,    0,
                                                  0, 0, 0xff, 0xff, 0, 0, 0xfe, 0xff};
static constexpr address_v6::bytes_type multicast{0xff, 0, 0,    0,    0,    0,    0,    0,
                                                  0,    0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type multicast_global{
    0xff, 0x0e, 0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type multicast_link_local{
    0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type multicast_node_local{
    0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type multicast_org_local{
    0xff, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};
static constexpr address_v6::bytes_type multicast_site_local{
    0xff, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14};

TEST_CASE("[default ctor should return unspecified address]", "[address_v6.ctor]") {
  address_v6 unspecified_addr{};
  CHECK(unspecified_addr.to_bytes() == unspecified);
  CHECK(unspecified_addr.to_string() == "::");
  CHECK(unspecified_addr.is_unspecified());
}

TEST_CASE("[construct an address from bytes of different network type]", "[address_v6.ctor]") {
  address_v6 unspecified_addr{unspecified};
  CHECK(unspecified_addr.to_bytes() == unspecified);
  CHECK(unspecified_addr.to_string() == "::");
  CHECK(unspecified_addr.is_unspecified());

  address_v6 loopback_addr{loopback};
  CHECK(loopback_addr.to_bytes() == loopback);
  CHECK(loopback_addr.to_string() == "::1");
  CHECK(loopback_addr.is_loopback());

  address_v6 link_local_addr{link_local};
  CHECK(link_local_addr.to_bytes() == link_local);
  CHECK(link_local_addr.to_string() == "fe80::1112:1314");
  CHECK(link_local_addr.is_link_local());

  address_v6 site_local_addr{site_local};
  CHECK(site_local_addr.to_bytes() == site_local);
  CHECK(site_local_addr.to_string() == "fec0::1112:1314");
  CHECK(site_local_addr.is_site_local());

  address_v6 v4_mapped_addr{v4_mapped};
  CHECK(v4_mapped_addr.to_bytes() == v4_mapped);
  CHECK(v4_mapped_addr.to_string() == "::ffff:0.0.254.255");
  CHECK(v4_mapped_addr.is_v4_mapped());

  address_v6 multicast_addr{multicast};
  CHECK(multicast_addr.to_bytes() == multicast);
  CHECK(multicast_addr.to_string() == "ff00::910:1112:1314");
  CHECK(multicast_addr.is_multicast());

  address_v6 multicast_global_addr{multicast_global};
  CHECK(multicast_global_addr.to_bytes() == multicast_global);
  CHECK(multicast_global_addr.to_string() == "ff0e::910:1112:1314");
  CHECK(multicast_global_addr.is_multicast_global());

  address_v6 multicast_local_addr{multicast_link_local};
  CHECK(multicast_local_addr.to_bytes() == multicast_link_local);
  CHECK(multicast_local_addr.to_string() == "ff02::910:1112:1314");
  CHECK(multicast_local_addr.is_multicast_link_local());

  address_v6 multicast_node_local_addr{multicast_node_local};
  CHECK(multicast_node_local_addr.to_bytes() == multicast_node_local);
  CHECK(multicast_node_local_addr.to_string() == "ff01::910:1112:1314");
  CHECK(multicast_node_local_addr.is_multicast_node_local());

  address_v6 multicast_org_local_addr{multicast_org_local};
  CHECK(multicast_org_local_addr.to_bytes() == multicast_org_local);
  CHECK(multicast_org_local_addr.to_string() == "ff08::910:1112:1314");
  CHECK(multicast_org_local_addr.is_multicast_org_local());

  address_v6 multicast_site_local_addr{multicast_site_local};
  CHECK(multicast_site_local_addr.to_bytes() == multicast_site_local);
  CHECK(multicast_site_local_addr.to_string() == "ff05::910:1112:1314");
  CHECK(multicast_site_local_addr.is_multicast_site_local());
}

TEST_CASE("[copy constructor]", "[address_v6.ctor]") {
  address_v6 addr6{multicast};
  address_v6 addr6_copy{addr6};
  CHECK(addr6_copy.to_bytes() == multicast);
  CHECK(addr6_copy.to_string() == "ff00::910:1112:1314");
  CHECK(addr6 == addr6_copy);
}

TEST_CASE("[move constructor]", "[address_v6.ctor]") {
  address_v6 addr6{multicast};
  address_v6 addr6_copy{std::move(addr6)};
  CHECK(addr6_copy.to_bytes() == multicast);
  CHECK(addr6_copy.to_string() == "ff00::910:1112:1314");
}

TEST_CASE("[operation <=> should work]", "[address_v6.comparision]") {
  CHECK(address_v6{multicast} == address_v6{multicast});
  CHECK(address_v6{loopback} < address_v6{multicast});
  CHECK(address_v6{loopback} > address_v6{unspecified});
}

TEST_CASE("[native_address should return sockaddr_storage]", "[address_v6.native_address]") {
  address_v6 normal{multicast};
  ::sockaddr_storage storage;
  socklen_t size = normal.native_address(&storage, 80);
  CHECK(size == sizeof(::sockaddr_in6));
  auto addr = *reinterpret_cast<::sockaddr_in6 *>(&storage);
  CHECK(addr.sin6_family == AF_INET6);
  CHECK(addr.sin6_port == htons(80));
  CHECK(std::bit_cast<address_v6::bytes_type>(addr.sin6_addr.s6_addr) == multicast);
}

TEST_CASE("[pass well-formed address to make_address_v6 should work]",
          "[address_v6.make_address_v6]") {
  CHECK(make_address_v6("::ffff:127.0.0.1").is_v4_mapped());
  CHECK(make_address_v6("::ffff:127.0.0.1").to_string() == "::ffff:127.0.0.1");

  CHECK(make_address_v6(std::string_view{"ff0e::910:1112:1314"}).is_multicast_global());
  CHECK(make_address_v6(std::string_view{"ff0e::910:1112:1314"}).to_string()
        == "ff0e::910:1112:1314");

  CHECK(make_address_v6(std::string{"fec0::1112:1314"}).is_site_local());
  CHECK(make_address_v6(std::string{"fec0::1112:1314"}).to_string() == "fec0::1112:1314");

  CHECK(make_address_v6(v4_mapped_t::v4_mapped, make_address_v4("127.0.0.1")).is_v4_mapped());
  CHECK(make_address_v6(v4_mapped_t::v4_mapped, make_address_v4("127.0.0.1")).to_string()
        == "::ffff:127.0.0.1");
}

TEST_CASE("[make ip v4 mapped address should work]", "[address_v6.make_address_v4]") {
  address_v6 v4_mapped_addr{v4_mapped};
  CHECK(v4_mapped_addr.to_bytes() == v4_mapped);
  CHECK(v4_mapped_addr.to_string() == "::ffff:0.0.254.255");
  CHECK(v4_mapped_addr.is_v4_mapped());
  address_v4 v4 = make_address_v4(v4_mapped_t::v4_mapped, v4_mapped_addr);
  CHECK(v4.to_string() == "0.0.254.255");
}

TEST_CASE("[make ip v4 mapped address use ill-formed address should return unspecified]",
          "[address_v6.make_address_v6]") {
  address_v6 not_v4_mapped_addr = make_address_v6("fec0::1112:1314");
  CHECK(!not_v4_mapped_addr.is_v4_mapped());
  address_v4 v4 = make_address_v4(v4_mapped_t::v4_mapped, not_v4_mapped_addr);
  CHECK(v4.is_unspecified());
}

TEST_CASE("[pass network interface name to make_address_v6 should correctly set scope id]",
          "[address_v6.make_address_v6]") {
  constexpr auto get_all_if_names = []() -> std::vector<std::string> {
    std::vector<std::string> result;
    struct if_nameindex *idx = nullptr;
    struct if_nameindex *if_idx_start = if_nameindex();
    if (if_idx_start != NULL) {
      for (idx = if_idx_start; idx->if_index != 0 || idx->if_name != NULL; idx++) {
        result.emplace_back(idx->if_name);
      }
      if_freenameindex(if_idx_start);
    }
    return result;
  };

  // link local
  for (const auto &if_name : get_all_if_names()) {
    address_v6 v6 = make_address_v6(fmt::format("fe80::1112:1314%{}", if_name));
    CHECK(v6.is_link_local());
    CHECK(v6.scope_id() == if_nametoindex(if_name.data()));
  }

  // multicast link local
  for (const auto &if_name : get_all_if_names()) {
    address_v6 v6 = make_address_v6(fmt::format("ff02::910:1112:1314%{}", if_name));
    CHECK(v6.is_multicast_link_local());
    CHECK(v6.scope_id() == if_nametoindex(if_name.data()));
  }

  // others
  for (const auto &if_name : get_all_if_names()) {
    address_v6 v6 = make_address_v6(fmt::format("ff00::910:1112:1314%{}", if_name));
    CHECK(v6.is_multicast());
    CHECK(v6.scope_id() == atoi(if_name.data()));
  }
}

TEST_CASE("[scoped_id should be set to 0 if ill-formed if_name is passed to make_address_v6]",
          "[address_v6.make_address_v6]") {
  // empty
  CHECK(make_address_v6(fmt::format("ff00::910:1112:1314%")).is_multicast());
  CHECK(make_address_v6(fmt::format("ff00::910:1112:1314%")).scope_id() == 0);

  // invalid network interface name
  CHECK(make_address_v6(fmt::format("ff00::910:1112:1314%||")).is_multicast());
  CHECK(make_address_v6(fmt::format("ff00::910:1112:1314%||")).scope_id() == 0);
}

TEST_CASE("get ip v4 address if ip v6 address is v4-mapped", "address_v6.to_v4") {
  address_v6 v4_mapped_addr{v4_mapped};
  CHECK(v4_mapped_addr.to_string() == "::ffff:0.0.254.255");
  CHECK(v4_mapped_addr.is_v4_mapped());
  CHECK(v4_mapped_addr.to_v4().to_string() == "0.0.254.255");
}

TEST_CASE("get unspecified ip v4 address if v6 is not v4-mapped", "address_v6.to_v4") {
  address_v6 non_v4_mapped_addr{multicast};
  CHECK(!non_v4_mapped_addr.is_v4_mapped());
  CHECK(non_v4_mapped_addr.to_v4().is_unspecified());
}

TEST_CASE("[pass ill-formed address to make_address_v6 should return unspecified]",
          "[address_v6.make_address_v6]") {
  CHECK(make_address_v6("xx:xx").is_unspecified());
  CHECK(make_address_v4("").is_unspecified());
}

TEST_CASE("hash address_v6 should work", "address_v6.hash") {
  std::unordered_map<address_v6, bool> addresses;
  addresses[make_address_v6(multicast)] = false;
  addresses[make_address_v6(multicast_global)] = false;
  CHECK(addresses[make_address_v6(multicast)] == false);
  CHECK(addresses[make_address_v6(multicast_global)] == false);

  addresses[make_address_v6(multicast)] = true;
  addresses[make_address_v6(multicast_global)] = true;
  CHECK(addresses[make_address_v6(multicast)] == true);
  CHECK(addresses[make_address_v6(multicast_global)] == true);
}
