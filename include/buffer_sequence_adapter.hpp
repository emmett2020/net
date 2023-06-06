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

#ifndef BUFFER_SEQUENCE_ADAPTER_HPP_
#define BUFFER_SEQUENCE_ADAPTER_HPP_

#include "buffer.hpp"
#include "ip/socket_types.hpp"

namespace net {

class buffer_sequence_adapter_base {
 public:
  // The maximum number of buffers to support in a single operation.
  static constexpr int max_buffers = max_iov_len > 64 ? 64 : max_iov_len;

 protected:
  using native_buffer_type = iovec;

  static constexpr void init_iov_base(void*& base, void* addr) { base = addr; }

  template <typename T>
  static constexpr void init_iov_base(T& base, void* addr) {
    base = static_cast<T>(addr);
  }

  static constexpr void init_native_buffer(iovec& iov,
                                           const mutable_buffer& buf) {
    init_iov_base(iov.iov_base, buf.data());
    iov.iov_len = buf.size();
  }

  static constexpr void init_native_buffer(iovec& iov,
                                           const const_buffer& buf) {
    init_iov_base(iov.iov_base, const_cast<void*>(buf.data()));
    iov.iov_len = buf.size();
  }
};

// Helper class to translate buffers into the native buffer representation.
template <typename Buffer, typename Buffers>
class buffer_sequence_adapter : private buffer_sequence_adapter_base {
 public:
  static constexpr bool is_single_buffer = false;
  static constexpr int linearization_storage_size = 8192;

  explicit constexpr buffer_sequence_adapter(const Buffers& sequence) noexcept
      : count_(0), total_buffer_size_(0) {
    buffer_sequence_adapter::init(buffer_sequence_begin(sequence),
                                  buffer_sequence_end(sequence));
  }

  constexpr native_buffer_type* buffers() noexcept { return buffers_; }

  constexpr std::size_t count() const noexcept { return count_; }

  constexpr std::size_t total_size() const noexcept {
    return total_buffer_size_;
  }

  constexpr bool all_empty() const noexcept { return total_buffer_size_ == 0; }

  static constexpr bool all_empty(const Buffers& sequence) noexcept {
    return buffer_sequence_adapter::all_empty(buffer_sequence_begin(sequence),
                                              buffer_sequence_end(sequence));
  }

  static constexpr Buffer first(const Buffers& sequence) noexcept {
    return buffer_sequence_adapter::first(buffer_sequence_begin(sequence),
                                          buffer_sequence_end(sequence));
  }

  static Buffer linearise(const Buffers& sequence,
                          const mutable_buffer& storage) {
    return buffer_sequence_adapter::linearise(buffer_sequence_begin(sequence),
                                              buffer_sequence_end(sequence),
                                              storage);
  }

 private:
  template <typename Iterator>
  void init(Iterator begin, Iterator end) {
    for (Iterator iter = begin; iter != end && count_ < max_buffers;
         ++iter, ++count_) {
      Buffer buffer{*iter};
      init_native_buffer(buffers_[count_], buffer);
      total_buffer_size_ += buffer.size();
    }
  }

  template <typename Iterator>
  static bool all_empty(Iterator begin, Iterator end) {
    std::size_t i = 0;
    for (Iterator iter = begin; iter != end && i < max_buffers; ++iter, ++i) {
      if (Buffer(*iter).size() > 0) {
        return false;
      }
    }
    return true;
  }

  template <typename Iterator>
  static Buffer first(Iterator begin, Iterator end) {
    for (Iterator iter = begin; iter != end; ++iter) {
      Buffer buffer{*iter};
      if (buffer.size() != 0) {
        return buffer;
      }
    }
    return Buffer{};
  }

  template <typename Iterator>
  static Buffer linearise(Iterator begin, Iterator end,
                          const mutable_buffer& storage) {
    mutable_buffer unused_storage = storage;
    Iterator iter = begin;
    while (iter != end && unused_storage.size() != 0) {
      Buffer buffer{*iter};
      ++iter;
      if (buffer.size() == 0) {
        continue;
      }
      if (unused_storage.size() == storage.size()) {
        if (iter == end) {
          return buffer;
        }
        if (buffer.size() >= unused_storage.size()) {
          return buffer;
        }
      }
      unused_storage += buffer_copy(unused_storage, buffer);
    }
    return Buffer{storage.data(), storage.size() - unused_storage.size()};
  }

