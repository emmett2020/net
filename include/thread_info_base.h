#ifndef NET_THREAD_INFO_BASE_H_
#define NET_THREAD_INFO_BASE_H_

#include <exception>
#include <memory>
#include <new>

namespace net {
static constexpr int32_t kRecyclingAllocatorCacheSize = 2;
static constexpr int32_t kDefaultAlign = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

class MultipleException : public std::exception {
 public:
  // Constructor.
  MultipleException(std::exception_ptr first) noexcept : first_(std::move(first)){};

  // Obtain message associated with exception.
  virtual const char* what() const noexcept { return "multiple exceptions"; }

  // Obtain a pointer to the first exception.
  std::exception_ptr firstException() const { return first_; }

 private:
  std::exception_ptr first_;
};

// TODO: add non copyable
// The base class for thread-related information.
class ThreadInfoBase {
 public:
  static constexpr int kBeginMemIndex = 0;
  static constexpr int kEndMemIndex = kBeginMemIndex + kRecyclingAllocatorCacheSize;
  static constexpr int kChunkSize = 4;

  ThreadInfoBase() : has_pending_exception_(0) {
    for (int i = 0; i < kEndMemIndex; ++i) {
      reusable_memory_[i] = nullptr;
    }
  }

  ~ThreadInfoBase() = default;

  static void* allocate(ThreadInfoBase* this_thread, std::size_t size,
                        std::size_t align = kDefaultAlign) {
    std::size_t chunks = (size + kChunkSize - 1) / kChunkSize;

    if (this_thread) {
      for (int mem_index = kBeginMemIndex; mem_index < kEndMemIndex; ++mem_index) {
        if (this_thread->reusable_memory_[mem_index]) {
          void* const pointer = this_thread->reusable_memory_[mem_index];
          unsigned char* const mem = static_cast<unsigned char*>(pointer);
          if (static_cast<std::size_t>(mem[0]) >= chunks
              && reinterpret_cast<std::size_t>(pointer) % align == 0) {
            this_thread->reusable_memory_[mem_index] = nullptr;
            mem[size] = mem[0];
            return pointer;
          }
        }
      }

      for (int mem_index = kBeginMemIndex; mem_index < kEndMemIndex; ++mem_index) {
        if (this_thread->reusable_memory_[mem_index]) {
          void* const pointer = this_thread->reusable_memory_[mem_index];
          this_thread->reusable_memory_[mem_index] = 0;
          free(pointer);
          break;
        }
      }
    }

    void* const pointer = alignedAlloc(align, chunks * kChunkSize + 1);
    unsigned char* const mem = static_cast<unsigned char*>(pointer);
    mem[size] = (chunks <= UCHAR_MAX) ? static_cast<unsigned char>(chunks) : 0;
    return pointer;
  }

  template <typename Purpose>
  static void deallocate(ThreadInfoBase* this_thread, void* pointer, std::size_t size) {
    if (size <= kChunkSize * UCHAR_MAX) {
      if (this_thread) {
        for (int mem_index = kBeginMemIndex; mem_index < kEndMemIndex; ++mem_index) {
          if (this_thread->reusable_memory_[mem_index] == 0) {
            unsigned char* const mem = static_cast<unsigned char*>(pointer);
            mem[0] = mem[size];
            this_thread->reusable_memory_[mem_index] = pointer;
            return;
          }
        }
      }
    }

    free(pointer);
  }

  void CaptureCurrentException() {
    switch (has_pending_exception_) {
      case 0:
        has_pending_exception_ = 1;
        pending_exception_ = std::current_exception();
        break;
      case 1:
        has_pending_exception_ = 2;
        pending_exception_ =
            std::make_exception_ptr<MultipleException>(MultipleException(pending_exception_));
        break;
      default:
        break;
    }
  }

  void RethrowPendingException() {
    if (has_pending_exception_ > 0) {
      has_pending_exception_ = 0;
      std::exception_ptr ex(static_cast<std::exception_ptr&&>(pending_exception_));
      std::rethrow_exception(ex);
    }
  }

  // Align and allocate no less than size bytes
  static void* alignedAlloc(std::size_t align, std::size_t size) {
    align = (align < kDefaultAlign) ? kDefaultAlign : align;
    size = (size % align == 0) ? size : size + (align - size % align);
    void* ptr = std::aligned_alloc(align, size);
    if (!ptr) {
      throw std::bad_alloc{};
    }
    return ptr;
  }

 private:
  void* reusable_memory_[kEndMemIndex];

  int has_pending_exception_;
  std::exception_ptr pending_exception_;
};

}  // namespace net

#endif  // NET_THREAD_INFO_BASE_H_
