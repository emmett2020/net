#ifndef NET_SRC_IP_ADDRESS_V6_HPP_
#define NET_SRC_IP_ADDRESS_V6_HPP_

#include <arpa/inet.h>
#include <fmt/format.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <string_view>

#include "../utils.hpp"
#include "address_v4.hpp"
#include "socket_types.hpp"

namespace net {
namespace ip {
// Implements IP version 6 style addresses.
class address_v6 {
 public:
  // The array type corresponding to the IP address.
  struct bytes_type : std::array<unsigned char, 16> {
    template <typename... T>
    explicit constexpr bytes_type(T... t)
        : std::array<unsigned char, 16>{{static_cast<unsigned char>(t)...}} {}
  };

  // Default constructor.
  constexpr address_v6() noexcept : bytes_(0), scope_id_(0) {}

  // Construct an address from raw bytes and scope ID.
  explicit constexpr address_v6(const bytes_type& bytes, scope_id_type scope = 0)
      : bytes_(bytes), scope_id_(scope) {}

  // Copy constructor.
  constexpr address_v6(const address_v6& other) noexcept = default;

  // Move constructor.
  constexpr address_v6(address_v6&& other) noexcept = default;

  // Assign from another address.
  constexpr address_v6& operator=(const address_v6& other) noexcept = default;

  // Move-assign from another address.
  constexpr address_v6& operator=(address_v6&& other) noexcept = default;

  constexpr bool operator==(address_v6 const&) const noexcept = default;

  // Compares two addresses in host byte order.
  constexpr std::strong_ordering operator<=>(address_v6 const& other) const {
    return bytes_ == other.bytes_  ? ::std::strong_ordering::equal
           : bytes_ < other.bytes_ ? ::std::strong_ordering::less
                                   : ::std::strong_ordering::greater;
  }

  // The scope ID of the address.
  constexpr scope_id_type scope_id() const noexcept { return scope_id_; }

  // Modifies the scope ID associated with the IPv6 address.
  constexpr void scope_id(scope_id_type id) noexcept { scope_id_ = id; }

  // Get the address in bytes, in network byte order.
  constexpr bytes_type to_bytes() const noexcept { return bytes_; }

  constexpr address_v4 to_v4() const noexcept {
    if (!is_v4_mapped() && !is_v4_compatible()) {
      return {};
    }
    return address_v4(address_v4::bytes_type{bytes_[12], bytes_[13], bytes_[14], bytes_[15]});
  }

  // Get the address as a string.
  std::string to_string() const noexcept {
    char addr_str[max_address_v6_string_len];
    ::in6_addr src = std::bit_cast<in6_addr>(bytes_);
    const char* addr = ::inet_ntop(AF_INET6, &src, addr_str, max_address_v6_string_len);
    return addr ? addr : std::string{};
  }

  // Determine whether the address is a loopback address.
  constexpr bool is_loopback() const noexcept {
    return ((bytes_[0] == 0) && (bytes_[1] == 0) && (bytes_[2] == 0) && (bytes_[3] == 0)
            && (bytes_[4] == 0) && (bytes_[5] == 0) && (bytes_[6] == 0) && (bytes_[7] == 0)
            && (bytes_[8] == 0) && (bytes_[9] == 0) && (bytes_[10] == 0) && (bytes_[11] == 0)
            && (bytes_[12] == 0) && (bytes_[13] == 0) && (bytes_[14] == 0) && (bytes_[15] == 1));
  }

  // Determine whether the address is unspecified.
  constexpr bool is_unspecified() const noexcept {
    return ((bytes_[0] == 0) && (bytes_[1] == 0) && (bytes_[2] == 0) && (bytes_[3] == 0)
            && (bytes_[4] == 0) && (bytes_[5] == 0) && (bytes_[6] == 0) && (bytes_[7] == 0)
            && (bytes_[8] == 0) && (bytes_[9] == 0) && (bytes_[10] == 0) && (bytes_[11] == 0)
            && (bytes_[12] == 0) && (bytes_[13] == 0) && (bytes_[14] == 0) && (bytes_[15] == 0));
  }

