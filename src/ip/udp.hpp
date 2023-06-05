#ifndef NET_IP_UDP_H_
#define NET_IP_UDP_H_

#include "basic_datagram_socket.hpp"
#include "ip/basic_endpoint.hpp"
#include "ip/basic_resolver.h"
#include "ip/basic_resolver_iterator.h"
#include "socket_option.hpp"

namespace net {
namespace ip {

// Encapsulates the flags needed for udp.
class udp {
 public:
  // The type of a udp endpoint.
  using endpoint = basic_endpoint<udp>;

  // The udp socket type.
  using socket = basic_datagram_socket<udp>;

  // TODO: 实现上这个
  // The udp resolver type.
  // using resolver = basic_resolver<udp>;

  // Construct with a specific family.
  constexpr explicit udp(int protocol_family) noexcept : family_(protocol_family) {}

  // Construct to represent the IPv4 udp protocol.
  constexpr static udp v4() noexcept { return udp(AF_INET); }

  // Construct to represent the IPv6 udp protocol.
  constexpr static udp v6() noexcept { return udp(AF_INET6); }

  // Obtain an identifier for the type of the protocol.
  constexpr int type() const noexcept { return SOCK_DGRAM; }

  // Obtain an identifier for the protocol.
  constexpr int protocol() const noexcept { return IPPROTO_UDP; }

  // Obtain an identifier for the protocol family.
  constexpr int family() const noexcept { return family_; }

  // Compare two protocols for equality.
  constexpr friend bool operator==(const udp& p1, const udp& p2) {
    return p1.family_ == p2.family_;
  }

  // Compare two protocols for inequality.
  constexpr friend bool operator!=(const udp& p1, const udp& p2) {
    return p1.family_ != p2.family_;
  }

 private:
  int family_;
};

}  // namespace ip
}  // namespace net

#endif  // NET_IP_UDP_H_
