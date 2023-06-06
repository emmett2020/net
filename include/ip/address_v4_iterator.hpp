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
#ifndef SRC_NET_IP_ADDRESS_V4_ITERATOR_H_
#define SRC_NET_IP_ADDRESS_V4_ITERATOR_H_

#include "address_v4.h"

namespace net {
namespace ip {

template <typename>
class BasicAddressIterator;

// An input iterator that can be used for traversing IPv4 addresses.

template <>
class BasicAddressIterator<AddressV4> {
 public:
  // The type of the elements pointed to by the iterator.
  using value_type = AddressV4;

  // Distance between two iterators.
  using difference_type = std::ptrdiff_t;

  // The type of a pointer to an element pointed to by the iterator.
  using pointer = const AddressV4*;

  // The type of a reference to an element pointed to by the iterator.
  using reference = const AddressV4&;

  // Denotes that the iterator satisfies the input iterator requirements.
  using iterator_category = std::input_iterator_tag;

  // Construct an iterator that points to the specified address.
  BasicAddressIterator(const AddressV4& addr) noexcept : address_(addr) {}

  // Copy constructor.
  BasicAddressIterator(const BasicAddressIterator& other) noexcept
      : address_(other.address_) {}

  // Move constructor.
  BasicAddressIterator(BasicAddressIterator&& other) noexcept
      : address_(std::move(other.address_)) {}

  // Assignment operator.
  BasicAddressIterator& operator=(const BasicAddressIterator& other) noexcept {
    address_ = other.address_;
    return *this;
  }

  // Move assignment operator.
  BasicAddressIterator& operator=(BasicAddressIterator&& other) noexcept {
    address_ = std::move(other.address_);
    return *this;
  }

  // Dereference the iterator.
  const AddressV4& operator*() const noexcept { return address_; }

  // Dereference the iterator.
  const AddressV4* operator->() const noexcept { return &address_; }

  // Pre-increment operator.
  BasicAddressIterator& operator++() noexcept {
    address_ = AddressV4((address_.to_uint() + 1) & 0xFFFFFFFF);
    return *this;
  }

  // Post-increment operator.
  BasicAddressIterator operator++(int) noexcept {
    BasicAddressIterator tmp(*this);
    ++*this;
    return tmp;
  }

  // Pre-decrement operator.
  BasicAddressIterator& operator--() noexcept {
    address_ = AddressV4((address_.to_uint() - 1) & 0xFFFFFFFF);
    return *this;
  }

  // Post-decrement operator.
  BasicAddressIterator operator--(int) {
    BasicAddressIterator tmp(*this);
    --*this;
    return tmp;
  }

  // Compare two addresses for equality.
  friend bool operator==(const BasicAddressIterator& a,
                         const BasicAddressIterator& b) {
    return a.address_ == b.address_;
  }

  // Compare two addresses for inequality.
  friend bool operator!=(const BasicAddressIterator& a,
                         const BasicAddressIterator& b) {
    return a.address_ != b.address_;
  }

 private:
  AddressV4 address_;
};

// An input iterator that can be used for traversing IPv4 addresses.
using AddressV4Iterator = BasicAddressIterator<AddressV4>;

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_ADDRESS_V4_ITERATOR_H_
