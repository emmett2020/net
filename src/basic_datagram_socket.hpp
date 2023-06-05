#ifndef NET_SRC_BASIC_DATAGRAM_SOCKET_HPP_
#define NET_SRC_BASIC_DATAGRAM_SOCKET_HPP_

#include <cstddef>

#include "basic_socket.hpp"
#include "status-code/system_code.hpp"

namespace net {

using system_error2::system_code;

// Provides datagram-oriented socket functionality.
template <typename Protocol>
class basic_datagram_socket : public basic_socket<Protocol> {
 public:
  // The native handle type.
  using native_handle_type = socket_base::native_handle_type;

  // The protocol type.
  using protocol_type = Protocol;

  // The endpoint type.
  using endpoint_type = typename protocol_type::endpoint;

  // The socket type.
  using socket_type = typename protocol_type::socket;

  // The context_type
  using context_type = typename basic_socket<protocol_type>::context_type;

  // Construct a basic_datagram_socket without opening it.
  constexpr basic_datagram_socket(context_type& ctx) noexcept : basic_socket<protocol_type>(ctx) {}

  // This constructor creates and opens a datagram socket.
  constexpr basic_datagram_socket(context_type& ctx, const Protocol& protocol,
                                  system_code& ec) noexcept
      : basic_socket<Protocol>(ctx, protocol, ec) {}

  // Constructor with native socket and specific protocol.
  constexpr basic_datagram_socket(context_type& ctx, const protocol_type& protocol,
                                  native_handle_type fd) noexcept
      : basic_socket<Protocol>(ctx, protocol, fd) {}

  // Construct a basic_datagram_socket, opening it and binding it to the given
  // local endpoint.
  constexpr basic_datagram_socket(context_type& ctx, const endpoint_type& endpoint) noexcept
      : basic_socket<Protocol>(ctx, endpoint) {}

  // Move-construct a basic_datagram_socket from another.
  constexpr basic_datagram_socket(basic_datagram_socket&& other) noexcept = default;

  // Move-assign a basic_datagram_socket from another.
  constexpr basic_datagram_socket& operator=(basic_datagram_socket&& other) noexcept = default;

  // Destroys the socket.
  constexpr ~basic_datagram_socket() noexcept = default;

 private:
  // Disallow copying and assignment.
  constexpr basic_datagram_socket(const basic_datagram_socket&) = delete;
  constexpr basic_datagram_socket& operator=(const basic_datagram_socket&) = delete;
};
}  // namespace net

#endif  // NET_SRC_BASIC_DATAGRAM_SOCKET_HPP_
