#ifndef NET_SRC_BASIC_SOCKET_HPP_
#define NET_SRC_BASIC_SOCKET_HPP_

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <exec/linux/safe_file_descriptor.hpp>
#include <optional>
#include <status-code/generic_code.hpp>
#include <status-code/result.hpp>
#include <status-code/system_code.hpp>

#include "buffer_sequence_adapter.hpp"
#include "execution_context.hpp"
#include "io_control.hpp"
#include "ip/socket_types.hpp"
#include "net_error.hpp"
#include "socket_base.hpp"
#include "socket_option.hpp"

namespace net {
// Code   : https://github.com/ned14/status-code
// Paper  : https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1028r4.pdf
// Example: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1883r2.pdf
using system_error2::errc;
using system_error2::result;
using system_error2::system_code;

// basic_socket is the base class for all socket types. It defines various socket-related functions.
// We don't force a context on basic_socket (as `Asio` does) because basic_socket defines generic
// socket operations that are independent of their context.
template <typename Protocol>
class basic_socket : public socket_base {
 public:
  // The native handle type.
  using native_handle_type = socket_base::native_handle_type;

  // The protocol type.
  using protocol_type = Protocol;

  // The endpoint type.
  using endpoint_type = typename protocol_type::endpoint;

  // The socket type.
  using socket_type = typename protocol_type::socket;

  // The current state of the socket.
  using socket_state = unsigned char;

  // The context type.
  using context_type = execution_context;

  // Default constructor.
  constexpr basic_socket(context_type &ctx) noexcept  // NOLINT
      : state_(0), protocol_(), descriptor_(), context_(ctx) {}

  // Constructor with specific protocol and create a new descriptor.
  constexpr basic_socket(context_type &ctx, const protocol_type &protocol,
                         system_code &code) noexcept
      : context_(ctx) {
    code = basic_socket::open(protocol);
  }

  // Constructor with native socket and specific protocol.
  constexpr basic_socket(context_type &ctx, const protocol_type &protocol,  // NOLINT
                         native_handle_type fd) noexcept
      : context_(ctx) {
    assign(protocol, fd);
  }

  // Move constructor. The file descriptor of the moved socket will be set to invalid.
  constexpr basic_socket(basic_socket &&other) noexcept
      : state_(std::move(other.state_)),            //
        protocol_(std::move(other.protocol_)),      //
        descriptor_(std::move(other.descriptor_)),  //
        context_(other.context_) {}

  // Move assign.
  constexpr basic_socket &operator=(basic_socket &&other) noexcept {
    state_ = std::move(other.state_);
    protocol_ = std::move(other.protocol_);
    descriptor_ = std::move(other.descriptor_);
    context_ = other.context_;
    return *this;
  }

  // // Destructor which protected to prevent deletion through this type.
  constexpr ~basic_socket() = default;

  // Get associated context.
  constexpr context_type &context() noexcept { return context_; }

  // Get the native socket representation.
  constexpr native_handle_type native_handle() const noexcept {
    return descriptor_.native_handle();
  }

  // Get associated protocol.
  constexpr protocol_type protocol() const noexcept { return *protocol_; }

  // Get associated state.
  constexpr socket_state state() const noexcept { return state_; }

  // Set new state.
  constexpr void set_state(socket_state state) noexcept { state_ = state; }

  // Sets the non-blocking mode of the socket.
  constexpr system_code set_non_blocking(bool mode) noexcept;

  // Gets the non-blocking mode of the socket.
  constexpr bool is_non_blocking() const noexcept { return (state_ & non_blocking) != 0; }

  // Open a new file descriptor. Reassign protocol_ and state_ based on given protocol.
  constexpr system_code open(const protocol_type &protocol) noexcept;

  // Check whether socket is opened.
  constexpr bool is_open() const noexcept {
    return descriptor_.native_handle() != invalid_socket_fd;
  }

  // Assign a native file descriptor with specified protocol to this socket.
  constexpr void assign(const protocol_type &protocol, native_handle_type fd) noexcept;

  // Close this socket and reset descriptor. state and protocol are not reset.
  constexpr system_code close() noexcept;

  // Disable sends or receives on the socket. state and protocol are not reset.
  constexpr system_code shutdown(shutdown_type what) noexcept;

