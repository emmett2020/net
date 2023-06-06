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
#ifndef SRC_NET_IP_ADDRESS_V4_RANGE_HPP
#define SRC_NET_IP_ADDRESS_V4_RANGE_HPP

#include "address_v4.h"
#include "address_v4_iterator.hpp"

namespace net {
namespace ip {

template <typename>
class BasicAddressRange;

// Represents a range of IPv4 addresses.
template <>
class BasicAddressRange<AddressV4> {
 public:
  // The type of an iterator that points into the range.
  using iterator = BasicAddressIterator<AddressV4>;

  // Construct an empty range.
  BasicAddressRange() : begin_(AddressV4()), end_(AddressV4()) {}

  // Construct an range that represents the given range of addresses.
  explicit BasicAddressRange(const iterator& first, const iterator& last)
      : begin_(first), end_(last) {}

  BasicAddressRange(const BasicAddressRange& other) noexcept
      : begin_(other.begin_), end_(other.end_) {}

  BasicAddressRange(BasicAddressRange&& other) noexcept
      : begin_(std::move(other.begin_)), end_(std::move(other.end_)) {}

  BasicAddressRange& operator=(const BasicAddressRange& other) noexcept {
    begin_ = other.begin_;
    end_ = other.end_;
    return *this;
  }

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
  bool empty() const noexcept { return size() == 0; }

  // Return the size of the range.
  std::size_t size() const noexcept {
    return end_->to_uint() - begin_->to_uint();
  }

  // Find an address in the range.
  iterator find(const AddressV4& addr) const noexcept {
    return addr >= *begin_ && addr < *end_ ? iterator(addr) : end_;
  }

 private:
  iterator begin_;
  iterator end_;
};

// Represents a range of IPv4 addresses.
using AddressV4Range = BasicAddressRange<AddressV4>;
}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_ADDRESS_V4_RANGE_H_
