#include <catch2/catch_test_macros.hpp>
#include <concepts>
#include <cstring>

#include "buffer.hpp"

using namespace net;

template <typename T>
class mock_mutable_contiguous_container {
 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;
  using reference = T&;
  using const_reference = const T&;

  mock_mutable_contiguous_container() {}
  std::size_t size() const { return 0; }
  iterator begin() { return 0; }
  const_iterator begin() const { return 0; }
  iterator end() { return 0; }
  const_iterator end() const { return 0; }
};

template <typename T>
class mock_const_contiguous_container {
 public:
  using value_type = const T;
  using iterator = const T*;
  using const_iterator = const T*;
  using reference = const T&;
  using const_reference = const T&;

  mock_const_contiguous_container() {}
  std::size_t size() const { return 0; }
  iterator begin() { return 0; }
  const_iterator begin() const { return 0; }
  iterator end() { return 0; }
  const_iterator end() const { return 0; }
};

TEST_CASE("default constructor of mutable_buffer", "mutable_buffer.ctor") {
  net::mutable_buffer buf;
  CHECK(buf.data_ == nullptr);
  CHECK(buf.size_ == 0);
  CHECK(buf.data() == nullptr);
  CHECK(buf.size() == 0);
}

TEST_CASE("default constructor of const_buffer", "const_buffer.ctor") {
  net::const_buffer buf;
  CHECK(buf.data_ == nullptr);
  CHECK(buf.size_ == 0);
  CHECK(buf.data() == nullptr);
  CHECK(buf.size() == 0);
}

TEST_CASE("construct const_buffer using std::string", "const_buffer.ctor") {
  std::string str;
  str.resize(1024);
  net::mutable_buffer buf{str.data(), str.size()};
  CHECK(buf.data() == str.data());
  CHECK(buf.size() == str.size());
}

TEST_CASE("use buffer_size to get std::string::size()", "buffer.buffer_size") {
  std::string str(1024, ' ');
  CHECK(buffer_size(net::buffer(str)) == 1024);
}

TEST_CASE("Create a new modifiable buffer that is offset from the start of another",
          "buffer.operation+") {
  std::string str(1024, ' ');
  CHECK((net::buffer(str) + 24).data() == &str[24]);
  CHECK((net::buffer(str) + 24).size() == 1000);
}

TEST_CASE("Create a new non-modifiable buffer that is offset from the start of another",
          "buffer.operation+") {
  const std::string str(1024, 'a');
  CHECK((net::buffer(str) + 24).data() == &str[24]);
  CHECK((net::buffer(str) + 24).size() == 1000);
}

TEST_CASE("Create a new mutable buffer from char[1024]", "buffer.buffer") {
  char buf[1024];
  CHECK(std::same_as<mutable_buffer, decltype(net::buffer(buf))>);
  CHECK(net::buffer(buf).data() == buf);
  CHECK(net::buffer(buf).size() == 1024);
}

TEST_CASE("Create a new const buffer from const char*", "buffer.buffer") {
  const char* buf = "hello world";
  CHECK(std::same_as<const_buffer, decltype(net::buffer(buf, 11))>);
  CHECK(net::buffer(buf, 11).data() == buf);
  CHECK(net::buffer(buf, 11).size() == 11);
}

TEST_CASE("Create a new mutable buffer from std::array", "buffer.buffer") {
  std::array<int, 1024> arr;
  CHECK(std::same_as<mutable_buffer, decltype(net::buffer(arr))>);
  CHECK(net::buffer(arr).data() == &arr[0]);
  CHECK(net::buffer(arr).size() == 1024 * 4);
}

TEST_CASE("Create a new const buffer from std::array", "buffer.buffer") {
  const std::array<int, 1024> arr{};
  CHECK(std::same_as<const_buffer, decltype(net::buffer(arr))>);
  CHECK(net::buffer(arr).data() == &arr[0]);
  CHECK(net::buffer(arr).size() == 1024 * 4);
}

