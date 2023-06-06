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
#ifndef IP_ADDRESS_V4_HPP_
#define IP_ADDRESS_V4_HPP_

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <string>

#include "ip/socket_types.hpp"
#include "utils.hpp"

namespace net {
namespace ip {
// Implements IP version 4 style addresses.
class address_v4 {
 public:
  // The unsigned integer type corresponding to the IP address.
  using uint_type = uint_least32_t;

  // The array type corresponding to the IP address.
  struct bytes_type : std::array<unsigned char, 4> {
    template <typename... T>
    explicit constexpr bytes_type(T... t)
        : std::array<unsigned char, 4>{{static_cast<unsigned char>(t)...}} {}
  };

  // Default constructor.
  constexpr address_v4() noexcept : bytes_() {}

  // Construct an address from bytes in network byte order.
  explicit constexpr address_v4(const bytes_type& bytes) : bytes_(bytes) {}

  // Construct an address from an unsigned integer in host byte order.
  explicit constexpr address_v4(uint_type addr) {
    if constexpr (std::endian::native == std::endian::big) {
      bytes_ = std::bit_cast<bytes_type>(addr);
    } else {
      bytes_ = std::bit_cast<bytes_type>(utils::byteswap(addr));
    }
  }

  // Copy constructor.
  constexpr address_v4(const address_v4& other) noexcept = default;

  // Move constructor.
  constexpr address_v4(address_v4&& other) noexcept = default;

  // Assign from another address.
  constexpr address_v4& operator=(const address_v4& other) noexcept = default;

  // Move assign from another address.
  constexpr address_v4& operator=(address_v4&& other) noexcept = default;

  // Compares two addresses in host byte order.
  constexpr std::strong_ordering operator<=>(const address_v4& other) const {
    return this->bytes_ == other.bytes_  ? std::strong_ordering::equal
           : this->bytes_ < other.bytes_ ? std::strong_ordering::less
                                         : std::strong_ordering::greater;
  }

  // Get the address in bytes, in network byte order.
  constexpr bytes_type to_bytes() const noexcept { return bytes_; }

  // Get the address as an unsigned integer in host byte order
  constexpr uint_type to_uint() const noexcept {
    if constexpr (std::endian::native == std::endian::big) {
      return std::bit_cast<uint_type>(bytes_);
    } else {
      return utils::byteswap(std::bit_cast<uint_type>(bytes_));
    }
  }

  std::string to_string() const {
    char src[max_address_v4_string_len];
    ::snprintf(src, sizeof(src), "%d.%d.%d.%d",
               static_cast<int>(bytes_[0]),  //
               static_cast<int>(bytes_[1]),  //
               static_cast<int>(bytes_[2]),  //
               static_cast<int>(bytes_[3]));
    return src;
  }

  // Determine whether the address is a loopback address. which corresponds to
  // the address range 127.0.0.0/8
  constexpr bool is_loopback() const {
    return (to_uint() & 0xFF000000) == 0x7F000000;
  }

  // Determine whether the address is 0.0.0.0
  constexpr bool is_unspecified() const { return to_uint() == 0; }

  // Determine whether the address is a multicast address. which corresponds to
  // the address range 224.0.0.0/4
  constexpr bool is_multicast() const {
    return (to_uint() & 0xF0000000) == 0xE0000000;
  }

  // Compare two addresses for equality.
  friend constexpr bool operator==(const address_v4& a1,
                                   const address_v4& a2) noexcept = default;

  // Obtain an object that represents any address. Usually 0.0.0.0
  static constexpr address_v4 any() noexcept { return address_v4{}; }

  // Obtain an object that represents the loopback address. Usually 127.0.0.1
  static constexpr address_v4 loopback() noexcept {
    return address_v4(0x7F000001);
  }

  // Obtain an object that represents the broadcast address. Usually
  // 255.255.255.255
  static constexpr address_v4 broadcast() noexcept {
    return address_v4(0xFFFFFFFF);
  }

  // Obtain associated address. port and addr in storage should be saved in
  // network order.
  socklen_t native_address(::sockaddr_storage* storage, port_type port) const {
    ::sockaddr_in addr{.sin_family = AF_INET,
                       .sin_addr = std::bit_cast<::in_addr>(bytes_)};
    if constexpr (std::endian::native == std::endian::big) {
      addr.sin_port = port;
    } else {
      addr.sin_port = utils::byteswap(port);
    }
    ::memcpy(storage, &addr, sizeof addr);
    return sizeof addr;
  }

 private:
  // The underlying IPv4 address. It's network order.
  bytes_type bytes_;
};

// Create an IPv4 address from raw bytes in network order.
inline constexpr address_v4 make_address_v4(
    const address_v4::bytes_type& bytes) noexcept {
  return address_v4(bytes);
}

// Create an IPv4 address from an unsigned integer in host byte order.
inline constexpr address_v4 make_address_v4(
    const address_v4::uint_type addr) noexcept {
  return address_v4(addr);
}

// Create an IPv4 address from an IP address string in dotted decimal form.
// Bytes is in network order.
inline address_v4 make_address_v4(const char* addr) noexcept {
  address_v4::bytes_type bytes;
  return ::inet_pton(AF_INET, addr, &bytes[0]) > 0 ? address_v4(bytes)
                                                   : address_v4{};
}

// Create an IPv4 address from an IP address string in dotted decimal form.
inline address_v4 make_address_v4(const std::string& str) noexcept {
  return make_address_v4(str.c_str());
}

// Create an IPv4 address from an IP address string in dotted decimal form.
inline address_v4 make_address_v4(std::string_view str) noexcept {
  return make_address_v4(static_cast<std::string>(str));
}

};  // namespace ip
}  // namespace net

template <>
struct std::hash<net::ip::address_v4> {
  std::size_t operator()(const net::ip::address_v4& addr) const noexcept {
    return std::hash<unsigned int>()(addr.to_uint());
  };
};

#endif  // IP_ADDRESS_V4_HPP_
