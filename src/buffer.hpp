#ifndef NET_SRC_BUFFER_HPP_
#define NET_SRC_BUFFER_HPP_

#include <array>
#include <concepts>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace net {

// Holds a buffer that can be modified.
class mutable_buffer {
 public:
  // Construct an empty buffer.
  constexpr mutable_buffer() noexcept : data_(nullptr), size_(0) {}

  // Construct a buffer to represent a given memory range.
  constexpr mutable_buffer(void* data, std::size_t size) noexcept : data_(data), size_(size) {}

  // Get a pointer to the beginning of the memory range.
  constexpr void* data() const noexcept { return data_; }

  // Get the size of the memory range.
  constexpr std::size_t size() const noexcept { return size_; }

  // Move the start of the buffer by the specified number of bytes.
  constexpr mutable_buffer& operator+=(std::size_t n) noexcept {
    std::size_t offset = n < size_ ? n : size_;
    data_ = static_cast<char*>(data_) + offset;
    size_ -= offset;
    return *this;
  }

 private:
  void* data_;
  std::size_t size_;
};

// Holds a buffer that cannot be modified.
class const_buffer {
 public:
  // Construct an empty buffer.
  constexpr const_buffer() noexcept : data_(0), size_(0) {}

  // Construct a buffer to represent a given memory range.
  constexpr const_buffer(const void* data, std::size_t size) noexcept : data_(data), size_(size) {}

  // Construct a non-modifiable buffer from a modifiable one.
  constexpr const_buffer(const mutable_buffer& b) noexcept : data_(b.data()), size_(b.size()) {}

  // Get a pointer to the beginning of the memory range.
  constexpr const void* data() const noexcept { return data_; }

  // Get the size of the memory range.
  constexpr std::size_t size() const noexcept { return size_; }

  // Move the start of the buffer by the specified number of bytes.
  constexpr const_buffer& operator+=(std::size_t n) noexcept {
    std::size_t offset = n < size_ ? n : size_;
    data_ = static_cast<const char*>(data_) + offset;
    size_ -= offset;
    return *this;
  }

 private:
  const void* data_;
  std::size_t size_;
};

// Get an iterator to the first element in a buffer sequence.
template <typename MutableBuffer>
  requires(std::convertible_to<const MutableBuffer*, const mutable_buffer*>)
inline constexpr const mutable_buffer* buffer_sequence_begin(const MutableBuffer& b) noexcept {
  return static_cast<const mutable_buffer*>(std::addressof(b));
}

// Get an iterator to the first element in a buffer sequence.
template <typename ConstBuffer>
  requires(std::convertible_to<const ConstBuffer*, const const_buffer*>)
inline constexpr const const_buffer* buffer_sequence_begin(const ConstBuffer& b) noexcept {
  return static_cast<const const_buffer*>(std::addressof(b));
}

// Get an iterator to the first element in a buffer sequence.
template <typename C>
  requires(!std::convertible_to<const C*, const const_buffer*>  //
           && !std::convertible_to<const C*, const mutable_buffer*>)
inline constexpr auto buffer_sequence_begin(C& c) noexcept -> decltype(c.begin()) {
  return c.begin();
}

// Get an iterator to the first element in a buffer sequence.
template <typename C>
  requires(!std::convertible_to<const C*, const const_buffer*>  //
           && !std::convertible_to<const C*, const mutable_buffer*>)
inline constexpr auto buffer_sequence_begin(const C& c) noexcept -> decltype(c.begin()) {
  return c.begin();
}

// Get an iterator to one past the end element in a buffer sequence.
template <typename MutableBuffer>
  requires(std::convertible_to<const MutableBuffer*, const mutable_buffer*>)
inline constexpr const mutable_buffer* buffer_sequence_end(const MutableBuffer& b) noexcept {
  return static_cast<const mutable_buffer*>(std::addressof(b)) + 1;
}

// Get an iterator to one past the end element in a buffer sequence.
template <typename ConstBuffer>
  requires(std::convertible_to<const ConstBuffer*, const const_buffer*>)
inline constexpr const const_buffer* buffer_sequence_end(const ConstBuffer& b) noexcept {
  return static_cast<const const_buffer*>(std::addressof(b)) + 1;
}

