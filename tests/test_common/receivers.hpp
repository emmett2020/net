#ifndef NET_TESTS_TEST_COMMON_RECEIVERS_HPP_
#define NET_TESTS_TEST_COMMON_RECEIVERS_HPP_

#include <catch2/catch_all.hpp>
#include <stdexec.hpp>
#include <stdexec/execution.hpp>
#include <tuple>

namespace ex = stdexec;
using ex::get_env_t;
using ex::set_error_t;
using ex::set_stopped_t;
using ex::set_value_t;

struct empty_receiver {
  using is_receiver = void;
  using __t = empty_receiver;
  using __id = empty_receiver;
  friend void tag_invoke(set_value_t, empty_receiver&&) noexcept {}
  friend void tag_invoke(set_stopped_t, empty_receiver&&) noexcept {}
  friend void tag_invoke(set_error_t, empty_receiver&&, std::exception_ptr) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const empty_receiver&) noexcept { return {}; }
};

struct universal_receiver {
  using is_receiver = void;
  using __t = universal_receiver;
  using __id = universal_receiver;
  friend void tag_invoke(set_value_t, universal_receiver&&, auto...) noexcept {}
  friend void tag_invoke(set_stopped_t, universal_receiver&&) noexcept {}
  friend void tag_invoke(set_error_t, universal_receiver&&, auto...) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const universal_receiver&) noexcept { return {}; }
};

struct size_receiver {
  using is_receiver = void;
  using __t = size_receiver;
  using __id = size_receiver;
  friend void tag_invoke(set_value_t, size_receiver&&, size_t) noexcept {}
  friend void tag_invoke(set_stopped_t, size_receiver&&) noexcept {}
  friend void tag_invoke(set_error_t, size_receiver&&, auto...) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const size_receiver&) noexcept { return {}; }
};

struct recv_int {
  friend void tag_invoke(set_value_t, recv_int&&, int) noexcept {}
  friend void tag_invoke(set_stopped_t, recv_int&&) noexcept {}
  friend void tag_invoke(set_error_t, recv_int&&, std::exception_ptr) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const recv_int&) noexcept { return {}; }
};

struct recv0_ec {
  friend void tag_invoke(set_value_t, recv0_ec&&) noexcept {}
  friend void tag_invoke(set_stopped_t, recv0_ec&&) noexcept {}
  friend void tag_invoke(set_error_t, recv0_ec&&, std::error_code) noexcept {}
  friend void tag_invoke(set_error_t, recv0_ec&&, std::exception_ptr) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const recv0_ec&) noexcept { return {}; }
};

struct recv_int_ec {
  friend void tag_invoke(set_value_t, recv_int_ec&&, int) noexcept {}
  friend void tag_invoke(set_stopped_t, recv_int_ec&&) noexcept {}
  friend void tag_invoke(set_error_t, recv_int_ec&&, std::error_code) noexcept {}
  friend void tag_invoke(set_error_t, recv_int_ec&&, std::exception_ptr) noexcept {}
  friend ex::empty_env tag_invoke(get_env_t, const recv_int_ec&) noexcept { return {}; }
};

template <class Env = ex::empty_env>
class expect_receiver_base {
 public:
  using is_receiver = void;
  using __t = expect_receiver_base;
  using __id = expect_receiver_base;

  constexpr expect_receiver_base() = default;

  ~expect_receiver_base() { CHECK(called_.load()); }

  explicit expect_receiver_base(Env env) : env_(std::move(env)) {}

  expect_receiver_base(expect_receiver_base&& other)
      : called_(other.called_.exchange(true)), env_(std::move(other.env_)) {}

  expect_receiver_base& operator=(expect_receiver_base&& other) {
    called_.store(other.called_.exchange(true));
    env_ = std::move(other.env_);
    return *this;
  }

  friend Env tag_invoke(ex::get_env_t, const expect_receiver_base& self) noexcept {
    return self.env_;
  }

  void set_called() { called_.store(true); }

 private:
  std::atomic<bool> called_{false};
  Env env_{};
};