  // Bind socket to given endpoint.
  constexpr system_code bind(const endpoint_type &endpoint) const noexcept;

  // Listen. Place the socket into the state where it will listen for new connections.
  constexpr system_code listen(int backlog = max_listen_connections) const noexcept;

  // Poll read.
  constexpr system_code poll_read(int msec) const noexcept;

  // Poll write.
  constexpr system_code poll_write(int msec) const noexcept;

  // Poll connect.
  constexpr system_code poll_connect(int msec) const noexcept;

  // Poll error.
  constexpr system_code poll_error(int msec) const noexcept;

  // getaddrinfo
  constexpr system_code getaddrinfo(const char *host, const char *service, const ::addrinfo &hints,
                                    addrinfo **result) noexcept;

  // getnameinfo
  constexpr system_code getnameinfo(const void *addr, ::socklen_t addrlen, char *host,
                                    uint64_t hostlen, char *serv, uint64_t servlen,
                                    int flags) noexcept;

  // gethostname
  constexpr system_code gethostname(char *name, size_t namelen) noexcept;

  // Provides an endpoint, which is set when getsockname executes successfully.
  constexpr result<endpoint_type> getsockname() const noexcept;

  // Provides an endpoint, which is set when getpeername executes successfully.
  constexpr result<endpoint_type> getpeername() const noexcept;

  // Get the local endpoint.
  constexpr result<endpoint_type> local_endpoint() const noexcept;

  // Get the peer endpoint.
  constexpr result<endpoint_type> peer_endpoint() const noexcept;

  // Get option for this socket.
  constexpr system_code getsockopt(int level, int optname, void *optval,
                                   ::socklen_t *optlen) const noexcept;

  // Set socket options for fd of this socket.
  constexpr system_code setsockopt(int level, int optname, const void *optval,
                                   ::socklen_t optlen) noexcept;

  // Get option for this socket.
  template <typename Option>
  constexpr system_code get_option(Option &option) const noexcept {
    ::socklen_t size = static_cast<::socklen_t>(option.size(protocol_));
    return basic_socket::getsockopt(option.level(protocol_), option.name(protocol_),
                                    option.data(protocol_), &size);
  }

  // TODO: 这里用concept限制住Option
  template <typename Option>
  constexpr system_code set_option(const Option &option) noexcept {
    return basic_socket::setsockopt(option.level(protocol_), option.name(protocol_),
                                    option.data(protocol_), option.size(protocol_));
  }

  // Accept a new connection.
  constexpr result<socket_type> accept() const noexcept;

  // Accept a new connection. This function will block itself while waiting connection.
  constexpr result<socket_type> sync_accept() const noexcept;

  // Accept a new connection without blocking.
  constexpr result<socket_type> non_blocking_accept() const noexcept;

  // Connect the socket to the specified endpoint.
  constexpr system_code connect(const endpoint_type &peer_endpoint) const noexcept;

  // Connect to peer which may be blocked.
  constexpr system_code sync_connect(const endpoint_type &peer_endpoint) const noexcept;

  // Connect to peer without blocking.
  constexpr system_code non_blocking_connect(const endpoint_type &peer_endpoint) const noexcept;

  // Recvmsg.
  constexpr result<size_t> recvmsg(iovec *bufs, size_t count, int in_flags);

  // sync_recvmsg with blocking.
  constexpr result<size_t> sync_recvmsg(iovec *bufs, size_t count, int flags,
                                        bool all_empty = false);

  // Recvmsg without blocking.
  constexpr result<size_t> non_blocking_recvmsg(iovec *bufs, size_t count, int flags);

  // Recvmsg from
  constexpr result<size_t> recvmsg_from(iovec *bufs, size_t count, int flags, void *addr,
                                        int *addrlen);

  // sync_recvmsg_from with blocking.
  constexpr result<size_t> sync_recvmsg_from(iovec *bufs, size_t count, int flags, void *addr,
                                             int *addrlen);

  // Recvmsg without blocking.
  constexpr result<size_t> non_blocking_recvmsg_from(iovec *bufs, size_t count, int flags,
                                                     void *addr, int *addrlen);

  // Recv.
  constexpr result<size_t> recv(void *data, size_t size, int flags);