// Get an iterator to one past the end element in a buffer sequence.
template <typename C>
  requires(!std::convertible_to<const C*, const const_buffer*>  //
           && !std::convertible_to<const C*, const mutable_buffer*>)
inline constexpr auto buffer_sequence_end(C& c) noexcept -> decltype(c.end()) {
  return c.end();
}

// Get an iterator to one past the end element in a buffer sequence.
template <typename C>
  requires(!std::convertible_to<const C*, const const_buffer*>  //
           && !std::convertible_to<const C*, const mutable_buffer*>)
inline constexpr auto buffer_sequence_end(const C& c) noexcept -> decltype(c.end()) {
  return c.end();
}

// Tag types used to select appropriately optimized overloads.
struct one_buffer {};
struct multiple_buffers {};

// Helper trait to detect single buffers.
template <typename BufferSequence>
struct buffer_sequence_cardinality
    : std::conditional<std::same_as<BufferSequence, const_buffer>
                           || std::same_as<BufferSequence, mutable_buffer>,
                       one_buffer, multiple_buffers>::type {};

template <typename Iterator>
inline constexpr std::size_t buffer_size(one_buffer, Iterator begin, Iterator) noexcept {
  return const_buffer{*begin}.size();
}

template <typename Iterator>
inline constexpr std::size_t buffer_size(multiple_buffers, Iterator begin, Iterator end) noexcept {
  std::size_t total_buffer_size = 0;

  Iterator iter = begin;
  for (; iter != end; ++iter) {
    const_buffer b{*iter};
    total_buffer_size += b.size();
  }

  return total_buffer_size;
}

// Get the total number of bytes in a buffer sequence.
template <typename BufferSequence>
inline constexpr std::size_t buffer_size(const BufferSequence& b) noexcept {
  return buffer_size(buffer_sequence_cardinality<BufferSequence>(),  //
                     buffer_sequence_begin(b),                       //
                     buffer_sequence_end(b));
}

// Create a new modifiable buffer that is offset from the start of another.
inline constexpr mutable_buffer operator+(const mutable_buffer& b, std::size_t n) noexcept {
  std::size_t offset = n < b.size() ? n : b.size();
  char* new_data = static_cast<char*>(b.data()) + offset;
  std::size_t new_size = b.size() - offset;
  return mutable_buffer{new_data, new_size};
}

// Create a new modifiable buffer that is offset from the start of another.
inline constexpr mutable_buffer operator+(std::size_t n, const mutable_buffer& b) noexcept {
  return b + n;
}

// Create a new non-modifiable buffer that is offset from the start of another.
inline constexpr const_buffer operator+(const const_buffer& b, std::size_t n) noexcept {
  std::size_t offset = n < b.size() ? n : b.size();
  const char* new_data = static_cast<const char*>(b.data()) + offset;
  std::size_t new_size = b.size() - offset;
  return const_buffer{new_data, new_size};
}

// Create a new non-modifiable buffer that is offset from the start of another.
inline constexpr const_buffer operator+(std::size_t n, const const_buffer& b) noexcept {
  return b + n;
}

// Create a new modifiable buffer from an existing buffer.
inline constexpr mutable_buffer buffer(const mutable_buffer& b) noexcept {
  return mutable_buffer{b};
}

// Create a new modifiable buffer from an existing buffer.
inline constexpr mutable_buffer buffer(const mutable_buffer& b, std::size_t max_size) noexcept {
  return mutable_buffer{b.data(), b.size() < max_size ? b.size() : max_size};
}

// Create a new non-modifiable buffer from an existing buffer.
inline constexpr const_buffer buffer(const const_buffer& b) noexcept { return const_buffer{b}; }

// Create a new non-modifiable buffer from an existing buffer.
inline constexpr const_buffer buffer(const const_buffer& b, std::size_t max_size) noexcept {
  return const_buffer{b.data(), b.size() < max_size ? b.size() : max_size};
}

// Create a new modifiable buffer that represents the given memory range.
inline constexpr mutable_buffer buffer(void* data, std::size_t size_in_bytes) noexcept {
  return mutable_buffer{data, size_in_bytes};
}

