

// #include "host_name.h"

// #include "../socket_ops.h"

// namespace net {
// namespace ip {

// std::string hostname() {
//   char name[1024];
//   std::error_code ec;
//   if (socket_ops::gethostname(name, sizeof(name), ec) != 0) {
//     // TODO: 加上throw error
//     // asio::detail::throw_error(ec);
//     return std::string();
//   }
//   return std::string(name);
// }

// std::string hostname(std::error_code& ec) {
//   char name[1024];
//   if (socket_ops::gethostname(name, sizeof(name), ec) != 0) {
//     return std::string();
//   }
//   return std::string(name);
// }

// }  // namespace ip
// }  // namespace net