TEST_CASE("Create a new mutable buffer from std::vector", "buffer.buffer") {
  std::vector<int> vec;
  vec.resize(1024);
  CHECK(std::same_as<mutable_buffer, decltype(net::buffer(vec))>);
  CHECK(net::buffer(vec).data() == &vec[0]);
  CHECK(net::buffer(vec).size() == 1024 * 4);
}

TEST_CASE("Create a new mutable buffer from std::string", "buffer.buffer") {
  std::string str;
  str.resize(1024);
  CHECK(std::same_as<mutable_buffer, decltype(net::buffer(str))>);
  CHECK(net::buffer(str).data() == &str[0]);
  CHECK(net::buffer(str).size() == 1024);
}

TEST_CASE("Create a new const buffer from std::string", "buffer.buffer") {
  const std::string str(1024, 'a');
  CHECK(std::same_as<const_buffer, decltype(net::buffer(str))>);
  CHECK(net::buffer(str).data() == &str[0]);
  CHECK(net::buffer(str).size() == 1024);
}

TEST_CASE("Create a new const buffer from std::string_view", "buffer.buffer") {
  // buffer(string_view) only return const_buffer
  std::string_view str = "hello world";
  CHECK(std::same_as<const_buffer, decltype(net::buffer(str))>);
  CHECK(net::buffer(str).data() == &str[0]);
  CHECK(net::buffer(str).size() == ::strlen("hello world"));
}

TEST_CASE("validate mutable_buffer and const_buffer with difference types", "buffer") {
  char raw_data[1024];
  const char const_raw_data[1024]{""};
  void* void_ptr_data = static_cast<void*>(raw_data);
  const void* const_void_ptr_data = static_cast<const void*>(const_raw_data);
  std::vector<char> vector_data(1024, ' ');
  const std::vector<char>& const_vector_data = vector_data;
  std::string string_data(1024, ' ');
  const std::string const_string_data(1024, ' ');
  std::string_view string_view_data{string_data};

  mock_mutable_contiguous_container<char> mutable_contiguous_data;
  const mock_mutable_contiguous_container<char> const_mutable_contiguous_data;
  mock_const_contiguous_container<char> const_contiguous_data;
  const mock_const_contiguous_container<char> const_const_contiguous_data;

  mutable_buffer mb1{raw_data, 1024};
  CHECK(mb1.data() == raw_data);
  CHECK(mb1.size() == 1024);
  CHECK(buffer_size(mb1) == 1024);
  CHECK((mb1 + 256).data() == raw_data + 256);

  mutable_buffer mb2{void_ptr_data, 1024};
  CHECK(mb2.data() == void_ptr_data);
  CHECK(mb2.size() == 1024);
  CHECK(buffer_size(mb2) == 1024);
  CHECK((mb2 + 256).data() == static_cast<char*>(void_ptr_data) + 256);
  CHECK(buffer_copy(mb1, mb2) == 1024);
  CHECK(buffer_copy(mb2, mb1) == 1024);
  CHECK(buffer_copy(mb2 + 1000, mb1) == 24);

  mutable_buffer mb3{net::buffer(vector_data)};
  CHECK(mb3.data() == &vector_data[0]);
  CHECK(mb3.size() == 1024);
  CHECK(buffer_size(mb3) == 1024);
  CHECK((mb3 + 256).data() == &vector_data[256]);
  CHECK(buffer_copy(mb3, mb2) == 1024);
  CHECK(buffer_copy(mb2, mb3) == 1024);
  CHECK(buffer_copy(mb3 + 1000, mb2) == 24);

  mutable_buffer mb4{net::buffer(string_data)};
  CHECK(mb4.data() == &string_data[0]);
  CHECK(mb4.size() == 1024);
  CHECK(buffer_size(mb4) == 1024);
  CHECK((mb4 + 256).data() == &string_data[256]);
  CHECK(buffer_copy(mb4, mb3) == 1024);
  CHECK(buffer_copy(mb3, mb4) == 1024);
  CHECK(buffer_copy(mb4 + 1000, mb3) == 24);

  const_buffer cb1{const_raw_data, 1024};
  CHECK(cb1.data() == const_raw_data);
  CHECK(cb1.size() == 1024);
  CHECK(buffer_size(cb1) == 1024);
  CHECK((cb1 + 256).data() == const_raw_data + 256);
  CHECK(buffer_copy(mb1, cb1) == 1024);
  CHECK(buffer_copy(mb1 + 1000, mb1) == 24);

  const_buffer cb2{const_void_ptr_data, 1024};
  CHECK(cb2.data() == const_void_ptr_data);
  CHECK(cb2.size() == 1024);
  CHECK(buffer_size(cb2) == 1024);
  CHECK((cb2 + 256).data() == static_cast<const char*>(const_void_ptr_data) + 256);
  CHECK(buffer_copy(mb2, cb2) == 1024);
  CHECK(buffer_copy(mb2 + 1000, cb2) == 24);

  const_buffer cb3{net::buffer(const_vector_data)};
  CHECK(cb3.data() == &const_vector_data[0]);
  CHECK(cb3.size() == 1024);
  CHECK(buffer_size(cb3) == 1024);
  CHECK((cb3 + 256).data() == &const_vector_data[256]);
  CHECK(buffer_copy(mb3, cb3) == 1024);
  CHECK(buffer_copy(mb3 + 1000, cb3) == 24);

  const_buffer cb4{net::buffer(const_string_data)};
  CHECK(cb4.data() == &const_string_data[0]);
  CHECK(cb4.size() == 1024);
  CHECK(buffer_size(cb4) == 1024);
  CHECK((cb4 + 256).data() == &const_string_data[256]);
  CHECK(buffer_copy(mb4, cb4) == 1024);
  CHECK(buffer_copy(mb4 + 1000, cb4) == 24);

  const_buffer cb5{net::buffer(string_view_data)};
  CHECK(cb5.data() == &string_view_data[0]);
  CHECK(cb5.size() == 1024);
  CHECK(buffer_size(cb5) == 1024);
  CHECK((cb5 + 256).data() == &string_view_data[256]);

  std::vector<mutable_buffer> mutable_buffer_sequence{mb1, mb2};
  std::vector<const_buffer> const_buffer_sequence{cb1, cb2};
  CHECK(buffer_size(mutable_buffer_sequence) == 2048);
  CHECK(buffer_size(const_buffer_sequence) == 2048);
  CHECK(buffer_copy(mb4, mutable_buffer_sequence) == 1024);
  CHECK(buffer_copy(mutable_buffer_sequence, mb4) == 1024);
}

