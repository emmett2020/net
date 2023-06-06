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
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>

#include "catch2/catch_test_macros.hpp"

#include "socket_base.hpp"

using net::socket_base;

TEST_CASE("[default constructor should compile]", "[socket_base.ctor]") {
  socket_base base{};
}

TEST_CASE("[options should compile]", "[socket_base.open]") {
  socket_base base{};

  socket_base::broadcast{};
  socket_base::bytes_readable{};
  socket_base::debug{};
  socket_base::do_not_route{};
  socket_base::enable_connection_aborted{};
  socket_base::keep_alive{};
  socket_base::linger{};
  socket_base::out_of_band_Inline{};
  socket_base::receive_buffer_size{};
  socket_base::reuse_address{};
  socket_base::send_low_water_mark{};
}
