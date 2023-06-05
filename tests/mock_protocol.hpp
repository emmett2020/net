#ifndef NET_TESTS_MOCK_PROTOCOL_HPP_
#define NET_TESTS_MOCK_PROTOCOL_HPP_

#include "basic_socket.hpp"
#include "ip/basic_endpoint.hpp"

using namespace net;
using namespace net::ip;

struct mock_protocol {
  using endpoint = basic_endpoint<mock_protocol>;
  using socket = basic_socket<mock_protocol>;

  static constexpr mock_protocol v6() { return mock_protocol{AF_INET6}; }
  static constexpr mock_protocol v4() { return mock_protocol{AF_INET}; }

  static constexpr mock_protocol v6_udp() {
    return mock_protocol{AF_INET6, SOCK_DGRAM, IPPROTO_IP};
  }
  static constexpr mock_protocol v4_udp() { return mock_protocol{AF_INET, SOCK_DGRAM, IPPROTO_IP}; }

  constexpr mock_protocol(int family) : family_(family) {}
  constexpr mock_protocol(int family, int type, int protocol)
      : family_(family), type_(type), protocol_(protocol) {}
  constexpr mock_protocol() = delete;

  constexpr int family() const { return family_; }
  constexpr int type() const { return type_; }
  constexpr int protocol() const { return protocol_; }

  // Compare two endpoints for equality.
  constexpr bool operator==(const mock_protocol&) const noexcept = default;

  // Compare endpoints for ordering.
  constexpr auto operator<=>(const mock_protocol&) const noexcept = default;

  int family_;
  int type_ = SOCK_STREAM;
  int protocol_ = IPPROTO_IP;
};

#endif  // NET_TESTS_MOCK_PROTOCOL_HPP_