  // Recv with blocking.
  constexpr result<size_t> sync_recv(void *data, size_t size, int flags);

  // Recv without blocking.
  constexpr result<size_t> non_blocking_recv(void *data, size_t count, int flags);

  // Recvfrom.
  constexpr result<size_t> recvfrom(void *data, size_t size, int flags, void *addr,
                                    uint64_t *addrlen);

  // Sync recvfrom
  constexpr result<size_t> sync_recvfrom(void *data, size_t size, int flags, void *addr,
                                         uint64_t *addrlen);

  // non_blocking_recvfrom
  constexpr result<size_t> non_blocking_recvfrom(void *data, size_t size, int flags, void *addr,
                                                 uint64_t *addrlen);

  // sendmsg
  constexpr result<size_t> sendmsg(iovec *bufs, uint64_t count, int flags);

  // sync_sendmsg
  constexpr result<size_t> sync_sendmsg(iovec *bufs, uint64_t count, int flags,
                                        bool all_empty = false);

  // non_blocking_sendmsg
  constexpr result<size_t> non_blocking_sendmsg(iovec *bufs, uint64_t count, int flags);

  // sendmsg_to
  constexpr result<size_t> sendmsg_to(iovec *bufs, uint64_t count, int flags, const void *addr,
                                      ::socklen_t addrlen);

  // sync_sendto
  constexpr result<size_t> sync_sendmsg_to(iovec *bufs, uint64_t count, int flags, const void *addr,
                                           ::socklen_t addrlen);

  // non_blocking_sendto
  constexpr result<size_t> non_blocking_sendmsg_to(iovec *bufs, uint64_t count, int flags,
                                                   const void *addr, ::socklen_t addrlen);

  // send
  constexpr result<size_t> send(const void *data, size_t size, int flags);

  // sync_send
  constexpr result<size_t> sync_send(const void *data, size_t size, int flags);

  // non_blocking_send
  constexpr result<size_t> non_blocking_send(const void *data, size_t size, int flags);

  // sendto
  constexpr result<size_t> sendto(const void *data, size_t size, int flags, const void *addr,
                                  ::socklen_t addrlen);

  // sync_sendto
  constexpr result<size_t> sync_sendto(const void *data, size_t size, int flags, const void *addr,
                                       ::socklen_t addrlen);

  // non_blocking_sendto
  constexpr result<size_t> non_blocking_sendto(const void *data, size_t size, int flags,
                                               const void *addr, ::socklen_t addrlen);

  // select
  constexpr result<size_t> select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                  timeval *timeout);

  // Determine whether the socket is at the out-of-band data mark.
  constexpr result<bool> at_mark() const noexcept;

  // Determine the number of bytes available for reading.
  constexpr result<std::size_t> available() const noexcept;

  constexpr system_code ioctl(int cmd, int *arg) noexcept;

  // TODO: 这里用concept限制住
  // Perform an IO control command on the socket.
  template <typename IoControlCommand>
  system_code ioctl(IoControlCommand &command) {
    return basic_socket::ioctl(command.name(), command.data());
  }

 protected:
  // Copy constructor.
  constexpr basic_socket(const basic_socket &) = delete;

  // Copy assign.
  constexpr basic_socket &operator=(const basic_socket &) = delete;

  socket_state state_;

  std::optional<protocol_type> protocol_;

  exec::safe_file_descriptor descriptor_;

  context_type &context_;
};

