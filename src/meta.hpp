#ifndef NET_SRC_META_HPP_
#define NET_SRC_META_HPP_

#include <concepts>
#include <type_traits>

#include "status-code/system_code.hpp"

namespace net {
template <typename Protocol>
concept transport_protocol = requires(Protocol& proto) {
                               typename Protocol::endpoint;
                               typename Protocol::socket;
                               { Protocol::v4() } -> std::same_as<Protocol>;
                               { Protocol::v6() } -> std::same_as<Protocol>;
                               { proto.type() } -> std::same_as<int>;
                               { proto.protocol() } -> std::same_as<int>;
                               { proto.family() } -> std::same_as<int>;
                               { proto == proto } -> std::same_as<bool>;
                               { proto != proto } -> std::same_as<bool>;
                             };

template <typename Socket>
concept socket = requires(Socket& socket) {
                   typename Socket::native_handle_type;
                   typename Socket::protocol_type;
                   typename Socket::endpoint_type;
                   typename Socket::socket_type;
                   typename Socket::socket_state;
                   typename Socket::context_type;
                   { socket.context() } -> std::same_as<typename Socket::context_type>;
                   { socket.native_handle() } -> std::same_as<typename Socket::native_handle_type>;
                   { socket.protocol() } -> std::same_as<typename Socket::protocol_type>;
                   { socket.state() } -> std::same_as<typename Socket::socket_state>;
                   {
                     socket.open(typename Socket::protocol_type{})
                     } -> std::same_as<system_error2::system_code>;
                   { socket.is_open() } -> std::same_as<bool>;
                 };

template <typename Endpoint>
concept endpoint = requires(Endpoint& ep) { typename Endpoint::protocol_type; };

}  // namespace net

#endif  // NET_SRC_META_HPP_