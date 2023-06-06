#ifndef SRC_NET_IP_BASIC_RESOLVER_QUERY_H_
#define SRC_NET_IP_BASIC_RESOLVER_QUERY_H_

#include <string>

#include "../socket_option.hpp"
#include "resolver_query_base.h"

namespace net {
namespace ip {

// An query to be passed to a resolver.
template <typename InternetProtocol>
class BasicResolverQuery : public ResolverQueryBase {
 public:
  // Construct with specified service name for any protocol.
  BasicResolverQuery(const std::string& service,
                     ResolverQueryBase::flags resolve_flags = passive | address_configured)
      : hints_(), host_name_(), service_name_(service) {
    typename InternetProtocol::Endpoint endpoint;

    hints_.ai_flags = static_cast<int>(resolve_flags);
    hints_.ai_family = PF_UNSPEC;
    hints_.ai_socktype = endpoint.protocol().type();
    hints_.ai_protocol = endpoint.protocol().protocol();
    hints_.ai_addrlen = 0;
    hints_.ai_canonname = 0;
    hints_.ai_addr = 0;
    hints_.ai_next = 0;
  }

  // Construct with specified service name for a given protocol.
  BasicResolverQuery(const InternetProtocol& protocol, const std::string& service,
                     ResolverQueryBase::flags resolve_flags = passive | address_configured)
      : hints_(), host_name_(), service_name_(service) {
    hints_.ai_flags = static_cast<int>(resolve_flags);
    hints_.ai_family = protocol.family();
    hints_.ai_socktype = protocol.type();
    hints_.ai_protocol = protocol.protocol();
    hints_.ai_addrlen = 0;
    hints_.ai_canonname = 0;
    hints_.ai_addr = 0;
    hints_.ai_next = 0;
  }

  // Construct with specified host name and service name for any protocol.
  BasicResolverQuery(const std::string& host, const std::string& service,
                     ResolverQueryBase::flags resolve_flags = address_configured)
      : hints_(), host_name_(host), service_name_(service) {
    typename InternetProtocol::endpoint endpoint;
    hints_.ai_flags = static_cast<int>(resolve_flags);
    hints_.ai_family = AF_UNSPEC;
    hints_.ai_socktype = endpoint.protocol().type();
    hints_.ai_protocol = endpoint.protocol().protocol();
    hints_.ai_addrlen = 0;
    hints_.ai_canonname = 0;
    hints_.ai_addr = 0;
    hints_.ai_next = 0;
  }

  // Construct with specified host name and service name for a given protocol.
  BasicResolverQuery(const InternetProtocol& protocol, const std::string& host,
                     const std::string& service,
                     ResolverQueryBase::flags resolve_flags = address_configured)
      : hints_(), host_name_(host), service_name_(service) {
    hints_.ai_flags = static_cast<int>(resolve_flags);
    hints_.ai_family = protocol.family();
    hints_.ai_socktype = protocol.type();
    hints_.ai_protocol = protocol.protocol();
    hints_.ai_addrlen = 0;
    hints_.ai_canonname = 0;
    hints_.ai_addr = 0;
    hints_.ai_next = 0;
  }

  // Get the hints associated with the query.
  const addrinfo& hints() const { return hints_; }

  // Get the host name associated with the query.
  std::string hostname() const { return host_name_; }

  // Get the service name associated with the query.
  std::string serviceName() const { return service_name_; }

 private:
  addrinfo hints_;
  std::string host_name_;
  std::string service_name_;
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_BASIC_RESOLVER_QUERY_H_