TEST_CASE("buffer_sequence_begin and buffer_sequence_end should work",
          "buffer.buffer_sequence_xxxxx") {
  const_buffer cb;
  CHECK(buffer_sequence_begin(cb) == &cb);
  CHECK(buffer_sequence_end(cb) == &cb + 1);

  const_buffer mb;
  CHECK(buffer_sequence_begin(mb) == &mb);
  CHECK(buffer_sequence_end(mb) == &mb + 1);

  std::vector<const_buffer> cb_vec;
  CHECK(buffer_sequence_begin(cb_vec) == cb_vec.begin());
  CHECK(buffer_sequence_end(cb_vec) == cb_vec.end());

  std::vector<const_buffer> mb_vec;
  CHECK(buffer_sequence_begin(mb_vec) == mb_vec.begin());
  CHECK(buffer_sequence_end(mb_vec) == mb_vec.end());
}

TEST_CASE("mutable_buffer should satisfy mutable_buffer_sequence", "buffer.concept") {
  CHECK(mutable_buffer_sequence<mutable_buffer>);
  CHECK(mutable_buffer_sequence<std::vector<mutable_buffer>>);
  CHECK(!mutable_buffer_sequence<std::vector<char>>);
  CHECK(!mutable_buffer_sequence<std::string>);
}

TEST_CASE("const_buffer should satisfy const_buffer_sequence", "buffer.concept") {
  CHECK(const_buffer_sequence<const_buffer>);
  CHECK(const_buffer_sequence<std::vector<const_buffer>>);
  CHECK(!const_buffer_sequence<const std::vector<char>>);
  CHECK(!const_buffer_sequence<const std::string>);
}
