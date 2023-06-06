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
#ifndef SRC_NET_IP_ADDRESS_V6_RANGE_H_
#define SRC_NET_IP_ADDRESS_V6_RANGE_H_

#include "address_v6_iterator.hpp"

namespace net {
namespace ip {

template <typename>
class BasicAddressRange;

// Represents a range of IPv6 addresses.
template <>
class BasicAddressRange<AddressV6> {
 public:
  // The type of an iterator that points into the range.
  using iterator = BasicAddressIterator<AddressV6>;

  // Construct an empty range.
  BasicAddressRange() noexcept : begin_(AddressV6()), end_(AddressV6()) {}

  // Construct an range that represents the given range of addresses.
  explicit BasicAddressRange(const iterator& first,
                             const iterator& last) noexcept
      : begin_(first), end_(last) {}

  // Copy constructor.
  BasicAddressRange(const BasicAddressRange& other) noexcept
      : begin_(other.begin_), end_(other.end_) {}

  // Move constructor.
  BasicAddressRange(BasicAddressRange&& other) noexcept
      : begin_(std::move(other.begin_)), end_(std::move(other.end_)) {}

  // Assignment operator.
  BasicAddressRange& operator=(const BasicAddressRange& other) noexcept {
    begin_ = other.begin_;
    end_ = other.end_;
    return *this;
  }

  // Move assignment operator.
  BasicAddressRange& operator=(BasicAddressRange&& other) noexcept {
    begin_ = std::move(other.begin_);
    end_ = std::move(other.end_);
    return *this;
  }

  // Obtain an iterator that points to the start of the range.
  iterator begin() const noexcept { return begin_; }

  // Obtain an iterator that points to the end of the range.
  iterator end() const noexcept { return end_; }

  // Determine whether the range is empty.
  bool empty() const noexcept { return begin_ == end_; }

  // Find an address in the range.
  iterator find(const AddressV6& addr) const noexcept {
    return addr >= *begin_ && addr < *end_ ? iterator(addr) : end_;
  }

 private:
  iterator begin_;
  iterator end_;
};

// Represents a range of IPv6 addresses.
typedef BasicAddressRange<AddressV6> address_v6_range;

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_ADDRESS_V6_RANGE_H_
