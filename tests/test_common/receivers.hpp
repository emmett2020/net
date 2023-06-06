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
#ifndef TESTS_TEST_COMMON_RECEIVERS_HPP_
#define TESTS_TEST_COMMON_RECEIVERS_HPP_

#include "catch2/catch_all.hpp"
#include "stdexec.hpp"

namespace ex = stdexec;
using ex::get_env_t;
using ex::set_error_t;
using ex::set_stopped_t;
using ex::set_value_t;

struct empty_receiver {
  using is_receiver = void;
  using __t = empty_receiver;
  using __id = empty_receiver;

  friend void tag_invoke(set_value_t, empty_receiver&&) noexcept {}

  friend void tag_invoke(set_stopped_t, empty_receiver&&) noexcept {}

  friend void tag_invoke(set_error_t, empty_receiver&&,
                         std::exception_ptr) noexcept {}

  friend ex::empty_env tag_invoke(get_env_t, const empty_receiver&) noexcept {
    return {};
  }
};

struct universal_receiver {
  using is_receiver = void;
  using __t = universal_receiver;
  using __id = universal_receiver;

  friend void tag_invoke(set_value_t, universal_receiver&&, auto...) noexcept {}

  friend void tag_invoke(set_stopped_t, universal_receiver&&) noexcept {}

  friend void tag_invoke(set_error_t, universal_receiver&&, auto...) noexcept {}

  friend ex::empty_env tag_invoke(get_env_t,
                                  const universal_receiver&) noexcept {
    return {};
  }
};

struct size_receiver {
  using is_receiver = void;
  using __t = size_receiver;
  using __id = size_receiver;

  friend void tag_invoke(set_value_t, size_receiver&&, size_t) noexcept {}

  friend void tag_invoke(set_stopped_t, size_receiver&&) noexcept {}

  friend void tag_invoke(set_error_t, size_receiver&&, auto...) noexcept {}

  friend ex::empty_env tag_invoke(get_env_t, const size_receiver&) noexcept {
    return {};
  }
};

#endif  // TESTS_TEST_COMMON_RECEIVERS_HPP_