// Create a new non-modifiable buffer that represents the given memory range.
inline constexpr const_buffer buffer(const void* data, std::size_t size_in_bytes) noexcept {
  return const_buffer{data, size_in_bytes};
}

// Create a new modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr mutable_buffer buffer(PodType (&data)[N]) noexcept {
  return mutable_buffer{data, N * sizeof(PodType)};
}

// Create a new modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr mutable_buffer buffer(PodType (&data)[N], std::size_t max_size) noexcept {
  return mutable_buffer{data, N * sizeof(PodType) < max_size ? N * sizeof(PodType) : max_size};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(const PodType (&data)[N]) noexcept {
  return const_buffer{data, N * sizeof(PodType)};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(const PodType (&data)[N], std::size_t max_size) noexcept {
  return const_buffer{data, N * sizeof(PodType) < max_size ? N * sizeof(PodType) : max_size};
}

// Create a new modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr mutable_buffer buffer(std::array<PodType, N>& data) noexcept {
  return mutable_buffer{data.data(), data.size() * sizeof(PodType)};
}

// Create a new modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr mutable_buffer buffer(std::array<PodType, N>& data,
                                       std::size_t max_size) noexcept {
  return {data.data(),
          data.size() * sizeof(PodType) < max_size ? data.size() * sizeof(PodType) : max_size};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(std::array<const PodType, N>& data) noexcept {
  return const_buffer{data.data(), data.size() * sizeof(PodType)};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(std::array<const PodType, N>& data,
                                     std::size_t max_size) noexcept {
  return {data.data(),
          data.size() * sizeof(PodType) < max_size ? data.size() * sizeof(PodType) : max_size};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(const std::array<PodType, N>& data) noexcept {
  return const_buffer{data.data(), data.size() * sizeof(PodType)};
}

// Create a new non-modifiable buffer that represents the given POD array.
template <typename PodType, std::size_t N>
inline constexpr const_buffer buffer(const std::array<PodType, N>& data,
                                     std::size_t max_size) noexcept {
  return {data.data(),
          data.size() * sizeof(PodType) < max_size ? data.size() * sizeof(PodType) : max_size};
}

// Create a new modifiable buffer that represents the given POD vector.
template <typename PodType, typename Allocator>
inline constexpr mutable_buffer buffer(std::vector<PodType, Allocator>& data) noexcept {
  return mutable_buffer(data.size() ? &data[0] : 0, data.size() * sizeof(PodType));
}

// Create a new modifiable buffer that represents the given POD vector.
template <typename PodType, typename Allocator>
inline constexpr mutable_buffer buffer(std::vector<PodType, Allocator>& data,
                                       std::size_t max_size) noexcept {
  return {data.size() ? &data[0] : 0,
          data.size() * sizeof(PodType) < max_size ? data.size() * sizeof(PodType) : max_size};
}

// Create a new non-modifiable buffer that represents the given POD vector.
template <typename PodType, typename Allocator>
inline constexpr const_buffer buffer(const std::vector<PodType, Allocator>& data) noexcept {
  return const_buffer{data.size() ? &data[0] : 0, data.size() * sizeof(PodType)};
}

// Create a new non-modifiable buffer that represents the given POD vector.
template <typename PodType, typename Allocator>
inline constexpr const_buffer buffer(const std::vector<PodType, Allocator>& data,
                                     std::size_t max_size) noexcept {
  return {data.size() ? &data[0] : 0,
          data.size() * sizeof(PodType) < max_size ? data.size() * sizeof(PodType) : max_size};
}

// Create a new modifiable buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr mutable_buffer buffer(std::basic_string<Elem, Traits, Allocator>& data) noexcept {
  return mutable_buffer(data.size() ? &data[0] : 0, data.size() * sizeof(Elem));
}

// Create a new modifiable buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr mutable_buffer buffer(std::basic_string<Elem, Traits, Allocator>& data,
                                       std::size_t max_size) noexcept {
  return {data.size() ? &data[0] : 0,
          data.size() * sizeof(Elem) < max_size ? data.size() * sizeof(Elem) : max_size};
}

// Create a new non-modifiable buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr const_buffer buffer(
    const std::basic_string<Elem, Traits, Allocator>& data) noexcept {
  return const_buffer(data.data(), data.size() * sizeof(Elem));
}

