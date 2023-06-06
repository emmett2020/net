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
#ifndef EXECUTION_CONTEXT_HPP_
#define EXECUTION_CONTEXT_HPP_

namespace net {

// The base class for the io operation context.
// The context of epoll/io_uring etc can inherit from this class, and then the
// `basic_socket` can be constructed normally. Note that we chose `inheritance`
// over `template` to avoid providing an extra template parameter when
// constructing the `basic_socket`. Therefore, this class is used only as a
// convenience in constructing sockets and has no other unique features.
class execution_context {
 public:
  // Default constructor.
  constexpr execution_context() noexcept = default;

  // Move constructor.
  constexpr execution_context(execution_context&&) noexcept = delete;

  // Move assign.
  constexpr execution_context& operator=(execution_context&&) noexcept = delete;

  // Copy constructor.
  constexpr execution_context(const execution_context&) noexcept = default;

  // Copy assign.
  constexpr execution_context& operator=(const execution_context&) noexcept =
      default;

  // Destructor.
  constexpr ~execution_context() noexcept = default;
};

}  // namespace net

#endif  // EXECUTION_CONTEXT_HPP_
