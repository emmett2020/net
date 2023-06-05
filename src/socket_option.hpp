#ifndef NET_SRC_SOCKET_OPTION_HPP_
#define NET_SRC_SOCKET_OPTION_HPP_

#include <sys/socket.h>

#include <cstddef>
#include <exception>

#include "ip/socket_types.hpp"

namespace net {
namespace socket_option {

// Helper template for implementing boolean-based options.
template <int Level, int Name>
class boolean {
 public:
  // Default constructor.
  constexpr boolean() : value_(0) {}

  // Construct with a specific option value.
  explicit constexpr boolean(bool v) : value_(v ? 1 : 0) {}

  // Set the current value of the boolean.
  constexpr boolean& operator=(bool v) {
    value_ = v ? 1 : 0;
    return *this;
  }

  // Get the current value of the boolean.
  constexpr bool value() const { return !!value_; }

  // Convert to bool.
  constexpr operator bool() const { return !!value_; }

  // Test for false.
  constexpr bool operator!() const { return !value_; }

  // Get the level of the socket option.
  template <typename Protocol>
  constexpr int level(const Protocol&) const {
    return Level;
  }

  // Get the name of the socket option.
  template <typename Protocol>
  constexpr int name(const Protocol&) const {
    return Name;
  }

  // Get the address of the boolean data.
  template <typename Protocol>
  constexpr int* data(const Protocol&) {
    return &value_;
  }

  // Get the address of the boolean data.
  template <typename Protocol>
  constexpr const int* data(const Protocol&) const {
    return &value_;
  }

  // Get the size of the boolean data.
  template <typename Protocol>
  constexpr std::size_t size(const Protocol&) const {
    return sizeof(value_);
  }

 private:
  int value_;
};

// Helper template for implementing integer options.
template <int Level, int Name>
class integer {
 public:
  // Default constructor.
  constexpr integer() : value_(0) {}

  // Construct with a specific option value.
  explicit constexpr integer(int v) : value_(v) {}

  // Set the value of the int option.
  constexpr integer& operator=(int v) {
    value_ = v;
    return *this;
  }

  // Get the current value of the int option.
  constexpr int value() const { return value_; }

  // Get the level of the socket option.
  template <typename Protocol>
  constexpr int level(const Protocol&) const {
    return Level;
  }

  // Get the name of the socket option.
  template <typename Protocol>
  constexpr int name(const Protocol&) const {
    return Name;
  }

  // Get the address of the int data.
  template <typename Protocol>
  constexpr int* data(const Protocol&) {
    return &value_;
  }

  // Get the address of the int data.
  template <typename Protocol>
  constexpr const int* data(const Protocol&) const {
    return &value_;
  }

  // Get the size of the int data.
  template <typename Protocol>
  constexpr std::size_t size(const Protocol&) const {
    return sizeof(value_);
  }

 private:
  int value_;
};

// Helper template for implementing linger options.
template <int Level, int Name>
class linger {
 public:
  // Default constructor.
  constexpr linger() : value_{.l_onoff = 0, .l_linger = 0} {}

  // Construct with specific option values.
  constexpr linger(bool e, int t) {
    enabled(e);
    timeout(t);
  }

  // Set the value for whether linger is enabled.
  constexpr void enabled(bool value) { value_.l_onoff = value ? 1 : 0; }

  // Get the value for whether linger is enabled.
  constexpr bool enabled() const { return value_.l_onoff != 0; }

  // Set the value for the linger timeout.
  constexpr void timeout(int value) { value_.l_linger = value; }

  // Get the value for the linger timeout.
  constexpr int timeout() const { return static_cast<int>(value_.l_linger); }

  // Get the level of the socket option.
  template <typename Protocol>
  constexpr int level(const Protocol&) const {
    return Level;
  }

  // Get the name of the socket option.
  template <typename Protocol>
  constexpr int name(const Protocol&) const {
    return Name;
  }

  // Get the address of the linger data.
  template <typename Protocol>
  constexpr ::linger* data(const Protocol&) {
    return &value_;
  }

  // Get the address of the linger data.
  template <typename Protocol>
  constexpr const ::linger* data(const Protocol&) const {
    return &value_;
  }

  // Get the size of the linger data.
  template <typename Protocol>
  constexpr std::size_t size(const Protocol&) const {
    return sizeof(value_);
  }

 private:
  ::linger value_;
};

}  // namespace socket_option
}  // namespace net

#endif  // NET_SRC_SOCKET_OPTION_HPP_