// Create a new non-modifiable buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr const_buffer buffer(const std::basic_string<Elem, Traits, Allocator>& data,
                                     std::size_t max_size) noexcept {
  return {data.data(),
          data.size() * sizeof(Elem) < max_size ? data.size() * sizeof(Elem) : max_size};
}

// Create a new non-modifiable buffer that represents the given string_view.
template <typename Elem, typename Traits>
inline constexpr const_buffer buffer(std::basic_string_view<Elem, Traits> data) noexcept {
  return const_buffer{data.size() ? &data[0] : 0, data.size() * sizeof(Elem)};
}

// Create a new non-modifiable buffer that represents the given string.
template <typename Elem, typename Traits>
inline constexpr const_buffer buffer(std::basic_string_view<Elem, Traits> data,
                                     std::size_t max_size) noexcept {
  return {data.size() ? &data[0] : 0,
          data.size() * sizeof(Elem) < max_size ? data.size() * sizeof(Elem) : max_size};
}

template <typename T>
concept mutable_contiguous_container =
    std::contiguous_iterator<typename T::iterator>
    && (!std::is_const<typename std::remove_reference<
            typename std::iterator_traits<typename T::iterator>::reference>::type>::value);

template <typename T>
concept const_contiguous_container =
    std::contiguous_iterator<typename T::iterator>
    && (std::is_const<typename std::remove_reference<
            typename std::iterator_traits<typename T::iterator>::reference>::type>::value);

// Create a new modifiable buffer from a contiguous container.
template <mutable_contiguous_container T>
inline constexpr mutable_buffer buffer(T& data) noexcept {
  return {data.size() ? std::addressof(data.begin()) : 0,
          data.size() * sizeof(typename T::value_type)};
}

// Create a new modifiable buffer from a contiguous container.
template <mutable_contiguous_container T>
inline constexpr mutable_buffer buffer(T& data, std::size_t max_size) noexcept {
  return {data.size() ? std::addressof(data.begin()) : 0,
          data.size() * sizeof(typename T::value_type) < max_size
              ? data.size() * sizeof(typename T::value_type)
              : max_size};
}

// Create a new non-modifiable buffer from a contiguous container.
template <const_contiguous_container T>
inline constexpr const_buffer buffer(T& data) noexcept {
  return {data.size() ? std::addressof(data.begin()) : 0,
          data.size() * sizeof(typename T::value_type)};
}

// Create a new non-modifiable buffer from a contiguous container.
template <const_contiguous_container T>
inline constexpr const_buffer buffer(T& data, std::size_t max_size) noexcept {
  return {data.size() ? std::addressof(data.begin()) : 0,
          data.size() * sizeof(typename T::value_type) < max_size
              ? data.size() * sizeof(typename T::value_type)
              : max_size};
}

// Create a new non-modifiable buffer from a contiguous container.
template <typename T>
  requires(std::contiguous_iterator<typename T::const_iterator>)
inline constexpr const_buffer buffer(const T& data) noexcept {
  return const_buffer(data.size() ? std::addressof(data.begin()) : 0,
                      data.size() * sizeof(typename T::value_type));
}

// Create a new non-modifiable buffer from a contiguous container.
template <typename T>
  requires(std::contiguous_iterator<typename T::const_iterator>)
inline constexpr const_buffer buffer(const T& data, std::size_t max_size) noexcept {
  return const_buffer(data.size() ? std::addressof(data.begin()) : 0,
                      data.size() * sizeof(typename T::value_type) < max_size
                          ? data.size() * sizeof(typename T::value_type)
                          : max_size);
}

// Adapt a basic_string to the DynamicBuffer requirements.
template <typename Elem, typename Traits, typename Allocator>
class dynamic_string_buffer {
 public:
  // The type used to represent a sequence of constant buffers that refers to the underlying memory.
  using const_buffers_type = const_buffer;

  // The type used to represent a sequence of mutable buffers that refers to the underlying memory.
  using mutable_buffers_type = mutable_buffer;