template <class Env = ex::empty_env>
struct expect_void_receiver : expect_receiver_base<Env> {
  constexpr expect_void_receiver() = default;

  explicit expect_void_receiver(Env env) : expect_receiver_base<Env>(std::move(env)) {}

  friend void tag_invoke(ex::set_value_t, expect_void_receiver&& self) noexcept {
    self.set_called();
  }

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_void_receiver&&, Ts...) noexcept {
    FAIL_CHECK("set_value called on expect_void_receiver with some non-void value");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_void_receiver&&) noexcept {
    FAIL_CHECK("set_stopped called on expect_void_receiver");
  }

  friend void tag_invoke(ex::set_error_t, expect_void_receiver&&, std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called on expect_void_receiver");
  }
};

struct expect_void_receiver_ex {
  expect_void_receiver_ex(bool& executed) : executed_(&executed) {}

 private:
  bool* executed_;

  template <class... Ty>
  friend void tag_invoke(ex::set_value_t, expect_void_receiver_ex&& self, const Ty&...) noexcept {
    *self.executed_ = true;
  }

  friend void tag_invoke(ex::set_stopped_t, expect_void_receiver_ex&&) noexcept {
    FAIL_CHECK("set_stopped called on expect_void_receiver_ex");
  }

  friend void tag_invoke(ex::set_error_t, expect_void_receiver_ex&&, std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called on expect_void_receiver_ex");
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, const expect_void_receiver_ex&) noexcept {
    return {};
  }
};

struct env_tag {};

template <class Env = ex::empty_env, typename... Ts>
struct expect_value_receiver : expect_receiver_base<Env> {
  explicit(sizeof...(Ts) != 1) expect_value_receiver(Ts... vals) : values_(std::move(vals)...) {}

  expect_value_receiver(env_tag, Env env, Ts... vals)
      : expect_receiver_base<Env>(std::move(env)), values_(std::move(vals)...) {}

  friend void tag_invoke(ex::set_value_t, expect_value_receiver&& self, const Ts&... vals)  //
      noexcept {
    CHECK(self.values_ == std::tie(vals...));
    self.set_called();
  }

  template <typename... Us>
  friend void tag_invoke(ex::set_value_t, expect_value_receiver&&, const Us&...) noexcept {
    FAIL_CHECK("set_value called with wrong value types on expect_value_receiver");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_value_receiver&& self) noexcept {
    FAIL_CHECK("set_stopped called on expect_value_receiver");
  }

  template <typename E>
  friend void tag_invoke(ex::set_error_t, expect_value_receiver&&, E) noexcept {
    FAIL_CHECK("set_error called on expect_value_receiver");
  }

 private:
  std::tuple<Ts...> values_;
};

template <typename T, class Env = ex::empty_env>
class expect_value_receiver_ex {
  T* dest_;
  Env env_{};

 public:
  explicit expect_value_receiver_ex(T& dest) : dest_(&dest) {}

  expect_value_receiver_ex(Env env, T& dest) : dest_(&dest), env_(std::move(env)) {}

  friend void tag_invoke(ex::set_value_t, expect_value_receiver_ex self, T val) noexcept {
    *self.dest_ = val;
  }

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_value_receiver_ex, Ts...) noexcept {
    FAIL_CHECK("set_value called with wrong value types on expect_value_receiver_ex");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_value_receiver_ex) noexcept {
    FAIL_CHECK("set_stopped called on expect_value_receiver_ex");
  }

  friend void tag_invoke(ex::set_error_t, expect_value_receiver_ex, std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called on expect_value_receiver_ex");
  }

  friend Env tag_invoke(ex::get_env_t, const expect_value_receiver_ex& self) noexcept {
    return self.env_;
  }
};

template <class Env = ex::empty_env>
struct expect_stopped_receiver : expect_receiver_base<Env> {
  using is_receiver = void;
  using __t = expect_stopped_receiver;
  using __id = expect_stopped_receiver;

