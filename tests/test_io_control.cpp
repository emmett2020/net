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
#include "catch2/catch_test_macros.hpp"

#include "io_control.hpp"

using namespace net;  // NOLINT

TEST_CASE("[default constructor]", "[io_control.ctor]") {
  io_control::bytes_readable ctrl{};
  CHECK(ctrl.name() == FIONREAD);
  CHECK(ctrl.get() == 0);
}

TEST_CASE("[set value]", "[io_control.set]") {
  io_control::bytes_readable ctrl{};
  ctrl.set(12);
  CHECK(ctrl.get() == 12);
}