  // Construct a dynamic buffer from a string.
  explicit constexpr dynamic_string_buffer(
      std::basic_string<Elem, Traits, Allocator>& s,
      std::size_t max_size = (std::numeric_limits<std::size_t>::max)()) noexcept
      : string_(s), size_((std::numeric_limits<std::size_t>::max)()), max_size_(max_size) {}

  // Copy construct a dynamic buffer.
  constexpr dynamic_string_buffer(const dynamic_string_buffer& other) noexcept
      : string_(other.string_), size_(other.size_), max_size_(other.max_size_) {}

  // Move construct a dynamic buffer.
  constexpr dynamic_string_buffer(dynamic_string_buffer&& other) noexcept
      : string_(other.string_), size_(other.size_), max_size_(other.max_size_) {}

  // Get the current size of the underlying memory.
  constexpr std::size_t size() const noexcept {
    if (size_ != (std::numeric_limits<std::size_t>::max)()) {
      return size_;
    }
    return std::min(string_.size(), max_size());
  }

  // Get the maximum size of the dynamic buffer.
  constexpr std::size_t max_size() const noexcept { return max_size_; }

  // Get the maximum size that the buffer may grow to without triggering reallocation.
  constexpr std::size_t capacity() const noexcept {
    return std::min(string_.capacity(), max_size());
  }

  // Get a list of buffers that represents the input sequence.
  constexpr const_buffers_type data() const noexcept {
    return const_buffers_type{buffer(string_, size_)};
  }

  // Get a sequence of buffers that represents the underlying memory.
  constexpr mutable_buffers_type data(std::size_t pos, std::size_t n) noexcept {
    return mutable_buffers_type(buffer(buffer(string_, max_size_) + pos, n));
  }

  // Get a sequence of buffers that represents the underlying memory.
  constexpr const_buffers_type data(std::size_t pos, std::size_t n) const noexcept {
    return const_buffers_type(buffer(buffer(string_, max_size_) + pos, n));
  }

  // Get a list of buffers that represents the output sequence, with the given size.
  constexpr mutable_buffers_type prepare(std::size_t n) {
    if (size() > max_size() || max_size() - size() < n) {
      throw std::length_error{"dynamic_string_buffer too long"};
    }

    if (size_ == (std::numeric_limits<std::size_t>::max)()) {
      size_ = string_.size();
    }

    string_.resize(size_ + n);
    return buffer(buffer(string_) + size_, n);
  }

  // Move bytes from the output sequence to the input sequence.
  constexpr void commit(std::size_t n) {
    size_ += (std::min)(n, string_.size() - size_);
    string_.resize(size_);
  }

  // Grow the underlying memory by the specified number of bytes.
  constexpr void grow(std::size_t n) {
    if (size() > max_size() || max_size() - size() < n) {
      throw std::length_error{"dynamic_string_buffer too long"};
    }
    string_.resize(size() + n);
  }

  // Shrink the underlying memory by the specified number of bytes.
  constexpr void shrink(std::size_t n) { string_.resize(n > size() ? 0 : size() - n); }

  // Remove characters from the input sequence. Consume the specified number of bytes from the
  // beginning of the underlying memory.
  constexpr void consume(std::size_t n) {
    if (size_ != (std::numeric_limits<std::size_t>::max)()) {
      std::size_t consume_length = (std::min)(n, size_);
      string_.erase(0, consume_length);
      size_ -= consume_length;
      return;
    }
    string_.erase(0, n);
  }

 private:
  std::basic_string<Elem, Traits, Allocator>& string_;
  std::size_t size_;
  const std::size_t max_size_;
};

// Adapt a vector to the DynamicBuffer requirements.
template <typename Elem, typename Allocator>
class dynamic_vector_buffer {
 public:
  // The type used to represent a sequence of constant buffers that refers to the underlying memory.
  using const_buffers_type = const_buffer;

  // The type used to represent a sequence of mutable buffers that refers to the underlying memory.
  using mutable_buffers_type = mutable_buffer;

  // Construct a dynamic buffer from a vector.
  explicit constexpr dynamic_vector_buffer(
      std::vector<Elem, Allocator>& v,
      std::size_t max_size = (std::numeric_limits<std::size_t>::max)()) noexcept
      : vector_(v), size_((std::numeric_limits<std::size_t>::max)()), max_size_(max_size) {}