  // Determine whether the address is link local.
  constexpr bool is_link_local() const noexcept {
    return ((bytes_[0] == 0xfe) && ((bytes_[1] & 0xc0) == 0x80));
  }

  // Determine whether the address is site local.
  constexpr bool is_site_local() const noexcept {
    return ((bytes_[0] == 0xfe) && ((bytes_[1] & 0xc0) == 0xc0));
  }

  // Determine whether the address is a mapped IPv4 address.
  constexpr bool is_v4_mapped() const noexcept {
    return ((bytes_[0] == 0) && (bytes_[1] == 0) && (bytes_[2] == 0) && (bytes_[3] == 0)
            && (bytes_[4] == 0) && (bytes_[5] == 0) && (bytes_[6] == 0) && (bytes_[7] == 0)
            && (bytes_[8] == 0) && (bytes_[9] == 0) && (bytes_[10] == 0xff)
            && (bytes_[11] == 0xff));
  }

  constexpr bool is_v4_compatible() const noexcept {
    return ((bytes_[0] == 0) && (bytes_[1] == 0) && (bytes_[2] == 0) && (bytes_[3] == 0)
            && (bytes_[4] == 0) && (bytes_[5] == 0) && (bytes_[6] == 0) && (bytes_[7] == 0)
            && (bytes_[8] == 0) && (bytes_[9] == 0) && (bytes_[10] == 0) && (bytes_[11] == 0)
            && !((bytes_[12] == 0) && (bytes_[13] == 0) && (bytes_[14] == 0)
                 && ((bytes_[15] == 0) || (bytes_[15] == 1))));
  }

  // Determine whether the address is a multicast address.
  constexpr bool is_multicast() const noexcept { return (bytes_[0] == 0xff); }

  // Determine whether the address is a global multicast address.
  constexpr bool is_multicast_global() const noexcept {
    return ((bytes_[0] == 0xff) && ((bytes_[1] & 0x0f) == 0x0e));
  }

  // Determine whether the address is a link-local multicast address.
  constexpr bool is_multicast_link_local() const noexcept {
    return ((bytes_[0] == 0xff) && ((bytes_[1] & 0x0f) == 0x02));
  }

  // Determine whether the address is a node-local multicast address.
  constexpr bool is_multicast_node_local() const noexcept {
    return ((bytes_[0] == 0xff) && ((bytes_[1] & 0x0f) == 0x01));
  }

  // Determine whether the address is a org-local multicast address.
  constexpr bool is_multicast_org_local() const noexcept {
    return ((bytes_[0] == 0xff) && ((bytes_[1] & 0x0f) == 0x08));
  }

  // Determine whether the address is a site-local multicast address.
  constexpr bool is_multicast_site_local() const noexcept {
    return ((bytes_[0] == 0xff) && ((bytes_[1] & 0x0f) == 0x05));
  }

  // Obtain an address object that represents any address.
  static constexpr address_v6 any() noexcept { return address_v6{}; }

  // Obtain an address object that represents the loopback address.
  static constexpr address_v6 loopback() noexcept {
    address_v6 tmp;
    tmp.bytes_[15] = 1;
    return tmp;
  }

  // Obtain associated address.
  socklen_t native_address(::sockaddr_storage* storage, port_type port) const {
    ::sockaddr_in6 addr{.sin6_family = AF_INET6};
    if constexpr (std::endian::native == std::endian::big) {
      addr.sin6_port = port;
    } else {
      addr.sin6_port = utils::byteswap(port);
    }
    std::memcpy(&addr.sin6_addr, &bytes_[0], bytes_.size());
    std::memcpy(storage, &addr, sizeof addr);
    return sizeof addr;
  }

 private:
  // The underlying IPv6 address bytes in network order.
  bytes_type bytes_;

