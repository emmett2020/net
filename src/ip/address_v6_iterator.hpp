#ifndef SRC_NET_IP_ADDRESS_V6_ITERATOR_H_
#define SRC_NET_IP_ADDRESS_V6_ITERATOR_H_

#include "address_v6.h"

namespace net {
namespace ip {

template <typename>
class BasicAddressIterator;

// An input iterator that can be used for traversing IPv6 addresses.
template <>
class BasicAddressIterator<AddressV6> {
 public:
  // The type of the elements pointed to by the iterator.
  using value_type = AddressV6;

  // Distance between two iterators.
  using difference_type = std::ptrdiff_t;

  // The type of a pointer to an element pointed to by the iterator.
  using pointer = const AddressV6*;

  // The type of a reference to an element pointed to by the iterator.
  using reference = const AddressV6&;

  // Denotes that the iterator satisfies the input iterator requirements.
  using iterator_category = std::input_iterator_tag;

  // Construct an iterator that points to the specified address.
  BasicAddressIterator(const AddressV6& addr) noexcept : address_(addr) {}

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
  const AddressV6& operator*() const noexcept { return address_; }

  // Dereference the iterator.
  const AddressV6* operator->() const noexcept { return &address_; }

  // Pre-increment operator.
  BasicAddressIterator& operator++() noexcept {
    for (int i = 15; i >= 0; --i) {
      if (address_.addr_.s6_addr[i] < 0xFF) {
        ++address_.addr_.s6_addr[i];
        break;
      }

      address_.addr_.s6_addr[i] = 0;
    }

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
    for (int i = 15; i >= 0; --i) {
      if (address_.addr_.s6_addr[i] > 0) {
        --address_.addr_.s6_addr[i];
        break;
      }

      address_.addr_.s6_addr[i] = 0xFF;
    }

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
  AddressV6 address_;
};

// An input iterator that can be used for traversing IPv6 addresses.
typedef BasicAddressIterator<AddressV6> AddressV6Iterator;

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_ADDRESS_V6_ITERATOR_H_