  //  Copy construct a dynamic buffer.
  constexpr dynamic_vector_buffer(const dynamic_vector_buffer& other) noexcept
      : vector_(other.vector_), size_(other.size_), max_size_(other.max_size_) {}

  // Move construct a dynamic buffer.
  constexpr dynamic_vector_buffer(dynamic_vector_buffer&& other) noexcept
      : vector_(other.vector_), size_(other.size_), max_size_(other.max_size_) {}

  //  Get the current size of the underlying memory.
  constexpr std::size_t size() const noexcept {
    if (size_ != (std::numeric_limits<std::size_t>::max)()) {
      return size_;
    }
    return std::min(vector_.size(), max_size());
  }

  // Get the maximum size of the dynamic buffer.
  constexpr std::size_t max_size() const noexcept { return max_size_; }

  // Get the maximum size that the buffer may grow to without triggering reallocation.
  constexpr std::size_t capacity() const noexcept {
    return std::min(vector_.capacity(), max_size());
  }

  // Get a list of buffers that represents the input sequence.
  constexpr const_buffers_type data() const noexcept {
    return const_buffers_type(buffer(vector_, size_));
  }

  // Get a sequence of buffers that represents the underlying memory.
  constexpr mutable_buffers_type data(std::size_t pos, std::size_t n) noexcept {
    return mutable_buffers_type(buffer(buffer(vector_, max_size_) + pos, n));
  }

  // Get a sequence of buffers that represents the underlying memory.
  constexpr const_buffers_type data(std::size_t pos, std::size_t n) const noexcept {
    return const_buffers_type(buffer(buffer(vector_, max_size_) + pos, n));
  }

  // Get a list of buffers that represents the output sequence, with the given size.
  constexpr mutable_buffers_type prepare(std::size_t n) {
    if (size() > max_size() || max_size() - size() < n) {
      throw std::length_error{"dynamic_vector_buffer too long"};
    }
    if (size_ == (std::numeric_limits<std::size_t>::max)()) {
      size_ = vector_.size();
    }
    vector_.resize(size_ + n);
    return buffer(buffer(vector_) + size_, n);
  }

  // Move bytes from the output sequence to the input sequence.
  constexpr void commit(std::size_t n) {
    size_ += (std::min)(n, vector_.size() - size_);
    vector_.resize(size_);
  }

  // Grow the underlying memory by the specified number of bytes.
  constexpr void grow(std::size_t n) {
    if (size() > max_size() || max_size() - size() < n) {
      throw std::length_error{"dynamic_vector_buffer too long"};
    }
    vector_.resize(size() + n);
  }

  // Shrink the underlying memory by the specified number of bytes.
  constexpr void shrink(std::size_t n) { vector_.resize(n > size() ? 0 : size() - n); }

  // Remove characters from the input sequence. Consume the specified number of bytes from the
  // beginning of the underlying memory.
  constexpr void consume(std::size_t n) {
    if (size_ != (std::numeric_limits<std::size_t>::max)()) {
      std::size_t consume_length = (std::min)(n, size_);
      vector_.erase(vector_.begin(), vector_.begin() + consume_length);
      size_ -= consume_length;
      return;
    }
    vector_.erase(vector_.begin(), vector_.begin() + (std::min)(size(), n));
  }

 private:
  std::vector<Elem, Allocator>& vector_;
  std::size_t size_;
  const std::size_t max_size_;
};

// Create a new dynamic buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr dynamic_string_buffer<Elem, Traits, Allocator> dynamic_buffer(
    std::basic_string<Elem, Traits, Allocator>& data) noexcept {
  return dynamic_string_buffer<Elem, Traits, Allocator>(data);
}

// Create a new dynamic buffer that represents the given string.
template <typename Elem, typename Traits, typename Allocator>
inline constexpr dynamic_string_buffer<Elem, Traits, Allocator> dynamic_buffer(
    std::basic_string<Elem, Traits, Allocator>& data, std::size_t max_size) noexcept {
  return dynamic_string_buffer<Elem, Traits, Allocator>(data, max_size);
}

