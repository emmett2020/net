#ifndef NET_CALL_STACK_H_
#define NET_CALL_STACK_H_

#include "keyword_tss_ptr.h"
// #include "noncopyable.h"

namespace net {

// Helper class to determine whether or not the current thread is inside an
// invocation of context::run() for a specified context object.
template <typename Key, typename Value = unsigned char>
class CallStack {
 public:
  // TODO: add non copyable
  // Context class automatically pushes the key/value pair on to the stack.
  class Context {
   public:
    // Push the key on to the stack.
    explicit Context(Key* k) : key_(k), next_(CallStack<Key, Value>::top_) {
      value_ = reinterpret_cast<unsigned char*>(this);
      CallStack<Key, Value>::top_ = this;
    }

    // Push the key/value pair on to the stack.
    Context(Key* k, Value& v) : key_(k), value_(&v), next_(CallStack<Key, Value>::top_) {
      CallStack<Key, Value>::top_ = this;
    }

    // Pop the key/value pair from the stack.
    ~Context() { CallStack<Key, Value>::top_ = next_; }

    // Find the next context with the same key.
    Value* nextByKey() const {
      Context* elem = next_;
      while (elem) {
        if (elem->key_ == key_) {
          return elem->value_;
        }
        elem = elem->next_;
      }
      return nullptr;
    }

   private:
    friend class CallStack<Key, Value>;

    // The key associated with the Context.
    Key* key_;

    // The value associated with the Context.
    Value* value_;

    // The next element in the stack.
    Context* next_;
  };

  friend class Context;

  // Determine whether the specified owner is on the stack. Returns address of
  // key if present, nullptr otherwise.
  static Value* contains(Key* k) {
    Context* elem = top_;
    while (elem) {
      if (elem->key_ == k) {
        return elem->value_;
      }
      elem = elem->next_;
    }
    return nullptr;
  }

  // Obtain the value at the top of the stack.
  static Value* top() {
    Context* elem = top_;
    return elem ? elem->value_ : nullptr;
  }

 private:
  // The top of the stack of calls for the current thread.
  static KeywordTssPtr<Context> top_;
};

template <typename Key, typename Value>
KeywordTssPtr<typename CallStack<Key, Value>::Context> CallStack<Key, Value>::top_;

}  // namespace net

#endif  // NET_CALL_STACK_H_
