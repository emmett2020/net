#ifndef SRC_NET_IP_BASIC_RESOLVER_ITERATOR_H_
#define SRC_NET_IP_BASIC_RESOLVER_ITERATOR_H_

#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "../socket_option.hpp"
#include "basic_resolver_entry.h"
#include "socket_types.hpp"

namespace net {
namespace ip {

// An iterator over the entries produced by a resolver.
template <typename InternetProtocol>
class BasicResolverIterator {
 public:
  // The type used for the distance between two iterators.
  using difference_type = std::ptrdiff_t;

  // The type of the value pointed to by the iterator.
  using value_type = BasicResolverEntry<InternetProtocol>;

  // The type of the result of applying operator->() to the iterator.
  using pointer = const BasicResolverEntry<InternetProtocol>*;

  // The type of the result of applying operator*() to the iterator.
  using reference = const BasicResolverEntry<InternetProtocol>&;

  // The iterator category.
  using iterator_category = std::forward_iterator_tag;

  // Default constructor creates an end iterator.
  BasicResolverIterator() : index_(0) {}

  // Copy constructor.
  BasicResolverIterator(const BasicResolverIterator& other)
      : values_(other.values_), index_(other.index_) {}

  // Move constructor.
  BasicResolverIterator(BasicResolverIterator&& other)
      : values_(std::move(other.values_)), index_(other.index_) {
    other.index_ = 0;
  }

  // Assignment operator.
  BasicResolverIterator& operator=(const BasicResolverIterator& other) {
    values_ = other.values_;
    index_ = other.index_;
    return *this;
  }

  // Move-assignment operator.
  BasicResolverIterator& operator=(BasicResolverIterator&& other) {
    if (this != &other) {
      values_ = std::move(other.values_);
      index_ = other.index_;
      other.index_ = 0;
    }

    return *this;
  }

  // Dereference an iterator.
  const BasicResolverEntry<InternetProtocol>& operator*() const { return dereference(); }

  // Dereference an iterator.
  const BasicResolverEntry<InternetProtocol>* operator->() const { return &dereference(); }

  // Increment operator (prefix).
  BasicResolverIterator& operator++() {
    increment();
    return *this;
  }

  // Increment operator (postfix).
  BasicResolverIterator operator++(int) {
    BasicResolverIterator tmp(*this);
    ++*this;
    return tmp;
  }

  // Test two iterators for equality.
  friend bool operator==(const BasicResolverIterator& a, const BasicResolverIterator& b) {
    return a.equal(b);
  }

  // Test two iterators for inequality.
  friend bool operator!=(const BasicResolverIterator& a, const BasicResolverIterator& b) {
    return !a.equal(b);
  }

 protected:
  void increment() {
    if (++index_ == values_->size()) {
      // Reset state to match a default constructed end iterator.
      values_.reset();
      index_ = 0;
    }
  }

  bool equal(const BasicResolverIterator& other) const {
    if (!values_ && !other.values_) {
      return true;
    }
    if (values_ != other.values_) {
      return false;
    }
    return index_ == other.index_;
  }

  const BasicResolverEntry<InternetProtocol>& dereference() const { return (*values_)[index_]; }

  using values_type = std::vector<BasicResolverEntry<InternetProtocol>>;
  using values_ptr_type = std::shared_ptr<values_type>;
  values_ptr_type values_;
  std::size_t index_;
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_BASIC_RESOLVER_ITERATOR_H_