// Create a new dynamic buffer that represents the given vector.
template <typename Elem, typename Allocator>
inline constexpr dynamic_vector_buffer<Elem, Allocator> dynamic_buffer(
    std::vector<Elem, Allocator>& data) noexcept {
  return dynamic_vector_buffer<Elem, Allocator>(data);
}

// Create a new dynamic buffer that represents the given vector.
template <typename Elem, typename Allocator>
inline constexpr dynamic_vector_buffer<Elem, Allocator> dynamic_buffer(
    std::vector<Elem, Allocator>& data, std::size_t max_size) noexcept {
  return dynamic_vector_buffer<Elem, Allocator>(data, max_size);
}

inline std::size_t buffer_copy_1(const mutable_buffer& target, const const_buffer& source) {
  std::size_t target_size = target.size();
  std::size_t source_size = source.size();
  std::size_t n = target_size < source_size ? target_size : source_size;
  if (n > 0) {
    ::memcpy(target.data(), source.data(), n);
  }
  return n;
}

template <typename TargetIterator, typename SourceIterator>
inline std::size_t buffer_copy(one_buffer,                   //
                               one_buffer,                   //
                               TargetIterator target_begin,  //
                               TargetIterator,               //
                               SourceIterator source_begin,  //
                               SourceIterator) noexcept {
  return buffer_copy_1(*target_begin, *source_begin);
}

template <typename TargetIterator, typename SourceIterator>
inline std::size_t buffer_copy(one_buffer,                   //
                               one_buffer,                   //
                               TargetIterator target_begin,  //
                               TargetIterator,               //
                               SourceIterator source_begin,  //
                               SourceIterator,               //
                               std::size_t max_bytes_to_copy) noexcept {
  return buffer_copy_1(*target_begin, buffer(*source_begin, max_bytes_to_copy));
}

template <typename TargetIterator, typename SourceIterator>
std::size_t buffer_copy(
    one_buffer,                   //
    multiple_buffers,             //
    TargetIterator target_begin,  //
    TargetIterator,               //
    SourceIterator source_begin,  //
    SourceIterator source_end,    //
    std::size_t max_bytes_to_copy = std::numeric_limits<std::size_t>::max()) noexcept {
  std::size_t total_bytes_copied = 0;
  SourceIterator source_iter = source_begin;

  for (mutable_buffer target_buffer{buffer(*target_begin, max_bytes_to_copy)};
       target_buffer.size() && source_iter != source_end;  //
       ++source_iter) {
    const_buffer source_buffer{*source_iter};
    std::size_t bytes_copied = buffer_copy_1(target_buffer, source_buffer);
    total_bytes_copied += bytes_copied;
    target_buffer += bytes_copied;
  }
  return total_bytes_copied;
}

template <typename TargetIterator, typename SourceIterator>
std::size_t buffer_copy(
    multiple_buffers,             //
    one_buffer,                   //
    TargetIterator target_begin,  //
    TargetIterator target_end,    //
    SourceIterator source_begin,  //
    SourceIterator,               //
    std::size_t max_bytes_to_copy = std::numeric_limits<std::size_t>::max()) noexcept {
  std::size_t total_bytes_copied = 0;
  TargetIterator target_iter = target_begin;

  for (const_buffer source_buffer{buffer(*source_begin, max_bytes_to_copy)};
       source_buffer.size() && target_iter != target_end;  //
       ++target_iter) {
    mutable_buffer target_buffer{*target_iter};
    std::size_t bytes_copied = buffer_copy_1(target_buffer, source_buffer);
    total_bytes_copied += bytes_copied;
    source_buffer += bytes_copied;
  }
  return total_bytes_copied;
}

template <typename TargetIterator, typename SourceIterator>
std::size_t buffer_copy(multiple_buffers,             //
                        multiple_buffers,             //
                        TargetIterator target_begin,  //
                        TargetIterator target_end,    //
                        SourceIterator source_begin,  //
                        SourceIterator source_end) noexcept {
  std::size_t total_bytes_copied = 0;
  TargetIterator target_iter = target_begin;
  std::size_t target_buffer_offset = 0;
  SourceIterator source_iter = source_begin;
  std::size_t source_buffer_offset = 0;

  while (target_iter != target_end && source_iter != source_end) {
    mutable_buffer target_buffer{mutable_buffer{*target_iter} + target_buffer_offset};
    const_buffer source_buffer{const_buffer{*source_iter} + source_buffer_offset};
    std::size_t bytes_copied = buffer_copy_1(target_buffer, source_buffer);
    total_bytes_copied += bytes_copied;

    if (bytes_copied == target_buffer.size()) {
      ++target_iter;
      target_buffer_offset = 0;
    } else {
      target_buffer_offset += bytes_copied;
    }

    if (bytes_copied == source_buffer.size()) {
      ++source_iter;
      source_buffer_offset = 0;
    } else {
      source_buffer_offset += bytes_copied;
    }
  }
  return total_bytes_copied;
}

