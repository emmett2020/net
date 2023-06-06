#ifndef NET_KEYWORD_TSS_PTR_H_
#define NET_KEYWORD_TSS_PTR_H_

namespace net {

// TODO: add non copyable
// Thread specific static pointer using `__thread` keyword.
template <typename T>
class KeywordTssPtr {
 public:
  // Constructor.
  KeywordTssPtr() {}

  // Destructor.
  ~KeywordTssPtr() {}

  // Get the value.
  operator T*() const { return value_; }

  // Set the value.
  void operator=(T* value) { value_ = value; }

 private:
  // `__thread` keyword is supported by gnu and clang.
  static __thread T* value_;
};

template <typename T>
__thread T* KeywordTssPtr<T>::value_;
}  // namespace net

#endif  // NET_KEYWORD_TSS_PTR_H_
