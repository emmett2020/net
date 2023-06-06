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
#ifndef IO_CONTROL_HPP_
#define IO_CONTROL_HPP_

#include <sys/ioctl.h>

#include <cstddef>

#include "ip/socket_types.hpp"

namespace net {
namespace io_control {

// I/O control command for getting number of bytes available.
class bytes_readable {
 public:
  // Default constructor.
  constexpr bytes_readable() : value_(0) {}

  // Construct with a specific command value.
  explicit constexpr bytes_readable(std::size_t value)
      : value_(static_cast<int>(value)) {}

  // Get the name of the IO control command.
  constexpr int name() const { return static_cast<int>(FIONREAD); }

  // Set the value of the I/O control command.
  constexpr void set(std::size_t value) { value_ = static_cast<int>(value); }

  // Get the current value of the I/O control command.
  constexpr std::size_t get() const { return static_cast<std::size_t>(value_); }

  // Get the address of the command data.
  constexpr int* data() { return &value_; }

  // Get the address of the command data.
  constexpr const int* data() const { return &value_; }

 private:
  int value_;
};

}  // namespace io_control
}  // namespace net

#endif  // IO_CONTROL_HPP_
