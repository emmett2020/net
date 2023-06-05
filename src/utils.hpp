#ifndef NET_UTILS_H_
#define NET_UTILS_H_

#include <algorithm>
#include <bit>
#include <ranges>

namespace net::utils {
// Currently (2023-04-22), C++23 support is not very good, so we provide a C++20 version of
// byteswap, when C++23 support, replace it with std::byteswap.
template <std::integral T>
constexpr T byteswap(T value) noexcept {
  static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
  auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  std::ranges::reverse(value_representation);
  return std::bit_cast<T>(value_representation);
}
}  // namespace net::utils

#endif