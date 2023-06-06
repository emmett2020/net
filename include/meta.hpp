/*
 * Copyright (c) 2023 Runner-2019
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
                   {
                     socket.context()
                     } -> std::same_as<typename Socket::context_type>;
                   {
                     socket.native_handle()
                     } -> std::same_as<typename Socket::native_handle_type>;
                   {
                     socket.protocol()
                     } -> std::same_as<typename Socket::protocol_type>;
                   {
                     socket.state()
                     } -> std::same_as<typename Socket::socket_state>;
                   {
                     socket.open(typename Socket::protocol_type{})
                     } -> std::same_as<system_error2::system_code>;
                   { socket.is_open() } -> std::same_as<bool>;
                 };

template <typename Endpoint>
concept endpoint = requires(Endpoint& ep) { typename Endpoint::protocol_type; };

}  // namespace net

#endif  // NET_SRC_META_HPP_