  // The scope ID associated with the address.
  scope_id_type scope_id_;
};

// Create an IPv6 address from raw bytes and scope ID.
inline constexpr address_v6 make_address_v6(const address_v6::bytes_type& bytes,
                                            uint32_t scope_id = 0) noexcept {
  return address_v6(bytes, scope_id);
}

// Create an IPv6 address from an IP address string.
inline address_v6 make_address_v6(const char* str) noexcept {
  const char* if_name = ::strchr(str, '%');
  const char* p = str;
  char addr_buf[max_address_v6_string_len]{};

  if (if_name) {
    // Too long, just return unspecified.
    if (if_name - str > max_address_v6_string_len) {
      return address_v6{};
    }

    // Split.
    ::memcpy(addr_buf, str, if_name - str);
    addr_buf[if_name - str] = '\0';
    p = addr_buf;
  }

  ::in6_addr addr;
  if (::inet_pton(AF_INET6, p, &addr) > 0) {
    scope_id_type scope_id = 0;

    // Get scope id by network interface name.
    if (if_name) {
      bool is_link_local = addr.s6_addr[0] == 0xfe && ((addr.s6_addr[1] & 0xc0) == 0x80);
      bool is_multicast_link_local = addr.s6_addr[0] == 0xff && addr.s6_addr[1] == 0x02;
      if (is_link_local || is_multicast_link_local) {
        scope_id = if_nametoindex(if_name + 1);
      }
      if (scope_id == 0) {
        scope_id = atoi(if_name + 1);
      }
    }
    return address_v6{std::bit_cast<address_v6::bytes_type>(addr), scope_id};
  }
  return address_v6{};
}

// Create IPv6 address from an IP address string.
inline address_v6 make_address_v6(const std::string& str) noexcept {
  return make_address_v6(str.c_str());
}

// Create an IPv6 address from an IP address string.
inline address_v6 make_address_v6(std::string_view str) noexcept {
  return make_address_v6(static_cast<std::string>(str));
}

// Tag type used for distinguishing overloads that deal in IPv4-mapped IPv6 addresses.
enum class v4_mapped_t { v4_mapped };

// Create an IPv4 address from a IPv4-mapped IPv6 address.
inline constexpr address_v4 make_address_v4(v4_mapped_t, const address_v6& v6_addr) {
  if (!v6_addr.is_v4_mapped()) {
    return {};
  }
  address_v6::bytes_type v6_bytes = v6_addr.to_bytes();
  return address_v4{address_v4::bytes_type{v6_bytes[12], v6_bytes[13], v6_bytes[14], v6_bytes[15]}};
}

// Create an IPv4-mapped IPv6 address from an IPv4 address.
inline address_v6 make_address_v6(v4_mapped_t, const address_v4& v4_addr) {
  address_v4::bytes_type v4_bytes = v4_addr.to_bytes();
  address_v6::bytes_type v6_bytes{
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, v4_bytes[0], v4_bytes[1], v4_bytes[2], v4_bytes[3]};
  return address_v6{v6_bytes};
}

}  // namespace ip
}  // namespace net

template <>
struct std::hash<net::ip::address_v6> {
  std::size_t operator()(const net::ip::address_v6& addr) const noexcept {
    const net::ip::address_v6::bytes_type bytes = addr.to_bytes();
    std::size_t result = static_cast<std::size_t>(addr.scope_id());
    combine_4_bytes(result, &bytes[0]);
    combine_4_bytes(result, &bytes[4]);
    combine_4_bytes(result, &bytes[8]);
    combine_4_bytes(result, &bytes[12]);
    return result;
  }

 private:
  static void combine_4_bytes(std::size_t& seed, const unsigned char* bytes) {
    const std::size_t bytes_hash =
        (static_cast<std::size_t>(bytes[0]) << 24) | (static_cast<std::size_t>(bytes[1]) << 16)
        | (static_cast<std::size_t>(bytes[2]) << 8) | (static_cast<std::size_t>(bytes[3]));
    seed ^= bytes_hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

#endif  // NET_SRC_IP_ADDRESS_V6_HPP_
