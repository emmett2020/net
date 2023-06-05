#ifndef SRC_NET_IP_BASIC_ENDPOINT_H_
#define SRC_NET_IP_BASIC_ENDPOINT_H_

#include <bit>

#include "address.hpp"
#include "address_v4.hpp"
#include "address_v6.hpp"
#include "utils.hpp"

namespace net {
namespace ip {

// TODO: concept protocol
// Describes an endpoint for a version-independent IP socket.
template <typename InternetProtocol>
class basic_endpoint {
 public:
  // Associated internet protocol.
  using protocol_type = InternetProtocol;

  // Default constructor.
  constexpr basic_endpoint() noexcept : protocol_{protocol_type::v6()}, address_{}, port_{0} {}

  // Construct an endpoint using a port number, specified in the host's byte order.
  // The IP address will be the any address (INADDR_ANY or in6addr_any) which IP version associated
  // with given protocol. This constructor would typically be used for accepting new connections.
  constexpr basic_endpoint(const protocol_type& internet_protocol, port_type port) noexcept
      : protocol_(internet_protocol), address_(), port_(port) {
    if (internet_protocol.family() == AF_INET) {
      address_ = address_v4{};
    }
  }

  // Construct an endpoint using a port number and an IP address.
  // This constructor may be used for accepting connections on a specific interface
  // or for making a connection to a remote endpoint.
  constexpr basic_endpoint(const address& addr, port_type port) noexcept
      : protocol_(addr.is_v4() ? protocol_type::v4() : protocol_type::v6()),
        address_(addr),
        port_(port) {}

  // Set the IPv4 address using native type. This will also set protocol and port.
  // Note the port and address of native_addr is always net work order.
  constexpr basic_endpoint(const ::sockaddr_in& native_addr) : protocol_(protocol_type::v4()) {
    if constexpr (std::endian::native == std::endian::little) {
      port_ = utils::byteswap(native_addr.sin_port);
      address_ = address_v4(utils::byteswap(native_addr.sin_addr.s_addr));
    } else {
      port_ = native_addr.sin_port;
      address_ = address_v4(native_addr.sin_addr.s_addr);
    };
  }

  // Set the IPv6 address using native type. This will also set protocol and port.
  constexpr basic_endpoint(const ::sockaddr_in6& native_addr)
      : protocol_(protocol_type::v6()),
        address_(address_v6(std::bit_cast<address_v6::bytes_type>(native_addr.sin6_addr.s6_addr),
                            native_addr.sin6_scope_id)) {
    if constexpr (std::endian::native == std::endian::little) {
      port_ = utils::byteswap(native_addr.sin6_port);
    } else {
      port_ = native_addr.sin6_port;
    };
  }

  // Copy constructor.
  constexpr basic_endpoint(const basic_endpoint& other) noexcept = default;

  // Move constructor.
  constexpr basic_endpoint(basic_endpoint&& other) noexcept = default;

  // Assign from another endpoint.
  constexpr basic_endpoint& operator=(const basic_endpoint& other) noexcept = default;

  // Move-assign from another endpoint.
  constexpr basic_endpoint& operator=(basic_endpoint&& other) noexcept = default;

  // The protocol associated with the endpoint.
  constexpr protocol_type protocol() const noexcept { return protocol_; }

  // Get the port associated with the endpoint. The port is always in the host's byte order.
  constexpr port_type port() const noexcept { return port_; }

  // Set the port associated with the endpoint. The port is always in the host's byte order.
  constexpr void set_port(port_type port) noexcept { port_ = port; }

  // Get the IP address associated with the endpoint.
  constexpr class address address() const noexcept { return address_; }

  // Set the IP address associated with the endpoint.
  constexpr void set_address(const class address& addr) noexcept { address_ = addr; }

  // Get the address in the native type.
  ::socklen_t native_address(::sockaddr_storage* storage) const {
    return address_.native_address(storage, port_);
  }

  // Compare two endpoints for equality.
  constexpr bool operator==(const basic_endpoint&) const noexcept = default;

  // Compare endpoints for ordering.
  constexpr auto operator<=>(const basic_endpoint&) const noexcept = default;

 private:
  // Protocol.
  protocol_type protocol_;

  // Version-independent address.
  class address address_;

  // Port number, specified in the host's byte order.
  port_type port_;
};

}  // namespace ip
}  // namespace net

namespace std {

template <typename InternetProtocol>
struct hash<net::ip::basic_endpoint<InternetProtocol>> {
  std::size_t operator()(const net::ip::basic_endpoint<InternetProtocol>& ep) const noexcept {
    std::size_t hash1 = std::hash<net::ip::address>()(ep.address());
    std::size_t hash2 = std::hash<unsigned short>()(ep.port());
    return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
  }
};

}  // namespace std

#endif  // SRC_NET_IP_BASIC_ENDPOINT_H_
