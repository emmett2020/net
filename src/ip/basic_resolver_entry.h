#ifndef SRC_NET_IP_BASIC_RESOLVER_ENTRY_H_
#define SRC_NET_IP_BASIC_RESOLVER_ENTRY_H_

#include <string>
#include <string_view>

namespace net {
namespace ip {

// An entry produced by a resolver.
template <typename InternetProtocol>
class BasicResolverEntry {
 public:
  // The endpoint type associated with the endpoint entry.
  using Endpoint = typename InternetProtocol::Endpoint;

  // Default constructor.
  BasicResolverEntry() {}

  // Construct with specified endpoint, host name and service name.
  BasicResolverEntry(const Endpoint& ep, std::string_view host, std::string_view service)
      : endpoint_(ep),
        host_name_(static_cast<std::string>(host)),
        service_name_(static_cast<std::string>(service)) {}

  // Get the endpoint associated with the entry.
  Endpoint endpoint() const { return endpoint_; }

  // Convert to the endpoint associated with the entry.
  operator Endpoint() const { return endpoint_; }

  // Get the host name associated with the entry.
  std::string hostname() const { return host_name_; }

  // Get the host name associated with the entry.
  template <class Allocator>
  std::basic_string<char, std::char_traits<char>, Allocator> hostname(
      const Allocator& alloc = Allocator()) const {
    return std::basic_string<char, std::char_traits<char>, Allocator>(host_name_.c_str(), alloc);
  }

  // Get the service name associated with the entry.
  std::string serviceName() const { return service_name_; }

  // Get the service name associated with the entry.
  template <class Allocator>
  std::basic_string<char, std::char_traits<char>, Allocator> serviceName(
      const Allocator& alloc = Allocator()) const {
    return std::basic_string<char, std::char_traits<char>, Allocator>(service_name_.c_str(), alloc);
  }

 private:
  Endpoint endpoint_;
  std::string host_name_;
  std::string service_name_;
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_BASIC_RESOLVER_ENTRY_H_
