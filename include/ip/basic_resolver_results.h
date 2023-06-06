#ifndef SRC_NET_BASIC_RESOLVER_RESULTS_H_
#define SRC_NET_BASIC_RESOLVER_RESULTS_H_

#include <cstddef>
#include <cstring>

#include "../socket_ops.h"
#include "basic_resolver_iterator.h"
#include "socket_types.hpp"

namespace net {
namespace ip {

// A range of entries produced by a resolver.
template <typename InternetProtocol>
class BasicResolverResults : public BasicResolverIterator<InternetProtocol> {
 public:
  // The endpoint type associated with the results.
  using Endpoint = typename InternetProtocol::Endpoint;

  // The type of a value in the results range.
  using value_type = BasicResolverEntry<InternetProtocol>;

  // The type of a const reference to a value in the range.
  using const_reference = const value_type&;

  // The type of a non-const reference to a value in the range.
  using reference = value_type&;

  // The type of an iterator into the range.
  using const_iterator = BasicResolverIterator<InternetProtocol>;

  // The type of an iterator into the range.
  using iterator = const_iterator;

  // Type used to represent the distance between two iterators in the range.
  using difference_type = std::ptrdiff_t;

  // Type used to represent a count of the elements in the range.
  using size_type = std::size_t;

  // Default constructor creates an empty range.
  BasicResolverResults() {}

  // Copy constructor.
  BasicResolverResults(const BasicResolverResults& other)
      : BasicResolverIterator<InternetProtocol>(other) {}

  // Move constructor.
  BasicResolverResults(BasicResolverResults&& other)
      : BasicResolverIterator<InternetProtocol>(std::move(other)) {}

  // Assignment operator.
  BasicResolverResults& operator=(const BasicResolverResults& other) {
    BasicResolverIterator<InternetProtocol>::operator=(other);
    return *this;
  }

  // Move-assignment operator.
  BasicResolverResults& operator=(BasicResolverResults&& other) {
    BasicResolverIterator<InternetProtocol>::operator=(std::move(other));
    return *this;
  }

  // Create results from an addrinfo list returned by getaddrinfo.
  static BasicResolverResults create(addrinfo* address_info, const std::string& host_name,
                                     const std::string& service_name) {
    BasicResolverResults results;
    if (!address_info) {
      return results;
    }

    std::string actual_host_name = host_name;
    if (address_info->ai_canonname) {
      actual_host_name = address_info->ai_canonname;
    }

    results.values_.reset(new values_type);

    while (address_info) {
      if (address_info->ai_family == AF_INET || address_info->ai_family == AF_INET6) {
        Endpoint endpoint;
        endpoint.resize(static_cast<std::size_t>(address_info->ai_addrlen));
        ::memcpy(endpoint.data(), address_info->ai_addr, address_info->ai_addrlen);
        results.values_->push_back(
            BasicResolverEntry<InternetProtocol>(endpoint, actual_host_name, service_name));
      }
      address_info = address_info->ai_next;
    }

    return results;
  }

  // Create results from an endpoint, host name and service name.
  static BasicResolverResults create(const Endpoint& endpoint, const std::string& host_name,
                                     const std::string& service_name) {
    BasicResolverResults results;
    results.values_.reset(new values_type);
    results.values_->push_back(
        BasicResolverEntry<InternetProtocol>(endpoint, host_name, service_name));
    return results;
  }

  // Create results from a sequence of endpoints, host and service name.
  template <typename EndpointIterator>
  static BasicResolverResults create(EndpointIterator begin, EndpointIterator end,
                                     const std::string& host_name,
                                     const std::string& service_name) {
    BasicResolverResults results;
    if (begin != end) {
      results.values_.reset(new values_type);
      for (EndpointIterator ep_iter = begin; ep_iter != end; ++ep_iter) {
        results.values_->push_back(
            BasicResolverEntry<InternetProtocol>(*ep_iter, host_name, service_name));
      }
    }
    return results;
  }

  // Get the number of entries in the results range.
  size_type size() const noexcept { return this->values_ ? this->values_->size() : 0; }

  // Get the maximum number of entries permitted in a results range.
  size_type maxSize() const noexcept {
    return this->values_ ? this->values_->maxSize() : values_type().maxSize();
  }

  // Determine whether the results range is empty.
  bool empty() const noexcept { return this->values_ ? this->values_->empty() : true; }

  // Obtain a begin iterator for the results range.
  const_iterator begin() const {
    BasicResolverResults tmp(*this);
    tmp.index_ = 0;
    return std::move(tmp);
  }

  // Obtain an end iterator for the results range.
  const_iterator end() const { return const_iterator(); }

  // Obtain a begin iterator for the results range.
  const_iterator cbegin() const { return begin(); }

  // Obtain an end iterator for the results range.
  const_iterator cend() const { return end(); }

  // Swap the results range with another.
  void swap(BasicResolverResults& that) noexcept {
    if (this != &that) {
      this->values_.swap(that.values_);
      std::size_t index = this->index_;
      this->index_ = that.index_;
      that.index_ = index;
    }
  }

  // Test two iterators for equality.
  friend bool operator==(const BasicResolverResults& a, const BasicResolverResults& b) {
    return a.equal(b);
  }

  // Test two iterators for inequality.
  friend bool operator!=(const BasicResolverResults& a, const BasicResolverResults& b) {
    return !a.equal(b);
  }

 private:
  using values_type = std::vector<BasicResolverEntry<InternetProtocol>>;
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_BASIC_RESOLVER_RESULTS_H_
