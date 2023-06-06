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
#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <algorithm>
#include <bit>     // NOLINT
#include <ranges>  // NOLINT

namespace net::utils {
// Currently (2023-04-22), C++23 support is not very good, so we provide a C++20
// version of byteswap, when C++23 support, replace it with std::byteswap.
template <std::integral T>
constexpr T byteswap(T value) noexcept {
  static_assert(std::has_unique_object_representations_v<T>,
                "T may not have padding bits");
  auto value_representation =
      std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  std::ranges::reverse(value_representation);
  return std::bit_cast<T>(value_representation);
}
}  // namespace net::utils

#endif  // UTILS_HPP_