  constexpr expect_stopped_receiver() = default;
  expect_stopped_receiver(Env env) : expect_receiver_base<Env>(std::move(env)) {}

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_stopped_receiver&&, Ts...) noexcept {
    FAIL_CHECK("set_value called on expect_stopped_receiver");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_stopped_receiver&& self) noexcept {
    self.set_called();
  }

  friend void tag_invoke(ex::set_error_t, expect_stopped_receiver&&, std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called on expect_stopped_receiver");
  }
};

template <class Env = ex::empty_env>
struct expect_stopped_receiver_ex {
  explicit expect_stopped_receiver_ex(bool& executed) : executed_(&executed) {}

  expect_stopped_receiver_ex(Env env, bool& executed)
      : executed_(&executed), env_(std::move(env)) {}

 private:
  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_stopped_receiver_ex&&, Ts...) noexcept {
    FAIL_CHECK("set_value called on expect_stopped_receiver_ex");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_stopped_receiver_ex&& self) noexcept {
    *self.executed_ = true;
  }

  friend void tag_invoke(ex::set_error_t, expect_stopped_receiver_ex&&, std::exception_ptr)  //
      noexcept {
    FAIL_CHECK("set_error called on expect_stopped_receiver_ex");
  }

  friend Env tag_invoke(ex::get_env_t, const expect_stopped_receiver_ex& self) noexcept {
    return self.env_;
  }

  bool* executed_;
  Env env_;
};

inline std::pair<const std::type_info&, std::string> to_comparable(std::exception_ptr eptr) {
  try {
    std::rethrow_exception(eptr);
  } catch (const std::exception& e) {
    return {typeid(e), e.what()};
  } catch (...) {
    return {typeid(void), "<unknown>"};
  }
}

template <typename T>
inline T to_comparable(T value) {
  return value;
}

template <class T = std::exception_ptr, class Env = ex::empty_env>
struct expect_error_receiver : expect_receiver_base<Env> {
  using is_receiver = void;
  using __t = expect_error_receiver;
  using __id = expect_error_receiver;

  expect_error_receiver() = default;

  explicit expect_error_receiver(T error) : error_(std::move(error)) {}

  expect_error_receiver(Env env, T error)
      : expect_receiver_base<Env>{std::move(env)}, error_(std::move(error)) {}

  // these do not move error_ and cannot be defaulted
  expect_error_receiver(expect_error_receiver&& other)
      : expect_receiver_base<Env>(std::move(other)), error_() {}

  expect_error_receiver& operator=(expect_error_receiver&& other) noexcept {
    expect_receiver_base<Env>::operator=(std::move(other));
    error_.reset();
    return *this;
  }

 private:
  std::optional<T> error_;

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_error_receiver&&, Ts...) noexcept {
    FAIL_CHECK("set_value called on expect_error_receiver");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_error_receiver&& self) noexcept {
    FAIL_CHECK("set_stopped called on expect_error_receiver");
  }

  friend void tag_invoke(ex::set_error_t, expect_error_receiver&& self, T err) noexcept {
    self.set_called();
    if (self.error_) {
      CHECK(to_comparable(err) == to_comparable(*self.error_));
    }
  }

  template <typename E>
  friend void tag_invoke(ex::set_error_t, expect_error_receiver&& self, E) noexcept {
    FAIL_CHECK("set_error called on expect_error_receiver with wrong error type");
  }
};

template <class T, class Env = ex::empty_env>
struct expect_error_receiver_ex {
  explicit expect_error_receiver_ex(T& value) : value_(&value) {}

  expect_error_receiver_ex(Env env, T& value) : value_(&value), env_(std::move(env)) {}

 private:
  T* value_;
  Env env_{};

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, expect_error_receiver_ex&&, Ts...) noexcept {
    FAIL_CHECK("set_value called on expect_error_receiver_ex");
  }

  friend void tag_invoke(ex::set_stopped_t, expect_error_receiver_ex&&) noexcept {
    FAIL_CHECK("set_stopped called on expect_error_receiver_ex");
  }

  template <class Err>
  friend void tag_invoke(ex::set_error_t, expect_error_receiver_ex&&, Err) noexcept {
    FAIL_CHECK("set_error called on expect_error_receiver_ex with the wrong error type");
  }

  friend void tag_invoke(ex::set_error_t, expect_error_receiver_ex&& self, T value) noexcept {
    *self.value_ = std::move(value);
  }

  friend Env tag_invoke(ex::get_env_t, const expect_error_receiver_ex& self) noexcept {
    return self.env_;
  }
};

