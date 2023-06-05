

#ifndef SRC_NET_IP_RESOLVER_QUERY_BASE_H_
#define SRC_NET_IP_RESOLVER_QUERY_BASE_H_

#include "resolver_base.h"

namespace net {
namespace ip {

// The ResolverQueryBase class is used as a base for the
// basic_resolver_query class templates to provide a common place to define
// the flag constants.
class ResolverQueryBase : public ResolverBase {
 protected:
  // Protected destructor to prevent deletion through this type.
  ~ResolverQueryBase() {}
};

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_RESOLVER_QUERY_BASE_H_
