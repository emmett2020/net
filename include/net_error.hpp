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
#ifndef NET_ERROR_HPP_
#define NET_ERROR_HPP_

#include <netdb.h>

#include <cerrno>
#include <initializer_list>
#include <string>

#include "status-code/quick_status_code_from_enum.hpp"
#include "status-code/system_code.hpp"

namespace net {
enum class network_errc : int { eof };
};  // namespace net

template <>
struct system_error2::quick_status_code_from_enum<net::network_errc>
    : public system_error2::quick_status_code_from_enum_defaults<
          net::network_errc> {
  static constexpr const auto domain_name = "network error";

  // Unique UUID for the enum.
  // PLEASE use https://www.random.org/cgi-bin/randbyte?nbytes=16&format=h
  static constexpr const auto domain_uuid =
      "{0768dead-0f2d-d0c6-14cb-6f82f353598e}";

  static const std::initializer_list<mapping>& value_mappings() {
    static const std::initializer_list<mapping> v = {
        {net::network_errc::eof, "end of file", {}},
    };
    return v;
  }

  template <class Base>
  struct mixin : Base {
    using Base::Base;
  };
};

// ADL discovered, must be in same namespace as AnotherCode
inline constexpr system_error2::quick_status_code_from_enum_code<
    net::network_errc>
status_code(net::network_errc ec) {
  return system_error2::quick_status_code_from_enum_code<net::network_errc>(ec);
}

namespace std {
template <>
struct is_error_code_enum<net::network_errc> {
  static const bool value = true;
};
}  // namespace std

namespace net {
class network_category : public std::error_category {
 public:
  constexpr const char* name() const noexcept override { return "net.error"; }

  std::string message(int value) const override {
    network_errc v = static_cast<network_errc>(value);
    if (v == network_errc::eof) {
      return "end of file";
    }

    // By default
    return "network error";
  }
};

inline const std::error_category& network_category() {
  static class network_category category;
  return category;
}

inline std::error_code make_error_code(const network_errc& e) {
  return {static_cast<int>(e), network_category()};
}

}  // namespace net

#endif  // NET_ERROR_HPP_
