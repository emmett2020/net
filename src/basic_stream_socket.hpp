#ifndef NET_SRC_BASIC_STREAM_SOCKET_HPP_
#define NET_SRC_BASIC_STREAM_SOCKET_HPP_

#include "basic_socket.hpp"

namespace net {
using system_error2::system_code;

// Provides stream-oriented socket functionality.
template <typename Protocol>
class basic_stream_socket : public basic_socket<Protocol> {
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

  // Construct a basic_stream_socket without opening it.
  constexpr basic_stream_socket(context_type& ctx) noexcept : basic_socket<protocol_type>(ctx) {}

  // Constructor with native socket and specific protocol.
  constexpr basic_stream_socket(context_type& ctx, const protocol_type& protocol,
                                native_handle_type fd) noexcept
      : basic_socket<Protocol>(ctx, protocol, fd) {}

  // Move-construct a basic_stream_socket from another.
  constexpr basic_stream_socket(basic_stream_socket&& other) noexcept = default;

  // Move-assign.
  constexpr basic_stream_socket& operator=(basic_stream_socket&& other) noexcept = default;

  // Destroys the socket.
  constexpr ~basic_stream_socket() noexcept = default;

 private:
  // Disallow copying and assignment.
  basic_stream_socket(const basic_stream_socket&) = delete;
  basic_stream_socket& operator=(const basic_stream_socket&) = delete;
};

}  // namespace net

#endif  // NET_SRC_BASIC_STREAM_SOCKET_HPP_
