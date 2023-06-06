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
#ifndef SOCKET_BASE_HPP_
#define SOCKET_BASE_HPP_

#include <fcntl.h>

#include "exec/linux/safe_file_descriptor.hpp"
#include "io_control.hpp"
#include "ip/socket_types.hpp"
#include "socket_option.hpp"

namespace net {

// The socket_base class is used as a base for the basic_stream_socket and
// basic_datagram_socket class templates so that we have a common place to
// define the shutdown_type and enum and lots of other functions.
class socket_base {
 public:
  // Different ways a socket may be shutdown.
  enum class shutdown_type : int {
    // Shutdown the receive side of the socket.
    shutdown_receive = SHUT_RD,

    // Shutdown the send side of the socket.
    shutdown_send = SHUT_WR,

    // Shutdown both send and receive on the socket.
    shutdown_both = SHUT_RDWR
  };

  // Wait types. For use with basic_socket::wait() and
  // basic_socket::async_wait().
  enum class wait_type {
    // Wait for a socket to become ready to read.
    wait_read,

    // Wait for a socket to become ready to write.
    wait_write,

    // Wait for a socket to have error conditions pending.
    wait_error
  };

  // The type of platform specific descriptor.
  using native_handle_type = int;

  // Bitmask type for flags that can be passed to send and receive operations.
  using message_flags = int;

  // Peek at incoming data without removing it from the input queue.
  static const message_flags message_peek = MSG_PEEK;

  // Process out-of-band data.
  static const message_flags message_out_of_band = MSG_OOB;

  // Specify that the data should not be subject to routing.
  static const message_flags message_do_not_route = MSG_DONTROUTE;

  // Specifies that the data marks the end of a record.
  static const message_flags message_end_of_record = MSG_EOR;

  // Socket option to permit sending of broadcast messages.
  using broadcast = socket_option::boolean<SOL_SOCKET, SO_BROADCAST>;

  // Socket option to enable socket-level debugging.
  using debug = socket_option::boolean<SOL_SOCKET, SO_DEBUG>;

  // Socket option to prevent routing, use local interfaces only.
  using do_not_route = socket_option::boolean<SOL_SOCKET, SO_DONTROUTE>;

  // Socket option to send keep-alive.
  using keep_alive = socket_option::boolean<SOL_SOCKET, SO_KEEPALIVE>;

  // Socket option for the send buffer size of a socket.
  using send_buffer_size = socket_option::integer<SOL_SOCKET, SO_SNDBUF>;

  // Socket option for the send low watermark.
  using send_low_water_mark = socket_option::integer<SOL_SOCKET, SO_SNDLOWAT>;

  // Socket option for the receive buffer size of a socket.
  using receive_buffer_size = socket_option::integer<SOL_SOCKET, SO_RCVBUF>;

  // Socket option for the receive low watermark.
  using receive_low_water_mark =
      socket_option::integer<SOL_SOCKET, SO_RCVLOWAT>;

  // Socket option to allow the socket to be bound to an address that is
  // already in use.
  using reuse_address = socket_option::boolean<SOL_SOCKET, SO_REUSEADDR>;

  // Socket option to specify whether the socket lingers on close if unsent
  // data is present.
  using linger = socket_option::linger<SOL_SOCKET, SO_LINGER>;

  // Socket option for putting received out-of-band data inline.
  using out_of_band_Inline = socket_option::boolean<SOL_SOCKET, SO_OOBINLINE>;

  // Socket option to report aborted connections on accept.
  using enable_connection_aborted =
      socket_option::boolean<custom_socket_option_level,
                             enable_connection_aborted_option>;

  // IO control command to get the amount of data that can be read without
  // blocking.
  using bytes_readable = io_control::bytes_readable;
};
}  // namespace net

#endif  // SOCKET_BASE_HPP_
