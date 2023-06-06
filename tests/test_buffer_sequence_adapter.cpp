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
#include <concepts>
#include <cstring>

#include "catch2/catch_test_macros.hpp"

#include "buffer.hpp"
#include "buffer_sequence_adapter.hpp"

using net::buffer;
using net::buffer_sequence_adapter;
using net::const_buffer;
using net::mutable_buffer;

TEST_CASE("buffer_sequence_adapter with different mutable_buffer",
          "buffer.buffer_sequence_adapter") {
  char raw_data[1024];
  const char const_raw_data[1024]{""};
  void* void_ptr_data = static_cast<void*>(raw_data);
  const void* const_void_ptr_data = static_cast<const void*>(const_raw_data);
  std::vector<char> vector_data(1024, ' ');
  const std::vector<char>& const_vector_data = vector_data;
  std::string string_data(1024, ' ');
  const std::string const_string_data(1024, ' ');
  std::string_view string_view_data{string_data};

  using bufs_type = buffer_sequence_adapter<mutable_buffer, mutable_buffer>;

  buffer_sequence_adapter<mutable_buffer, mutable_buffer> b1{
      buffer(raw_data, 1024)};
  CHECK(b1.buffers()->iov_base == raw_data);
  CHECK(b1.buffers()->iov_len == 1024);
  CHECK(b1.count() == 1);
  CHECK(b1.total_size() == 1024);
  CHECK(b1.all_empty() == false);
  CHECK(bufs_type::is_single_buffer);
  CHECK(bufs_type::first(buffer(raw_data, 1024)).data() == raw_data);
  CHECK(bufs_type::first(buffer(raw_data, 1024)).size() == 1024);

  buffer_sequence_adapter<mutable_buffer, mutable_buffer> b2{
      buffer(void_ptr_data, 1024)};
  CHECK(b2.buffers()->iov_base == void_ptr_data);
  CHECK(b2.buffers()->iov_len == 1024);
  CHECK(b2.count() == 1);
  CHECK(b2.total_size() == 1024);
  CHECK(b2.all_empty() == false);
  CHECK(bufs_type::is_single_buffer);
  CHECK(bufs_type::first(buffer(void_ptr_data, 1024)).data() == void_ptr_data);
  CHECK(bufs_type::first(buffer(void_ptr_data, 1024)).size() == 1024);
}

TEST_CASE("buffer_sequence_adapter with const_buffer sequence",
          "buffer.buffer_sequence_adapter") {
  std::vector<const_buffer> vec{10, const_buffer{}};
  using bufs_type =
      buffer_sequence_adapter<const_buffer, std::vector<const_buffer>>;
  bufs_type b1{vec};
  CHECK(b1.buffers()[0].iov_base == vec[0].data());
  CHECK(b1.buffers()->iov_len == buffer_size(vec));
  CHECK(b1.count() == 10);
  CHECK(b1.total_size() == buffer_size(vec));
  CHECK(b1.all_empty() == true);
  CHECK(bufs_type::is_single_buffer == false);
  CHECK(bufs_type::first(vec).data() == vec[0].data());
  CHECK(bufs_type::first(vec).size() == buffer_size(vec[0]));
}
