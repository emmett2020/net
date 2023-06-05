#ifndef NET_SRC_BASIC_SOCKET_ACCEPTOR_HPP_
#define NET_SRC_BASIC_SOCKET_ACCEPTOR_HPP_

#include <utility>

#include "basic_socket.hpp"
#include "execution_context.hpp"
#include "ip/socket_types.hpp"
#include "socket_base.hpp"
#include "status-code/system_code.hpp"

namespace net {

using system_error2::system_code;

// Provides the ability to accept new connections.
template <typename Protocol>
class basic_socket_acceptor : public basic_socket<Protocol> {
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

  // Construct an acceptor without opening it.
  constexpr explicit basic_socket_acceptor(context_type& ctx) noexcept
      : basic_socket<protocol_type>(ctx) {}

  // Move copy
  constexpr basic_socket_acceptor(basic_socket_acceptor&& other) noexcept = default;

  // Move-assign.
  constexpr basic_socket_acceptor& operator=(basic_socket_acceptor&& other) noexcept = default;

  // Construct an acceptor opened on the given endpoint.
  constexpr basic_socket_acceptor(context_type& ctx, const endpoint_type& endpoint, system_code& ec,
                                  bool reuse_addr = true) noexcept
      : basic_socket<Protocol>(ctx) {
    if (ec = basic_socket<Protocol>::open(endpoint.protocol()); ec.failure()) {
      return;
    }

    if (reuse_addr) {
      if (ec = basic_socket<Protocol>::set_option(socket_base::reuse_address{true}); ec.failure())
        return;
    }

    if (ec = basic_socket<Protocol>::bind(endpoint); ec.failure()) {
      return;
    }

    if (ec = basic_socket<Protocol>::listen(max_listen_connections); ec.failure()) {
      return;
    }
  }

  // Destroys the acceptor.
  constexpr ~basic_socket_acceptor() noexcept = default;

 private:
  // Disallow copying and assignment.
  constexpr basic_socket_acceptor(const basic_socket_acceptor&) = delete;
  constexpr basic_socket_acceptor& operator=(const basic_socket_acceptor&) = delete;
};

}  // namespace net

#endif  // NET_SRC_BASIC_SOCKET_ACCEPTOR_HPP_
