#ifndef SRC_NET_OBJECT_POOL_H_
#define SRC_NET_OBJECT_POOL_H_

#include "noncopyable.h"

namespace net {

template <typename Object>
class ObjectPool;

class ObjectPoolAccess {
 public:
  template <typename Object>
  static Object* create() {
    return new Object;
  }

  template <typename Object, typename Arg>
  static Object* create(Arg arg) {
    return new Object(arg);
  }

  template <typename Object>
  static void destroy(Object* o) {
    delete o;
  }

  template <typename Object>
  static Object*& next(Object* o) {
    return o->next_;
  }

  template <typename Object>
  static Object*& prev(Object* o) {
    return o->prev_;
  }
};

template <typename Object>
class ObjectPool : private NonCopyable {
 public:
  // Constructor.
  ObjectPool() : live_list_(nullptr), free_list_(nullptr) {}

  // Destructor destroys all objects.
  ~ObjectPool() {
    destroyList(live_list_);
    destroyList(free_list_);
  }

  // Get the object at the start of the live list.
  Object* first() { return live_list_; }

  // Allocate a new object.
  Object* alloc() {
    Object* o = free_list_;
    if (o) {
      free_list_ = ObjectPoolAccess::next(free_list_);
    } else {
      o = ObjectPoolAccess::create<Object>();
    }

    ObjectPoolAccess::next(o) = live_list_;
    ObjectPoolAccess::prev(o) = nullptr;
    if (live_list_) {
      ObjectPoolAccess::prev(live_list_) = o;
    }
    live_list_ = o;

    return o;
  }

  // Allocate a new object with an argument.
  template <typename Arg>
  Object* alloc(Arg arg) {
    Object* o = free_list_;
    if (o) {
      free_list_ = ObjectPoolAccess::next(free_list_);
    } else {
      o = ObjectPoolAccess::create<Object>(arg);
    }

    ObjectPoolAccess::next(o) = live_list_;
    ObjectPoolAccess::prev(o) = nullptr;
    if (live_list_) {
      ObjectPoolAccess::prev(live_list_) = o;
    }
    live_list_ = o;

    return o;
  }

  // Free an object. Moves it to the free list. No destructors are run.
  void free(Object* o) {
    if (live_list_ == o) {
      live_list_ = ObjectPoolAccess::next(o);
    }

    if (ObjectPoolAccess::prev(o)) {
      ObjectPoolAccess::next(ObjectPoolAccess::prev(o)) = ObjectPoolAccess::next(o);
    }

    if (ObjectPoolAccess::next(o)) {
      ObjectPoolAccess::prev(ObjectPoolAccess::next(o)) = ObjectPoolAccess::prev(o);
    }

    ObjectPoolAccess::next(o) = free_list_;
    ObjectPoolAccess::prev(o) = nullptr;
    free_list_ = o;
  }

 private:
  // Helper function to destroy all elements in a list.
  void destroyList(Object* list) {
    while (list) {
      Object* o = list;
      list = ObjectPoolAccess::next(o);
      ObjectPoolAccess::destroy(o);
    }
  }

  // The list of live objects.
  Object* live_list_;

  // The free list.
  Object* free_list_;
};

}  // namespace net

#endif  // SRC_NET_OBJECT_POOL_H_
