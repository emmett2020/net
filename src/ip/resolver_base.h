#ifndef SRC_NET_IP_RESOLVER_BASE_H_
#define SRC_NET_IP_RESOLVER_BASE_H_

#include "socket_types.hpp"

namespace net {
namespace ip {

// The ResolverBase class is used as a base for the basic_resolver class
// templates to provide a common place to define the flag constants.
class ResolverBase {
 public:
  enum flags {
    canonical_name = AI_CANONNAME,
    passive = AI_PASSIVE,
    numeric_host = AI_NUMERICHOST,
    numeric_service = AI_NUMERICSERV,
    v4_mapped = AI_V4MAPPED,
    all_matching = AI_ALL,
    address_configured = AI_ADDRCONFIG
  };

  // Implement bitmask operations as shown in C++ Std [lib.bitmask.types].

  friend flags operator&(flags x, flags y) {
    return static_cast<flags>(static_cast<unsigned int>(x) & static_cast<unsigned int>(y));
  }

  friend flags operator|(flags x, flags y) {
    return static_cast<flags>(static_cast<unsigned int>(x) | static_cast<unsigned int>(y));
  }

  friend flags operator^(flags x, flags y) {
    return static_cast<flags>(static_cast<unsigned int>(x) ^ static_cast<unsigned int>(y));
  }

  friend flags operator~(flags x) { return static_cast<flags>(~static_cast<unsigned int>(x)); }

  friend flags& operator&=(flags& x, flags y) {
    x = x & y;
    return x;
  }

  friend flags& operator|=(flags& x, flags y) {
    x = x | y;
    return x;
  }

  friend flags& operator^=(flags& x, flags y) {
    x = x ^ y;
    return x;
  }

 protected:
  // Protected destructor to prevent deletion through this type.
  ~ResolverBase() {}
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_RESOLVER_BASE_H_
