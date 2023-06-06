

#ifndef SRC_NET_IP_HOST_NAME_H_
#define SRC_NET_IP_HOST_NAME_H_

#include <string>
#include <system_error>

#include "../error.h"

namespace net {
namespace ip {

// Get the current host name.
std::string hostname();

// Get the current host name.
std::string hostname(std::error_code& ec);

}  // namespace ip
}  // namespace net

#endif  // SRC_NET_IP_HOST_NAME_H_
