#ifndef SRC_NET_EXECUTION_CONTEXT_HPP_
#define SRC_NET_EXECUTION_CONTEXT_HPP_

namespace net {

// The base class for the io operation context.
// The context of epoll/io_uring etc can inherit from this class, and then the `basic_socket` can be
// constructed normally. Note that we chose `inheritance` over `template` to avoid providing an
// extra template parameter when constructing the `basic_socket`. Therefore, this class is used only
// as a convenience in constructing sockets and has no other unique features.
class execution_context {
 public:
  // Default constructor.
  constexpr execution_context() noexcept = default;

  // Move constructor.
  constexpr execution_context(execution_context&&) noexcept = delete;

  // Move assign.
  constexpr execution_context& operator=(execution_context&&) noexcept = delete;

  // Copy constructor.
  constexpr execution_context(const execution_context&) noexcept = default;

  // Copy assign.
  constexpr execution_context& operator=(const execution_context&) noexcept = default;

  // Destructor.
  constexpr ~execution_context() noexcept = default;
};

}  // namespace net

#endif  // SRC_EXECUTION_CONTEXT_HPP_
