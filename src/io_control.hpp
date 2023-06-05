#ifndef NET_SRC_IO_CONTROL_HPP_
#define NET_SRC_IO_CONTROL_HPP_

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
  constexpr bytes_readable(std::size_t value) : value_(static_cast<int>(value)) {}

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

#endif  // NET_SRC_IO_CONTROL_HPP_
