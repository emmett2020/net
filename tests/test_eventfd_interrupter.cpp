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

#include "eventfd_interrupter.hpp"
#include "ip/socket_types.hpp"

using namespace std;  // NOLINT
using namespace net;  // NOLINT

TEST_CASE("[default constructor should open eventfd successfully]",
          "[eventfd_interrupter.ctor]") {
  eventfd_interrupter interrupter{};
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.read_descriptor_ == interrupter.write_descriptor_);
}

TEST_CASE("[destructor should close two descriptors]",
          "[eventfd_interrupter.dtor]") {
  eventfd_interrupter interrupter;
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);

  interrupter.close_descriptors();
  CHECK(interrupter.read_descriptor_ == invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ == invalid_socket_fd);
}

TEST_CASE(
    "[Call interrupt to notify the reader of the connection. Call reset to "
    "receive data and reset "
    "the read status of the connection]",
    "[eventfd_interrupter.interrupt]") {
  eventfd_interrupter interrupter{};
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.interrupt());
  CHECK(interrupter.reset());
}
