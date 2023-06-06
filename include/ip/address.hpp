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
#ifndef IP_ADDRESS_HPP_
#define IP_ADDRESS_HPP_

#include <string>
#include <variant>

#include "ip/address_v4.hpp"
#include "ip/address_v6.hpp"

namespace net {
namespace ip {
// Implements version-independent IP addresses.
class address {
 public:
  // The IPv4 address index of address variant.
  static constexpr uint32_t v6_index{0};

  // The IPv6 address index of address variant.
  static constexpr uint32_t v4_index{1};

  // This variant stores either ipv4 or ipv6.
  using address_variant = std::variant<address_v6, address_v4>;

  // Default constructor.
  constexpr address() noexcept = default;

  // Construct an address from an IPv4 address.
  explicit constexpr address(const address_v4& addr) noexcept
      : address_(addr) {}

  // Construct an address from an IPv6 address.
  explicit constexpr address(const address_v6& addr) noexcept
      : address_(addr) {}

  // Copy constructor.
  constexpr address(const address& other) noexcept = default;

  // Move constructor.
  constexpr address(address&& other) noexcept = default;

  // Assign from another address.
  constexpr address& operator=(const address& other) noexcept = default;

  // Move-assign from another address.
  constexpr address& operator=(address&& other) noexcept = default;

  // Assign from an IPv4 address.
  constexpr address& operator=(const address_v4& addr) noexcept {
    address_ = addr;
    return *this;
  }

  // Assign from an IPv6 address.
  constexpr address& operator=(const address_v6& addr) noexcept {
    address_ = addr;
    return *this;
  }

  // Get whether the address is an IP version 4 address.
  constexpr bool is_v4() const noexcept { return address_.index() == v4_index; }

  // Get whether the address is an IP version 6 address.
  constexpr bool is_v6() const noexcept { return address_.index() == v6_index; }

  // Get the address as an IP version 4 address.
  constexpr address_v4 to_v4() const { return std::get<v4_index>(address_); }

  // Get the address as an IP version 6 address.
  constexpr address_v6 to_v6() const { return std::get<v6_index>(address_); }

  // Get the address as a string.
  std::string to_string() const {
    return std::visit([](auto&& addr) { return addr.to_string(); }, address_);
  }

  // Determine whether the address is a loopback address.
  constexpr bool is_loopback() const noexcept {
    return std::visit([](auto&& addr) { return addr.is_loopback(); }, address_);
  }

  // Determine whether the address is unspecified.
  constexpr bool is_unspecified() const noexcept {
    return std::visit([](auto&& addr) { return addr.is_unspecified(); },
                      address_);
  }

  // Determine whether the address is a multicast address.
  constexpr bool is_multicast() const noexcept {
    return std::visit([](auto&& addr) { return addr.is_multicast(); },
                      address_);
  }

  // Compare two addresses for equality.
  constexpr friend bool operator==(const address& a1,
                                   const address& a2) noexcept = default;

  // Compare addresses for ordering.
  constexpr std::partial_ordering operator<=>(
      address const& other) const noexcept {
    return address_.index() == other.address_.index()
               ? (address_.index() == 0 ? ::std::get<0>(this->address_) <=>
                                              ::std::get<0>(other.address_)
                                        : ::std::get<1>(this->address_) <=>
                                              ::std::get<1>(other.address_))
               : ::std::partial_ordering::unordered;
  }

  // Get the Linux-specific form of address.
  inline ::socklen_t native_address(::sockaddr_storage* storage,
                                    port_type port) const {
    return ::std::visit(
        [storage, port](auto&& addr) {
          return addr.native_address(storage, port);
        },
        address_);
  }

 private:
  // The underlying address.
  address_variant address_;
};

// Create an address from an IPv4 address string in dotted decimal form,
// or from an IPv6 address in hexadecimal notation.
inline constexpr address make_address(const char* str) noexcept {
  // TODO(xiaoming) Implement
  return {};
}

// Create an address from an IPv4 address string in dotted decimal form,
// or from an IPv6 address in hexadecimal notation.
inline address make_address(const std::string& str) {
  return make_address(str.c_str());
}

// Create an address from an IPv4 address string in dotted decimal form,
// or from an IPv6 address in hexadecimal notation.
inline address make_address(std::string_view str) {
  return make_address(static_cast<std::string>(str));
}

}  // namespace ip
}  // namespace net

template <>
struct std::hash<net::ip::address> {
  std::size_t operator()(const net::ip::address& addr) const noexcept {
    return addr.is_v4() ? std::hash<net::ip::address_v4>()(addr.to_v4())
                        : std::hash<net::ip::address_v6>()(addr.to_v6());
  }
};

#endif  // IP_ADDRESS_HPP_
