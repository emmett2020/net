#ifndef NET_SRC_IP_TCP_HPP_
#define NET_SRC_IP_TCP_HPP_

#include "basic_socket_acceptor.hpp"
#include "basic_stream_socket.hpp"
#include "ip/basic_endpoint.hpp"
#include "ip/basic_resolver_iterator.h"
#include "ip/basic_resolver_query.h"
#include "socket_option.hpp"

namespace net {
namespace ip {
// Encapsulates the flags needed for tcp.
class tcp {
 public:
  // The type of a tcp endpoint.
  using endpoint = basic_endpoint<tcp>;

  // The tcp socket type.
  using socket = basic_stream_socket<tcp>;

  // The tcp acceptor type.
  using acceptor = basic_socket_acceptor<tcp>;

  // The tcp resolver type.
  // TODO: 实现上这个
  // using resolver = BasicResolver<tcp>;

  // Construct with a specific family.
  constexpr explicit tcp(int protocol_family) noexcept : family_(protocol_family) {}

  // Construct to represent the IPv4 tcp protocol.
  constexpr static tcp v4() noexcept { return tcp(AF_INET); }

  // Construct to represent the IPv6 tcp protocol.
  constexpr static tcp v6() noexcept { return tcp(AF_INET6); }

  // Obtain an identifier for the type of the protocol.
  constexpr int type() const noexcept { return SOCK_STREAM; }

  // Obtain an identifier for the protocol.
  constexpr int protocol() const noexcept { return IPPROTO_TCP; }

  // Obtain an identifier for the protocol family.
  constexpr int family() const noexcept { return family_; }

  // Compare two protocols for equality.
  constexpr friend bool operator==(const tcp& p1, const tcp& p2) {
    return p1.family_ == p2.family_;
  }

  // Compare two protocols for inequality.
  constexpr friend bool operator!=(const tcp& p1, const tcp& p2) {
    return p1.family_ != p2.family_;
  }

 private:
  int family_;
};

}  // namespace ip
}  // namespace net

#endif  // NET_SRC_IP_TCP_HPP_