struct logging_receiver {
  logging_receiver(int& state) : state_(&state) {}

 private:
  int* state_;

  template <typename... Args>
  friend void tag_invoke(ex::set_value_t, logging_receiver&& self, Args...) noexcept {
    *self.state_ = 0;
  }

  friend void tag_invoke(ex::set_stopped_t, logging_receiver&& self) noexcept { *self.state_ = 1; }

  template <typename E>
  friend void tag_invoke(ex::set_error_t, logging_receiver&& self, E) noexcept {
    *self.state_ = 2;
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, const logging_receiver&) noexcept { return {}; }
};

enum class typecat {
  undefined,
  value,
  ref,
  cref,
  rvalref,
};

template <typename T>
struct typecat_receiver {
  T* value_;
  typecat* cat_;

  // friend void tag_invoke(ex::set_value_t, typecat_receiver self, T v) noexcept {
  //     *self.value_ = v;
  //     *self.cat_ = typecat::value;
  // }
  friend void tag_invoke(ex::set_value_t, typecat_receiver self, T& v) noexcept {
    *self.value_ = v;
    *self.cat_ = typecat::ref;
  }

  friend void tag_invoke(ex::set_value_t, typecat_receiver self, const T& v) noexcept {
    *self.value_ = v;
    *self.cat_ = typecat::cref;
  }

  friend void tag_invoke(ex::set_value_t, typecat_receiver self, T&& v) noexcept {
    *self.value_ = v;
    *self.cat_ = typecat::rvalref;
  }

  friend void tag_invoke(ex::set_stopped_t, typecat_receiver self) noexcept {
    FAIL_CHECK("set_stopped called");
  }

  friend void tag_invoke(ex::set_error_t, typecat_receiver self, std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called");
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, const typecat_receiver&) noexcept { return {}; }
};

template <typename F>
struct fun_receiver {
  F f_;

  template <typename... Ts>
  friend void tag_invoke(ex::set_value_t, fun_receiver&& self, Ts... vals) noexcept {
    try {
      std::move(self.f_)((Ts &&) vals...);
    } catch (...) {
      ex::set_error(std::move(self), std::current_exception());
    }
  }

  friend void tag_invoke(ex::set_stopped_t, fun_receiver) noexcept { FAIL("Done called"); }

  friend void tag_invoke(ex::set_error_t, fun_receiver, std::exception_ptr eptr) noexcept {
    try {
      if (eptr) std::rethrow_exception(eptr);
      FAIL("Empty exception thrown");
    } catch (const std::exception& e) {
      FAIL("Exception thrown: " << e.what());
    }
  }

  friend ex::empty_env tag_invoke(ex::get_env_t, const fun_receiver&) noexcept { return {}; }
};

template <typename F>
fun_receiver<F> make_fun_receiver(F f) {
  return fun_receiver<F>{std::forward<F>(f)};
}

template <ex::sender S, typename... Ts>
inline void wait_for_value(S&& snd, Ts&&... val) {
  // Ensure that the given sender type has only one variant for set_value calls
  // If not, sync_wait will not work
  static_assert(stdexec::__single_value_variant_sender<S>,
                "Sender passed to sync_wait needs to have one variant for sending set_value");

  std::optional<std::tuple<Ts...>> res = stdexec::sync_wait((S &&) snd);
  CHECK(res.has_value());
  std::tuple<Ts...> expected((Ts &&) val...);
  if constexpr (std::tuple_size_v<std::tuple<Ts...>> == 1)
    CHECK(std::get<0>(res.value()) == std::get<0>(expected));
  else
    CHECK(res.value() == expected);
}

#endif  // NET_TESTS_TEST_COMMON_RECEIVERS_HPP_
