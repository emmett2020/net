/*
 * Copyright (c) 2023 Runner-2019
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef BASIC_SOCKET_HPP_
#define BASIC_SOCKET_HPP_

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <optional>

#include "buffer_sequence_adapter.hpp"
#include "exec/linux/safe_file_descriptor.hpp"
#include "execution_context.hpp"
#include "io_control.hpp"
#include "ip/socket_types.hpp"
#include "net_error.hpp"
#include "socket_base.hpp"
#include "socket_option.hpp"
#include "status-code/generic_code.hpp"
#include "status-code/result.hpp"
#include "status-code/system_code.hpp"

namespace net {
// Code   : https://github.com/ned14/status-code
// Paper  : https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1028r4.pdf
// Example: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1883r2.pdf
using system_error2::errc;
using system_error2::result;
using system_error2::system_code;

// basic_socket is the base class for all socket types. It defines various
// socket-related functions. We don't force a context on basic_socket (as `Asio`
// does) because basic_socket defines generic socket operations that are
// independent of their context.
template <typename Protocol>
class basic_socket : public socket_base {
 public:
  using native_handle_type = socket_base::native_handle_type;
  using protocol_type = Protocol;
  using endpoint_type = typename protocol_type::endpoint;
  using socket_type = typename protocol_type::socket;
  using socket_state = unsigned char;
  using context_type = execution_context;

  // Default constructor.
  explicit constexpr basic_socket(context_type& ctx) noexcept
      : state_(0), protocol_(), descriptor_(), context_(ctx) {}

  // Constructor with specific protocol and create a new descriptor.
  constexpr basic_socket(context_type& ctx, const protocol_type& protocol,
                         system_code& code) noexcept
      : context_(ctx) {
    code = basic_socket::open(protocol);
  }

  // Constructor with native socket and specific protocol.
  constexpr basic_socket(context_type& ctx, const protocol_type& protocol,
                         native_handle_type fd) noexcept
      : context_(ctx) {
    assign(protocol, fd);
  }

  // Move constructor.
  // The file descriptor of the moved socket will be set to invalid.
  constexpr basic_socket(basic_socket&& o) noexcept
      : state_(static_cast<socket_state&&>(o.state_)),
        protocol_(static_cast<std::optional<protocol_type>&&>(o.protocol_)),
        descriptor_(static_cast<exec::safe_file_descriptor&&>(o.descriptor_)),
        context_(o.context_) {}

  // Move assign.
  constexpr basic_socket& operator=(basic_socket&& other) noexcept {
    state_ = static_cast<socket_state&&>(other.state_);
    protocol_ = static_cast<std::optional<protocol_type>&&>(other.protocol_);
    descriptor_ = static_cast<exec::safe_file_descriptor&&>(other.descriptor_);
    context_ = other.context_;
    return *this;
  }

  constexpr ~basic_socket() = default;

  // Get associated context.
  constexpr context_type& context() noexcept { return context_; }

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
  constexpr system_code set_non_blocking(bool mode) noexcept {
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

  // Gets the non-blocking mode of the socket.
  constexpr bool is_non_blocking() const noexcept {
    return (state_ & non_blocking) != 0;
  }

  // Open a new file descriptor. Reassign protocol_ and state_ based on given
  // protocol.
  constexpr system_code open(const protocol_type& protocol) noexcept {
    if (descriptor_ != invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }
    descriptor_.reset(
        ::socket(protocol.family(), protocol.type(), protocol.protocol()));
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

  // Check whether socket is opened.
  constexpr bool is_open() const noexcept {
    return descriptor_.native_handle() != invalid_socket_fd;
  }

  // Assign a native file descriptor with specified protocol to this socket.
  constexpr void assign(const protocol_type& protocol,
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

  // Close this socket and reset descriptor. state and protocol are not reset.
  constexpr system_code close() noexcept {
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

  // Disable sends or receives on the socket. state and protocol are not reset.
  constexpr system_code shutdown(shutdown_type what) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }
    if (::shutdown(descriptor_, static_cast<int>(what)) != 0) {
      return system_error2::posix_code::current();
    }
    descriptor_.reset();
    return errc::success;
  }

  // Bind socket to given endpoint.
  constexpr system_code bind(const endpoint_type& endpoint) const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }
    ::sockaddr_storage storage;
    ::socklen_t size = endpoint.native_address(&storage);
    if (::bind(descriptor_, reinterpret_cast<sockaddr*>(&storage), size)) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // Place the socket into the state where it will listen for new connections.
  constexpr system_code listen(
      int backlog = max_listen_connections) const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }
    if (::listen(descriptor_, backlog) != 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // Poll read.
  constexpr system_code poll_read(int msec) const noexcept {
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

  // Poll write.
  constexpr system_code poll_write(int msec) const noexcept {
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

  // Poll connect.
  constexpr system_code poll_connect(int msec) const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    pollfd fds{descriptor_, POLLOUT, 0};
    if (::poll(&fds, 1, msec) < 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // Poll error.
  constexpr system_code poll_error(int msec) const noexcept {
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

  // getaddrinfo
  constexpr system_code getaddrinfo(const char* host, const char* service,
                                    const ::addrinfo& hints,
                                    addrinfo** result) noexcept {
    host = (host && *host) ? host : 0;
    service = (service && *service) ? service : 0;
    if (::getaddrinfo(host, service, &hints, result) != 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // getnameinfo
  constexpr system_code getnameinfo(const void* addr, ::socklen_t addrlen,
                                    char* host, uint64_t hostlen, char* serv,
                                    uint64_t serv_len, int flags) noexcept {
    if (getnameinfo(static_cast<const sockaddr*>(addr), addrlen, host, hostlen,
                    serv, serv_len, flags) != 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // gethostname
  constexpr system_code gethostname(char* name, size_t namelen) noexcept {
    int result = ::gethostname(name, namelen);
    if (result != 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // Provides an endpoint, which is set when getsockname executes successfully.
  constexpr result<endpoint_type> getsockname() const noexcept {
    ::sockaddr_storage storage;
    ::socklen_t size = static_cast<::socklen_t>(sizeof(storage));
    if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&storage),
                      &size) != 0) {
      return system_error2::posix_code::current();
    }
    if (storage.ss_family == AF_INET6) {
      return endpoint_type{*reinterpret_cast<sockaddr_in6*>(&storage)};
    } else if (storage.ss_family == AF_INET) {
      return endpoint_type{*reinterpret_cast<sockaddr_in*>(&storage)};
    } else {
      return errc::address_family_not_supported;
    }
  }

  // Provides an endpoint, which is set when getpeername executes successfully.
  constexpr result<endpoint_type> getpeername() const noexcept {
    ::sockaddr_storage storage;
    ::socklen_t size = static_cast<::socklen_t>(sizeof(storage));
    if (::getpeername(descriptor_, reinterpret_cast<sockaddr*>(&storage),
                      &size) != 0) {
      return system_error2::posix_code::current();
    }
    if (storage.ss_family == AF_INET6) {
      return endpoint_type{*reinterpret_cast<sockaddr_in6*>(&storage)};
    } else if (storage.ss_family == AF_INET) {
      return endpoint_type{*reinterpret_cast<sockaddr_in*>(&storage)};
    } else {
      return errc::address_family_not_supported;
    }
  }

  // Get the local endpoint.
  constexpr result<endpoint_type> local_endpoint() const noexcept {
    return getsockname();
  }

  // Get the peer endpoint.
  constexpr result<endpoint_type> peer_endpoint() const noexcept {
    return getpeername();
  }

  // Get option for this socket.
  constexpr system_code getsockopt(int level, int opt_name, void* opt_val,
                                   ::socklen_t* opt_len) const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    if (level == custom_socket_option_level && opt_name == always_fail_option) {
      return errc::invalid_argument;
    }

    if (level == custom_socket_option_level &&
        opt_name == enable_connection_aborted_option) {
      if (*opt_len != sizeof(int)) {
        return errc::invalid_argument;
      }
      *static_cast<int*>(opt_val) =
          (state_ & enable_connection_aborted_state) ? 1 : 0;
      return errc::success;
    }

    if (::getsockopt(descriptor_, level, opt_name, opt_val, opt_len) != 0) {
      return system_error2::posix_code::current();
    } else {
      if (level == SOL_SOCKET                                  //
          && (opt_name == SO_SNDBUF || opt_name == SO_RCVBUF)  //
          && *opt_len == sizeof(int)) {
        // On Linux, setting SO_SNDBUF or SO_RCVBUF to N actually causes the
        // kernel to set the buffer size to N*2. Linux puts additional stuff
        // into the buffers so that only about half is actually available to the
        // application. The retrieved value is divided by 2 here to make it
        // appear as though the correct value has been set.
        *static_cast<int*>(opt_val) /= 2;
      }
    }
    return errc::success;
  }

  // Set socket options for fd of this socket.
  constexpr system_code setsockopt(int level, int opt_name, const void* opt_val,
                                   ::socklen_t opt_len) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    if (level == custom_socket_option_level && opt_name == always_fail_option) {
      return errc::invalid_argument;
    }

    if (level == custom_socket_option_level &&
        opt_name == enable_connection_aborted_option) {
      if (opt_len != sizeof(int)) {
        return errc::invalid_argument;
      }

      if (*static_cast<const int*>(opt_val)) {
        state_ |= enable_connection_aborted_state;
      } else {
        state_ &= ~enable_connection_aborted_state;
      }

      return errc::success;
    }

    if (level == SOL_SOCKET && opt_name == SO_LINGER) {
      state_ |= user_set_linger;
    }

    if (::setsockopt(descriptor_, level, opt_name, opt_val, opt_len) != 0) {
      return system_error2::posix_code::current();
    }
    return errc::success;
  }

  // Get option for this socket.
  template <typename Option>
  constexpr system_code get_option(Option& option) const noexcept {
    ::socklen_t size = static_cast<::socklen_t>(option.size(protocol_));
    return basic_socket::getsockopt(option.level(protocol_),
                                    option.name(protocol_),
                                    option.data(protocol_), &size);
  }

  // TODO(xiaoming): add concept for Option
  template <typename Option>
  constexpr system_code set_option(const Option& option) noexcept {
    return basic_socket::setsockopt(
        option.level(protocol_), option.name(protocol_), option.data(protocol_),
        option.size(protocol_));
  }

  // Accept a new connection.
  constexpr result<socket_type> accept() const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    int new_fd = ::accept(descriptor_, nullptr, 0);
    if (new_fd == invalid_socket_fd) {
      return system_error2::posix_code::current();
    }
    return socket_type{context_, protocol(), new_fd};
  }

  // Accept a new connection.
  // This function will block itself while waiting connection.
  constexpr result<socket_type> sync_accept() const noexcept {
    while (true) {
      // Try to complete the operation without blocking.
      auto res = basic_socket::accept();
      if (res.has_value()) {
        return res;
      }

      // Operation failed.
      if (res.error() == errc::operation_would_block ||
          res.error() == errc::resource_unavailable_try_again) {
        if (state_ & non_blocking) {
          return res;
        }
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

  // Accept a new connection without blocking.
  constexpr result<socket_type> non_blocking_accept() const noexcept {
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

  // Connect the socket to the specified endpoint.
  constexpr system_code connect(
      const endpoint_type& peer_endpoint) const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    ::sockaddr_storage storage;
    ::socklen_t size = peer_endpoint.native_address(&storage);

    if (::connect(descriptor_, reinterpret_cast<::sockaddr*>(&storage), size) !=
        0) {
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

  // Connect to peer which may be blocked.
  constexpr system_code sync_connect(
      const endpoint_type& peer_endpoint) const noexcept {
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

    // Get the error code from the connect operation.
    // Return the result of the connect operation.
    int connect_error = 0;
    ::socklen_t len = static_cast<::socklen_t>(sizeof(connect_error));
    return getsockopt(SOL_SOCKET, SO_ERROR, &connect_error, &len);
  }

  // Connect to peer without blocking.
  constexpr system_code non_blocking_connect(
      const endpoint_type& peer_endpoint) const noexcept {
    pollfd fds{descriptor_, POLLOUT, 0};
    if (::poll(&fds, 1, 0) == 0) {
      return errc::operation_in_progress;
    }

    // Get the error informations from the connect operation.
    int connect_error = 0;
    ::socklen_t len = static_cast<::socklen_t>(sizeof(connect_error));
    if (getsockopt(SOL_SOCKET, SO_ERROR, &connect_error, &len).success()) {
      if (connect_error) {
        return system_error2::posix_code{connect_error};
      }
    }
    return errc::unknown;
  }

  // Recvmsg.
  constexpr result<size_t> recvmsg(iovec* bufs, size_t count,
                                   int in_flags) noexcept {
    msghdr msg{.msg_iov = bufs, .msg_iovlen = count};
    ssize_t result = ::recvmsg(descriptor_, &msg, in_flags);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // sync_recvmsg with blocking.
  constexpr result<size_t> sync_recvmsg(iovec* bufs, size_t count, int flags,
                                        bool all_empty = false) noexcept {
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
      if (res.has_value()) {
        return res;
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = basic_socket::poll_read(-1); err.failure()) {
        return err;
      }
    }
  }

  // Recvmsg without blocking.
  constexpr result<size_t> non_blocking_recvmsg(iovec* bufs, size_t count,
                                                int flags) noexcept {
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

  // Recvmsg from
  constexpr result<size_t> recvmsg_from(iovec* bufs, size_t count, int flags,
                                        void* addr, int* addrlen) noexcept {
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

  // sync_recvmsg_from with blocking.
  constexpr result<size_t> sync_recvmsg_from(iovec* bufs, size_t count,
                                             int flags, void* addr,
                                             int* addrlen) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    // Read some data.
    while (true) {
      // Try to complete the operation without blocking.
      auto res = basic_socket::recvmsg_from(bufs, count, flags, addr, addrlen);
      if (res.has_value()) {
        return res.value();
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = basic_socket::poll_read(0); err.failure()) {
        return err;
      }
    }
  }

  // Recvmsg without blocking.
  constexpr result<size_t> non_blocking_recvmsg_from(iovec* bufs, size_t count,
                                                     int flags, void* addr,
                                                     int* addrlen) noexcept {
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

  // Recv.
  constexpr result<size_t> recv(void* data, size_t size, int flags) noexcept {
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

  // Recv with blocking.
  constexpr result<size_t> sync_recv(void* data, size_t size,
                                     int flags) noexcept {
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
      if (res.has_value()) {
        return res.value();
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = basic_socket::poll_read(-1); err.failure()) {
        return err;
      }
    }
  }

  // Recv without blocking.
  constexpr result<size_t> non_blocking_recv(void* data, size_t size,
                                             int flags) noexcept {
    while (true) {
      // Read some data.
      auto res = basic_socket::recv(data, size, flags);
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

  // Recvfrom.
  constexpr result<size_t> recvfrom(void* data, size_t size, int flags,
                                    void* addr, uint64_t* addrlen) noexcept {
    socklen_t tmp_addrlen = addrlen ? (socklen_t)*addrlen : 0;
    int32_t result =
        ::recvfrom(descriptor_, static_cast<char*>(data), size, flags,
                   static_cast<sockaddr*>(addr), addrlen ? &tmp_addrlen : 0);
    if (addrlen) {
      *addrlen = static_cast<uint64_t>(tmp_addrlen);
    }
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // Sync recvfrom
  constexpr result<size_t> sync_recvfrom(void* data, size_t size, int flags,
                                         void* addr,
                                         uint64_t* addrlen) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    // Read some data.
    while (true) {
      // Try to complete the operation without blocking.
      auto res = basic_socket::recvfrom(data, size, flags, addr, addrlen);
      if (res.has_value()) {
        return res;
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = basic_socket::poll_read(-1); err.failure()) {
        return err;
      }
    }
  }

  // non_blocking_recvfrom
  constexpr result<size_t> non_blocking_recvfrom(void* data, size_t size,
                                                 int flags, void* addr,
                                                 uint64_t* addrlen) noexcept {
    while (true) {
      // Read some data.
      auto res = basic_socket::recvfrom(data, size, flags, addr, addrlen);
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

  // sendmsg
  constexpr result<size_t> sendmsg(iovec* bufs, uint64_t count,
                                   int flags) noexcept {
    msghdr msg{.msg_iov = bufs, .msg_iovlen = count};
    flags |= MSG_NOSIGNAL;
    ssize_t result = ::sendmsg(descriptor_, &msg, flags);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // sync_sendmsg
  constexpr result<size_t> sync_sendmsg(iovec* bufs, uint64_t count, int flags,
                                        bool all_empty = false) noexcept {
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
      if (res.has_value()) {
        return res;
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = poll_write(-1); err.failure()) {
        return err;
      }
    }
  }

  // non_blocking_sendmsg
  constexpr result<size_t> non_blocking_sendmsg(iovec* bufs, uint64_t count,
                                                int flags) noexcept {
    while (true) {
      // Write some data.
      auto res = basic_socket::sendmsg(bufs, count, flags);
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

  // sendmsg_to
  constexpr result<size_t> sendmsg_to(iovec* bufs, uint64_t count, int flags,
                                      const void* addr,
                                      ::socklen_t addrlen) noexcept {
    msghdr msg{.msg_namelen = addrlen, .msg_iov = bufs, .msg_iovlen = count};
    init_msghdr_msg_name(msg.msg_name, addr);

    int32_t result = ::sendmsg(descriptor_, &msg, flags);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // sync_sendto
  constexpr result<size_t> sync_sendmsg_to(iovec* bufs, uint64_t count,
                                           int flags, const void* addr,
                                           ::socklen_t addrlen) noexcept {
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
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = poll_write(-1); err.failure()) {
        return res;
      }
    }
  }

  // non_blocking_sendto
  constexpr result<size_t> non_blocking_sendmsg_to(
      iovec* bufs, uint64_t count, int flags, const void* addr,
      ::socklen_t addrlen) noexcept {
    while (true) {
      auto res = basic_socket::sendmsg_to(bufs, count, flags, addr, addrlen);
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

  // send
  constexpr result<size_t> send(const void* data, size_t size,
                                int flags) noexcept {
    flags |= MSG_NOSIGNAL;
    int32_t result = ::send(descriptor_, data, size, flags);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // sync_send
  constexpr result<size_t> sync_send(const void* data, size_t size,
                                     int flags) noexcept {
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
      if (res.has_value()) {
        return res.value();
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = poll_write(-1); err.failure()) {
        return err;
      }
    }
  }

  // non_blocking_send
  constexpr result<size_t> non_blocking_send(const void* data, size_t size,
                                             int flags) noexcept {
    while (true) {
      auto res = basic_socket::send(data, size, flags);
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

  // sendto
  constexpr result<size_t> sendto(const void* data, size_t size, int flags,
                                  const void* addr,
                                  ::socklen_t addrlen) noexcept {
    int32_t result = ::sendto(descriptor_, data, size, flags,
                              static_cast<const sockaddr*>(addr), addrlen);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // sync_sendto
  constexpr result<size_t> sync_sendto(const void* data, size_t size, int flags,
                                       const void* addr,
                                       ::socklen_t addrlen) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    while (true) {
      // Try to complete the operation without blocking.
      auto res = basic_socket::sendto(data, size, flags, addr, addrlen);
      if (res.has_value()) {
        return res;
      }

      // Operation failed.
      if ((state_ & non_blocking) ||
          (res.error() != errc::operation_would_block &&
           res.error() != errc::resource_unavailable_try_again)) {
        return res;
      }

      // Wait for socket to become ready.
      if (auto err = poll_write(-1); err.failure()) {
        return err;
      }
    }
  }

  // non_blocking_sendto
  constexpr result<size_t> non_blocking_sendto(const void* data, size_t size,
                                               int flags, const void* addr,
                                               ::socklen_t addrlen) noexcept {
    while (true) {
      auto res = basic_socket::sendto(data, size, flags, addr, addrlen);
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

  // select
  constexpr result<size_t> select(int nfds, fd_set* readfds, fd_set* writefds,
                                  fd_set* exceptfds,
                                  timeval* timeout) noexcept {
    int result = ::select(nfds, readfds, writefds, exceptfds, timeout);
    if (result < 0) {
      return system_error2::posix_code::current();
    }
    return static_cast<size_t>(result);
  }

  // Determine whether the socket is at the out-of-band data mark.
  constexpr result<bool> at_mark() const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    int value = 0;
    if (::ioctl(descriptor_, SIOCATMARK, &value) != 0) {
      return system_error2::posix_code::current();
    }

    return value != 0;
  }

  // Determine the number of bytes available for reading.
  constexpr result<std::size_t> available() const noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    size_t value = 0;
    if (::ioctl(descriptor_, FIONREAD, &value) != 0) {
      return system_error2::posix_code::current();
    }

    return value;
  }

  constexpr system_code ioctl(int cmd, int* arg) noexcept {
    if (descriptor_ == invalid_socket_fd) {
      return errc::bad_file_descriptor;
    }

    if (::ioctl(descriptor_, cmd, arg) < 0) {
      return system_error2::posix_code::current();
    } else {
      // When updating the non-blocking mode we always perform the ioctl
      // syscall, even if the flags would otherwise indicate that the socket is
      // already in the correct state_. This ensures that the underlying socket
      // is put into the state_ that has been requested by the user. If the
      // ioctl syscall was successful then we need to update the flags to match.
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

  // TODO(xiaoming): add concept for IoControlCommand
  // Perform an IO control command on the socket.
  template <typename IoControlCommand>
  system_code ioctl(IoControlCommand& command) {
    return basic_socket::ioctl(command.name(), command.data());
  }

 protected:
  // Copy constructor.
  constexpr basic_socket(const basic_socket&) noexcept = delete;

  // Copy assign.
  constexpr basic_socket& operator=(const basic_socket&) noexcept = delete;

  static void init_msghdr_msg_name(void*& name, void* addr) {
    name = static_cast<sockaddr*>(addr);
  }

  static void init_msghdr_msg_name(void*& name, const sockaddr* addr) {
    name = const_cast<sockaddr*>(addr);
  }

  template <typename T>
  static void init_msghdr_msg_name(T& name, void* addr) {
    name = static_cast<T>(addr);
  }

  template <typename T>
  static void init_msghdr_msg_name(T& name, const void* addr) {
    name = static_cast<T>(const_cast<void*>(addr));
  }

  static void init_buf_iov_base(void*& base, void* addr) { base = addr; }

  template <typename T>
  static void init_buf_iov_base(T& base, void* addr) {
    base = static_cast<T>(addr);
  }

  static void init_buf(iovec& b, void* data, size_t size) {
    init_buf_iov_base(b.iov_base, data);
    b.iov_len = size;
  }

  static void init_buf(iovec& b, const void* data, size_t size) {
    init_buf_iov_base(b.iov_base, const_cast<void*>(data));
    b.iov_len = size;
  }

  socket_state state_;
  std::optional<protocol_type> protocol_;
  exec::safe_file_descriptor descriptor_;
  context_type& context_;
};

}  // namespace net

#endif  // BASIC_SOCKET_HPP_