  native_buffer_type buffers_[max_buffers];
  std::size_t count_;
  std::size_t total_buffer_size_;
};

template <typename Buffer>
class buffer_sequence_adapter<Buffer, mutable_buffer>
    : buffer_sequence_adapter_base {
 public:
  static constexpr bool is_single_buffer = true;
  static constexpr int linearization_storage_size = 1;

  explicit constexpr buffer_sequence_adapter(const mutable_buffer& sequence) {
    init_native_buffer(buffer_, Buffer(sequence));
    total_buffer_size_ = sequence.size();
  }

  constexpr native_buffer_type* buffers() noexcept { return &buffer_; }

  constexpr std::size_t count() const noexcept { return 1; }

  constexpr std::size_t total_size() const noexcept {
    return total_buffer_size_;
  }

  constexpr bool all_empty() const noexcept { return total_buffer_size_ == 0; }

  static constexpr bool all_empty(const mutable_buffer& sequence) noexcept {
    return sequence.size() == 0;
  }

  static constexpr Buffer first(const mutable_buffer& sequence) noexcept {
    return Buffer(sequence);
  }

  static constexpr Buffer linearise(const mutable_buffer& sequence,
                                    const Buffer&) noexcept {
    return Buffer(sequence);
  }

 private:
  native_buffer_type buffer_;
  std::size_t total_buffer_size_;
};

template <typename Buffer>
class buffer_sequence_adapter<Buffer, const_buffer>
    : buffer_sequence_adapter_base {
 public:
  static constexpr bool is_single_buffer = true;
  static constexpr int linearization_storage_size = 1;

  explicit constexpr buffer_sequence_adapter(const const_buffer& sequence) {
    init_native_buffer(buffer_, Buffer(sequence));
    total_buffer_size_ = sequence.size();
  }

  constexpr native_buffer_type* buffers() noexcept { return &buffer_; }

  constexpr std::size_t count() const noexcept { return 1; }

  constexpr std::size_t total_size() const noexcept {
    return total_buffer_size_;
  }

  constexpr bool all_empty() const noexcept { return total_buffer_size_ == 0; }

  static constexpr bool all_empty(const const_buffer& sequence) noexcept {
    return sequence.size() == 0;
  }

  static constexpr void validate(const const_buffer& sequence) noexcept {
    sequence.data();
  }

  static constexpr Buffer first(const const_buffer& sequence) noexcept {
    return Buffer(sequence);
  }

  static constexpr Buffer linearise(const const_buffer& sequence,
                                    const Buffer&) noexcept {
    return Buffer(sequence);
  }

 private:
  native_buffer_type buffer_;
  std::size_t total_buffer_size_;
};

template <typename Buffer, typename Elem>
class buffer_sequence_adapter<Buffer, std::array<Elem, 2>>
    : buffer_sequence_adapter_base {
 public:
  static constexpr bool is_single_buffer = false;
  static constexpr int linearization_storage_size = 8192;

  explicit constexpr buffer_sequence_adapter(
      const std::array<Elem, 2>& sequence) noexcept {
    init_native_buffer(buffers_[0], Buffer(sequence[0]));
    init_native_buffer(buffers_[1], Buffer(sequence[1]));
    total_buffer_size_ = sequence[0].size() + sequence[1].size();
  }

  constexpr native_buffer_type* buffers() noexcept { return buffers_; }

  constexpr std::size_t count() const noexcept { return 2; }

  constexpr std::size_t total_size() const noexcept {
    return total_buffer_size_;
  }

  constexpr bool all_empty() const noexcept { return total_buffer_size_ == 0; }

  static constexpr bool all_empty(
      const std::array<Elem, 2>& sequence) noexcept {
    return sequence[0].size() == 0 && sequence[1].size() == 0;
  }

  static constexpr Buffer first(const std::array<Elem, 2>& sequence) noexcept {
    return Buffer{sequence[0].size() != 0 ? sequence[0] : sequence[1]};
  }

  static constexpr Buffer linearise(const std::array<Elem, 2>& sequence,
                                    const mutable_buffer& storage) {
    if (sequence[0].size() == 0) {
      return Buffer{sequence[1]};
    }
    if (sequence[1].size() == 0) {
      return Buffer{sequence[0]};
    }
    return Buffer{storage.data(), buffer_copy(storage, sequence)};
  }

 private:
  native_buffer_type buffers_[2];
  std::size_t total_buffer_size_;
};

}  // namespace net

#endif  // BUFFER_SEQUENCE_ADAPTER_HPP_