template <typename TargetIterator, typename SourceIterator>
std::size_t buffer_copy(multiple_buffers,             //
                        multiple_buffers,             //
                        TargetIterator target_begin,  //
                        TargetIterator target_end,    //
                        SourceIterator source_begin,  //
                        SourceIterator source_end,    //
                        std::size_t max_bytes_to_copy) noexcept {
  std::size_t total_bytes_copied = 0;
  TargetIterator target_iter = target_begin;
  std::size_t target_buffer_offset = 0;
  SourceIterator source_iter = source_begin;
  std::size_t source_buffer_offset = 0;

  while (total_bytes_copied != max_bytes_to_copy && target_iter != target_end
         && source_iter != source_end) {
    mutable_buffer target_buffer{mutable_buffer{*target_iter} + target_buffer_offset};
    const_buffer source_buffer{const_buffer{*source_iter} + source_buffer_offset};
    std::size_t bytes_copied =
        buffer_copy_1(target_buffer, buffer(source_buffer, max_bytes_to_copy - total_bytes_copied));
    total_bytes_copied += bytes_copied;

    if (bytes_copied == target_buffer.size()) {
      ++target_iter;
      target_buffer_offset = 0;
    } else {
      target_buffer_offset += bytes_copied;
    }

    if (bytes_copied == source_buffer.size()) {
      ++source_iter;
      source_buffer_offset = 0;
    } else {
      source_buffer_offset += bytes_copied;
    }
  }
  return total_bytes_copied;
}

// Copies bytes from a source buffer sequence to a target buffer sequence.
// Return the bytes that real copied.
template <typename MutableBufferSequence, typename ConstBufferSequence>
inline std::size_t buffer_copy(const MutableBufferSequence& target,
                               const ConstBufferSequence& source) noexcept {
  return buffer_copy(buffer_sequence_cardinality<MutableBufferSequence>(),
                     buffer_sequence_cardinality<ConstBufferSequence>(),
                     buffer_sequence_begin(target), buffer_sequence_end(target),
                     buffer_sequence_begin(source), buffer_sequence_end(source));
}

// Copies a limited number of bytes from a source buffer sequence to a target buffer sequence.
// Return the bytes that real copied.
template <typename MutableBufferSequence, typename ConstBufferSequence>
inline std::size_t buffer_copy(const MutableBufferSequence& target,
                               const ConstBufferSequence& source,
                               std::size_t max_bytes_to_copy) noexcept {
  return buffer_copy(buffer_sequence_cardinality<MutableBufferSequence>(),
                     buffer_sequence_cardinality<ConstBufferSequence>(),
                     buffer_sequence_begin(target), buffer_sequence_end(target),
                     buffer_sequence_begin(source), buffer_sequence_end(source), max_bytes_to_copy);
}

// Concept to determine whether a type satisfies the MutableBufferSequence requirements.
template <typename T>
concept mutable_buffer_sequence =
    std::is_class_v<T>
    && requires(T& t) {
         buffer_sequence_begin(t);
         buffer_sequence_end(t);
         requires std::convertible_to<decltype(*buffer_sequence_begin(t)), mutable_buffer>;
       };

// Concept to determine whether a type satisfies the ConstBufferSequence requirements.
template <typename T>
concept const_buffer_sequence =
    std::is_class_v<T>
    && requires(T& t) {
         buffer_sequence_begin(t);
         buffer_sequence_end(t);
         requires std::convertible_to<decltype(*buffer_sequence_begin(t)), const_buffer>;
       };
}  // namespace net

#endif  // NET_SRC_BUFFER_HPP_
