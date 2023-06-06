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
#ifndef EVENTFD_INTERRUPTER_HPP_
#define EVENTFD_INTERRUPTER_HPP_

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>  // NOLINT

#include "ip/socket_types.hpp"

namespace net {

// Use eventfd to wake up blocking calls. When creating an eventfd fails, a pipe
// is used to create a file descriptor
class eventfd_interrupter {
 public:
  // Constructor.
  eventfd_interrupter() { create_descriptors(); }

  // Destructor.
  constexpr ~eventfd_interrupter() { close_descriptors(); }

  // Interrupt the call.
  bool interrupt() {
    uint64_t counter(1UL);
    return ::write(write_descriptor_, &counter, sizeof(uint64_t)) > 0;
  }

  // Reset the interrupter. Returns true if the reset was successful.
  bool reset() {
    if (write_descriptor_ == read_descriptor_) {
      while (true) {
        // Only perform one read. The kernel maintains an atomic counter.
        uint64_t counter(0);
        errno = 0;
        int bytes_read = ::read(read_descriptor_, &counter, sizeof(uint64_t));
        if (bytes_read < 0 && errno == EINTR) {
          continue;
        }
        return true;
      }
    } else {
      while (true) {
        // Clear all data from the pipe.
        char data[1024];
        int bytes_read = ::read(read_descriptor_, data, sizeof(data));
        if (bytes_read == sizeof(data)) {
          continue;
        }
        if (bytes_read > 0) {
          return true;
        }
        if (bytes_read == 0) {
          return false;
        }
        if (errno == EINTR) {
          continue;
        }
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          return true;
        }
        return false;
      }
    }
  }

  // Get the read descriptor.
  constexpr int read_descriptor() const { return read_descriptor_; }

 private:
  // Open the descriptors. Throws exception when open fails.
  // If the values of the two descriptors are the same,  the eventfd is
  // successfully created. If the values of the two descriptors are different,
  // the pipe fd is successfully created.
  void create_descriptors() {
    write_descriptor_ = read_descriptor_ =
        ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    if (read_descriptor_ == invalid_socket_fd && errno == EINVAL) {
      write_descriptor_ = read_descriptor_ = ::eventfd(0, 0);
      if (read_descriptor_ != invalid_socket_fd) {
        ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);
      }
    }
    if (read_descriptor_ == invalid_socket_fd) {
      int pipe_fds[2];
      if (pipe(pipe_fds) == 0) {
        read_descriptor_ = pipe_fds[0];
        ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);

        write_descriptor_ = pipe_fds[1];
        ::fcntl(write_descriptor_, F_SETFL, O_NONBLOCK);
        ::fcntl(write_descriptor_, F_SETFD, FD_CLOEXEC);
      } else {
        throw std::system_error{static_cast<int>(errno), std::system_category(),
                                "eventfd_interrupter"};
      }
    }
  }

  // Close the descriptors.
  constexpr void close_descriptors() {
    if (write_descriptor_ != invalid_socket_fd &&
        write_descriptor_ != read_descriptor_) {
      ::close(write_descriptor_);
    }
    if (read_descriptor_ != invalid_socket_fd) {
      ::close(read_descriptor_);
    }
    read_descriptor_ = invalid_socket_fd;
    write_descriptor_ = invalid_socket_fd;
  }

  // The read end of a connection used to interrupt the blocking call. This file
  // descriptor is passed to select/epoll etc such that when it is time to stop,
  // a single 64bit value will be written on the other end of the connection and
  // this descriptor will become readable.
  int read_descriptor_;

  // The write end of a connection used to interrupt the blocking call. A single
  // 64bit non-zero value may be written to this to wake up the select/epoll
  // which is waiting for the other end to become readable. This descriptor will
  // only differ from the read descriptor when a pipe is used.
  int write_descriptor_;
};
}  // namespace net

#endif  // EVENTFD_INTERRUPTER_HPP_