template <typename Protocol>
constexpr system_error2::system_code basic_socket<Protocol>::set_non_blocking(bool mode) noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  int arg = mode ? 1 : 0;
  if (::ioctl(descriptor_, FIONBIO, &arg) < 0) {
    return system_error2::posix_code::current();
  } else {
    if (mode) {
      state_ |= non_blocking;
    } else {
      state_ &= ~non_blocking;
    }
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::open(const Protocol &protocol) noexcept {
  if (descriptor_ != invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  descriptor_.reset(::socket(protocol.family(), protocol.type(), protocol.protocol()));
  if (descriptor_ < 0) {
    return system_error2::posix_code::current();
  }
  if (protocol.type() == SOCK_STREAM) {
    set_state(stream_oriented);
  } else if (protocol.type() == SOCK_DGRAM) {
    set_state(datagram_oriented);
  } else {
    set_state(0);
  }
  protocol_ = protocol;
  return errc::success;
}

template <typename Protocol>
constexpr void basic_socket<Protocol>::assign(const Protocol &protocol,
                                              native_handle_type fd) noexcept {
  descriptor_.reset(fd);
  if (protocol.type() == SOCK_STREAM) {
    set_state(stream_oriented);
  } else if (protocol.type() == SOCK_DGRAM) {
    set_state(datagram_oriented);
  } else {
    set_state(0);
  }
  protocol_ = protocol;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::close() noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  if (::close(descriptor_) != 0) {
    // According to UNIX Network Programming Vol. 1, it is possible for
    // close() to fail with EWOULDBLOCK under certain circumstances. What
    // isn't clear is the state of the descriptor after this error. The one
    // current OS where this behaviour is seen, Windows, says that the socket
    // remains open. Therefore we'll put the descriptor back into blocking
    // mode and have another attempt at closing it.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      // Set back to blocking mode.
      int arg = 0;
      ::ioctl(descriptor_, FIONBIO, &arg);
      state_ &= ~non_blocking;

      // Still can't close.
      if (::close(descriptor_) != 0) {
        return system_error2::posix_code::current();
      }

      descriptor_.reset();
      return errc::success;
    }
  }

  descriptor_.reset();
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::shutdown(shutdown_type what) noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  if (::shutdown(descriptor_, static_cast<int>(what)) != 0) {
    return system_error2::posix_code::current();
  }
  descriptor_.reset();
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::bind(const endpoint_type &endpoint) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  ::sockaddr_storage storage;
  ::socklen_t size = endpoint.native_address(&storage);
  if (::bind(descriptor_, reinterpret_cast<sockaddr *>(&storage), size)) {
    return system_error2::posix_code::current();
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::listen(int backlog) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }
  if (::listen(descriptor_, backlog) != 0) {
    return system_error2::posix_code::current();
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::poll_read(int msec) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  pollfd fds{descriptor_, POLLIN, 0};
  int result = ::poll(&fds, 1, is_non_blocking() ? 0 : msec);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  if (result == 0 && is_non_blocking()) {
    return errc::operation_would_block;
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::poll_write(int msec) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  pollfd fds{descriptor_, POLLOUT, 0};
  int result = ::poll(&fds, 1, is_non_blocking() ? 0 : msec);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  if (result == 0 && is_non_blocking()) {
    return errc::operation_would_block;
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::poll_connect(int msec) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  pollfd fds{descriptor_, POLLOUT, 0};
  if (::poll(&fds, 1, msec) < 0) {
    return system_error2::posix_code::current();
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::poll_error(int msec) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  pollfd fds{descriptor_, POLLPRI | POLLERR | POLLHUP, 0};
  int result = ::poll(&fds, 1, is_non_blocking() ? 0 : msec);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  if (result == 0 && is_non_blocking()) {
    return errc::operation_would_block;
  }
  return errc::success;
}

template <typename Protocol>
result<typename Protocol::socket>  //
    constexpr basic_socket<Protocol>::accept() const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  int new_fd = ::accept(descriptor_, nullptr, 0);
  if (new_fd == invalid_socket_fd) {
    return system_error2::posix_code::current();
  }
  return socket_type{context_, protocol(), new_fd};
}

template <typename Protocol>
result<typename Protocol::socket>  //
    constexpr basic_socket<Protocol>::sync_accept() const noexcept {
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::accept();

    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if (res.error() == errc::operation_would_block
        || res.error() == errc::resource_unavailable_try_again) {
      if (state_ & non_blocking) {
        return res;
      };
    } else if (res.error() == errc::connection_aborted) {
      if (state_ & enable_connection_aborted_state) {
        return res;
      }
    } else if (res.error() == errc::protocol_error) {
      if (state_ & enable_connection_aborted_state) {
        return res;
      }
    } else {
      return res;
    }

    // Wait for socket to become ready.
    if (auto res = basic_socket::poll_read(-1); res.failure()) {
      return res;
    }
  }
}

template <typename Protocol>
result<typename Protocol::socket>  //
    constexpr basic_socket<Protocol>::non_blocking_accept() const noexcept {
  while (true) {
    // Accept the waiting connection.
    auto res = basic_socket::accept();

    // Retry operation if interrupted by signal.
    if (!res.has_value() && res.error() == errc::interrupted) {
      continue;
    }
    return res;
  }
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::connect(
    const endpoint_type &peer_endpoint) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  ::sockaddr_storage storage;
  ::socklen_t size = peer_endpoint.native_address(&storage);

  if (::connect(descriptor_, reinterpret_cast<::sockaddr *>(&storage), size) != 0) {
    if (errno == EAGAIN) {
      if (storage.ss_family == AF_UNIX) {
        return errc::operation_in_progress;
      } else {
        return errc::no_buffer_space;
      }
    }
    return system_error2::posix_code::current();
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::sync_connect(
    const endpoint_type &peer_endpoint) const noexcept {
  system_code res = basic_socket<Protocol>::connect(peer_endpoint);
  if (res.failure()                          //
      && res != errc::operation_in_progress  //
      && res != errc::operation_would_block) {
    // The connect operation finished immediately.
    return res;
  }

  // Wait for socket to become ready.
  res = basic_socket<Protocol>::poll_connect(-1);
  if (res.failure()) {
    return res;
  }

  // Get the error code from the connect operation. Return the result of the connect operation.
  int connect_error = 0;
  ::socklen_t connect_error_len = static_cast<::socklen_t>(sizeof(connect_error));
  return getsockopt(SOL_SOCKET, SO_ERROR, &connect_error, &connect_error_len);
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::non_blocking_connect(
    const typename Protocol::endpoint &peer_endpoint) const noexcept {
  pollfd fds{descriptor_, POLLOUT, 0};
  if (::poll(&fds, 1, 0) == 0) {
    return errc::operation_in_progress;
  }

  // Get the error informations from the connect operation.
  int connect_error = 0;
  ::socklen_t connect_error_len = static_cast<::socklen_t>(sizeof(connect_error));
  if (getsockopt(SOL_SOCKET, SO_ERROR, &connect_error, &connect_error_len).success()) {
    if (connect_error) {
      return system_error2::posix_code{connect_error};
    }
  }
  return errc::unknown;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::getsockopt(int level, int optname, void *optval,
                                                         ::socklen_t *optlen) const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  if (level == custom_socket_option_level && optname == always_fail_option) {
    return errc::invalid_argument;
  }

  if (level == custom_socket_option_level && optname == enable_connection_aborted_option) {
    if (*optlen != sizeof(int)) {
      return errc::invalid_argument;
    }
    *static_cast<int *>(optval) = (state_ & enable_connection_aborted_state) ? 1 : 0;
    return errc::success;
  }

  if (::getsockopt(descriptor_, level, optname, optval, optlen) != 0) {
    return system_error2::posix_code::current();
  } else {
    if (level == SOL_SOCKET                                //
        && (optname == SO_SNDBUF || optname == SO_RCVBUF)  //
        && *optlen == sizeof(int)) {
      // On Linux, setting SO_SNDBUF or SO_RCVBUF to N actually causes the kernel
      // to set the buffer size to N*2. Linux puts additional stuff into the
      // buffers so that only about half is actually available to the application.
      // The retrieved value is divided by 2 here to make it appear as though the
      // correct value has been set.
      *static_cast<int *>(optval) /= 2;
    }
  }
  return errc::success;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::setsockopt(int level, int optname, const void *optval,
                                                         ::socklen_t optlen) noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  if (level == custom_socket_option_level && optname == always_fail_option) {
    return errc::invalid_argument;
  }

  if (level == custom_socket_option_level && optname == enable_connection_aborted_option) {
    if (optlen != sizeof(int)) {
      return errc::invalid_argument;
    }

    if (*static_cast<const int *>(optval)) {
      state_ |= enable_connection_aborted_state;
    } else {
      state_ &= ~enable_connection_aborted_state;
    }

    return errc::success;
  }

  if (level == SOL_SOCKET && optname == SO_LINGER) {
    state_ |= user_set_linger;
  }

  if (::setsockopt(descriptor_, level, optname, optval, optlen) != 0) {
    return system_error2::posix_code::current();
  }
  return errc::success;
}

template <typename Protocol>
constexpr result<typename Protocol::endpoint>  //
basic_socket<Protocol>::getsockname() const noexcept {
  ::sockaddr_storage storage;
  ::socklen_t size = static_cast<::socklen_t>(sizeof(storage));
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr *>(&storage), &size) != 0) {
    return system_error2::posix_code::current();
  }
  if (storage.ss_family == AF_INET6) {
    return endpoint_type{*reinterpret_cast<sockaddr_in6 *>(&storage)};
  } else if (storage.ss_family == AF_INET) {
    return endpoint_type{*reinterpret_cast<sockaddr_in *>(&storage)};
  } else {
    return errc::address_family_not_supported;
  }
}

template <typename Protocol>
constexpr result<typename Protocol::endpoint>  //
basic_socket<Protocol>::getpeername() const noexcept {
  ::sockaddr_storage storage;
  ::socklen_t size = static_cast<::socklen_t>(sizeof(storage));
  if (::getpeername(descriptor_, reinterpret_cast<sockaddr *>(&storage), &size) != 0) {
    return system_error2::posix_code::current();
  }
  if (storage.ss_family == AF_INET6) {
    return endpoint_type{*reinterpret_cast<sockaddr_in6 *>(&storage)};
  } else if (storage.ss_family == AF_INET) {
    return endpoint_type{*reinterpret_cast<sockaddr_in *>(&storage)};
  } else {
    return errc::address_family_not_supported;
  }
}

template <typename Protocol>
constexpr result<typename Protocol::endpoint>  //
basic_socket<Protocol>::local_endpoint() const noexcept {
  return getsockname();
}

template <typename Protocol>
constexpr result<typename Protocol::endpoint>  //
basic_socket<Protocol>::peer_endpoint() const noexcept {
  return getpeername();
}

template <typename Protocol>
constexpr result<bool> basic_socket<Protocol>::at_mark() const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  int value = 0;
  if (::ioctl(descriptor_, SIOCATMARK, &value) != 0) {
    return system_error2::posix_code::current();
  }

  return value != 0;
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::available() const noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  size_t value = 0;
  if (::ioctl(descriptor_, FIONREAD, &value) != 0) {
    return system_error2::posix_code::current();
  }

  return value;
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::ioctl(int cmd, int *arg) noexcept {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  if (::ioctl(descriptor_, cmd, arg) < 0) {
    return system_error2::posix_code::current();
  } else {
    // When updating the non-blocking mode we always perform the ioctl syscall,
    // even if the flags would otherwise indicate that the socket is already in
    // the correct state_. This ensures that the underlying socket is put into
    // the state_ that has been requested by the user. If the ioctl syscall was
    // successful then we need to update the flags to match.
    if (cmd == static_cast<int>(FIONBIO)) {
      if (*arg) {
        state_ |= non_blocking;
      } else {
        state_ &= ~non_blocking;
      }
    }
  }
  return errc::success;
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::recvmsg(iovec *bufs, size_t count, int in_flags) {
  msghdr msg{.msg_iov = bufs, .msg_iovlen = count};
  ssize_t result = ::recvmsg(descriptor_, &msg, in_flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_recvmsg(iovec *bufs, size_t count, int flags,
                                                              bool all_empty) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (all_empty && (state_ & stream_oriented)) {
    return static_cast<size_t>(0);
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    result<size_t> res = basic_socket::recvmsg(bufs, count, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = basic_socket::poll_read(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_recvmsg(iovec *bufs, size_t count,
                                                                      int flags) {
  while (true) {
    // Read some data.
    auto res = basic_socket::recvmsg(bufs, count, flags);

    // Retry operation if interrupted by signal.
    if (!res.has_value() && res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

inline void init_msghdr_msg_name(void *&name, void *addr) { name = static_cast<sockaddr *>(addr); }

inline void init_msghdr_msg_name(void *&name, const sockaddr *addr) {
  name = const_cast<sockaddr *>(addr);
}

template <typename T>
inline void init_msghdr_msg_name(T &name, void *addr) {
  name = static_cast<T>(addr);
}

template <typename T>
inline void init_msghdr_msg_name(T &name, const void *addr) {
  name = static_cast<T>(const_cast<void *>(addr));
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::recvmsg_from(iovec *bufs, size_t count, int flags,
                                                              void *addr, int *addrlen) {
  msghdr msg = msghdr();
  init_msghdr_msg_name(msg.msg_name, addr);
  msg.msg_namelen = static_cast<int>(*addrlen);
  msg.msg_iov = bufs;
  msg.msg_iovlen = static_cast<int>(count);
  int32_t result = ::recvmsg(descriptor_, &msg, flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  *addrlen = msg.msg_namelen;
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_recvmsg_from(iovec *bufs, uint64_t count,
                                                                   int flags, void *addr,
                                                                   int *addrlen) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::recvmsg_from(bufs, count, flags, addr, addrlen);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = basic_socket::poll_read(0); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_recvmsg_from(iovec *bufs,
                                                                           uint64_t count,
                                                                           int flags, void *addr,
                                                                           int *addrlen) {
  while (true) {
    // Read some data.
    auto res = basic_socket::recvmsg_from(bufs, count, flags, addr, addrlen);

    // Retry operation if interrupted by signal.
    if (!res.has_value() && res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::recv(void *data, size_t size, int flags) {
  ssize_t result = ::recv(descriptor_, data, size, flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  } else if (result == 0) {
    if (state_ & stream_oriented) {
      return network_errc::eof;
    }
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_recv(void *data, size_t size, int flags) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (size == 0 && (state_ & stream_oriented)) {
    return static_cast<size_t>(0);
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::recv(data, size, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = basic_socket::poll_read(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_recv(void *data, size_t size,
                                                                   int flags) {
  while (true) {
    // Read some data.
    auto res = basic_socket::recv(data, size, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::recvfrom(void *data, size_t size, int flags,
                                                          void *addr, uint64_t *addrlen) {
  socklen_t tmp_addrlen = addrlen ? (socklen_t)*addrlen : 0;
  int32_t result = ::recvfrom(descriptor_, static_cast<char *>(data), size, flags,
                              static_cast<sockaddr *>(addr), addrlen ? &tmp_addrlen : 0);
  if (addrlen) {
    *addrlen = static_cast<uint64_t>(tmp_addrlen);
  }
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_recvfrom(void *data, size_t size, int flags,
                                                               void *addr, uint64_t *addrlen) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::recvfrom(data, size, flags, addr, addrlen);
    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = basic_socket::poll_read(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_recvfrom(void *data, size_t size,
                                                                       int flags, void *addr,
                                                                       uint64_t *addrlen) {
  while (true) {
    // Read some data.
    auto res = basic_socket::recvfrom(data, size, flags, addr, addrlen);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sendmsg(iovec *bufs, size_t count, int flags) {
  msghdr msg = msghdr{.msg_iov = bufs, .msg_iovlen = count};
  flags |= MSG_NOSIGNAL;
  ssize_t result = ::sendmsg(descriptor_, &msg, flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

inline void init_buf_iov_base(void *&base, void *addr) { base = addr; }

template <typename T>
inline void init_buf_iov_base(T &base, void *addr) {
  base = static_cast<T>(addr);
}

inline void init_buf(iovec &b, void *data, size_t size) {
  init_buf_iov_base(b.iov_base, data);
  b.iov_len = size;
}

inline void init_buf(iovec &b, const void *data, size_t size) {
  init_buf_iov_base(b.iov_base, const_cast<void *>(data));
  b.iov_len = size;
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_sendmsg(iovec *bufs, size_t count, int flags,
                                                              bool all_empty) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // A request to write 0 bytes to a stream is a no-op.
  if (all_empty && (state_ & stream_oriented)) {
    return static_cast<size_t>(0);
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::sendmsg(bufs, count, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = poll_write(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_sendmsg(iovec *bufs, size_t count,
                                                                      int flags) {
  while (true) {
    // Write some data.
    auto res = basic_socket::sendmsg(bufs, count, flags);
    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sendmsg_to(iovec *bufs, size_t count, int flags,
                                                            const void *addr, ::socklen_t addrlen) {
  msghdr msg = msghdr{.msg_namelen = addrlen, .msg_iov = bufs, .msg_iovlen = count};
  init_msghdr_msg_name(msg.msg_name, addr);

  int32_t result = ::sendmsg(descriptor_, &msg, flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_sendmsg_to(iovec *bufs, size_t count,
                                                                 int flags, const void *addr,
                                                                 ::socklen_t addrlen) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // Write some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::sendmsg_to(bufs, count, flags, addr, addrlen);
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = poll_write(-1); err.failure()) {
      return res;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_sendmsg_to(iovec *bufs, size_t count,
                                                                         int flags,
                                                                         const void *addr,
                                                                         ::socklen_t addrlen) {
  while (true) {
    // Write some data.
    auto res = basic_socket::sendmsg_to(bufs, count, flags, addr, addrlen);
    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }
    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::send(const void *data, size_t size, int flags) {
  flags |= MSG_NOSIGNAL;
  int32_t result = ::send(descriptor_, data, size, flags);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_send(const void *data, size_t size,
                                                           int flags) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // A request to write 0 bytes to a stream is a no-op.
  if (size == 0 && (state_ & stream_oriented)) {
    return static_cast<size_t>(0);
  }

  // Read some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::send(data, size, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = poll_write(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_send(const void *data, size_t size,
                                                                   int flags) {
  while (true) {
    // Write some data.
    auto res = basic_socket::send(data, size, flags);

    // Check if operation succeeded.
    if (res.has_value()) {
      return res.value();
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }
    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sendto(const void *data, size_t size, int flags,
                                                        const void *addr, ::socklen_t addrlen) {
  int32_t result =
      ::sendto(descriptor_, data, size, flags, static_cast<const sockaddr *>(addr), addrlen);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::sync_sendto(const void *data, size_t size,
                                                             int flags, const void *addr,
                                                             ::socklen_t addrlen) {
  if (descriptor_ == invalid_socket_fd) {
    return errc::bad_file_descriptor;
  }

  // Write some data.
  while (true) {
    // Try to complete the operation without blocking.
    auto res = basic_socket::sendto(data, size, flags, addr, addrlen);
    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Operation failed.
    if ((state_ & non_blocking)
        || (res.error() != errc::operation_would_block
            && res.error() != errc::resource_unavailable_try_again)) {
      return res;
    }

    // Wait for socket to become ready.
    if (auto err = poll_write(-1); err.failure()) {
      return err;
    }
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::non_blocking_sendto(const void *data, size_t size,
                                                                     int flags, const void *addr,
                                                                     ::socklen_t addrlen) {
  while (true) {
    // Write some data.
    auto res = basic_socket::sendto(data, size, flags, addr, addrlen);
    // Check if operation succeeded.
    if (res.has_value()) {
      return res;
    }

    // Retry operation if interrupted by signal.
    if (res.error() == errc::interrupted) {
      continue;
    }

    return res;
  }
}

template <typename Protocol>
constexpr result<size_t> basic_socket<Protocol>::select(int nfds, fd_set *readfds, fd_set *writefds,
                                                        fd_set *exceptfds, timeval *timeout) {
  int result = ::select(nfds, readfds, writefds, exceptfds, timeout);
  if (result < 0) {
    return system_error2::posix_code::current();
  }
  return static_cast<size_t>(result);
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::getaddrinfo(const char *host, const char *service,
                                                          const addrinfo &hints,
                                                          addrinfo **result) noexcept {
  host = (host && *host) ? host : 0;
  service = (service && *service) ? service : 0;

  if (::getaddrinfo(host, service, &hints, result) != 0) {
    return system_error2::posix_code::current();
  }
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::getnameinfo(const void *addr, ::socklen_t addrlen,
                                                          char *host, uint64_t hostlen, char *serv,
                                                          uint64_t servlen, int flags) noexcept {
  if (getnameinfo(static_cast<const sockaddr *>(addr), addrlen, host, hostlen, serv, servlen, flags)
      != 0) {
    return system_error2::posix_code::current();
  }
}

template <typename Protocol>
constexpr system_code basic_socket<Protocol>::gethostname(char *name, size_t namelen) noexcept {
  int result = ::gethostname(name, namelen);
  if (result != 0) {
    return system_error2::posix_code::current();
  }
  return errc::success;
}

}  // namespace net

#endif  // NET_SRC_BASIC_SOCKET_HPP_
