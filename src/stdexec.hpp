#ifndef NET_SRC_STDEXEC_HPP_
#define NET_SRC_STDEXEC_HPP_

// stdexec related header summary, easy to use
#include <stdexec/__detail/__config.hpp>
#include <stdexec/__detail/__execution_fwd.hpp>
#include <stdexec/__detail/__intrusive_queue.hpp>
#include <stdexec/__detail/__meta.hpp>
#include <stdexec/concepts.hpp>
#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>
#include <stdexec/stop_token.hpp>

// The current version of stdexec's header dependency is not handled properly. __manual_lifetime
// relies on stdexec::__nothrow_constructible_from. The former is defined in __manual_lifetime.hpp
// and the latter in stdexec/execution.hpp. We must deal with this dependency. So we need to keep
// the header files in order.
#include <exec/__detail/__atomic_intrusive_queue.hpp>
#include <exec/__detail/__manual_lifetime.hpp>
#include <exec/linux/__detail/safe_file_descriptor.hpp>
#include <exec/linux/io_uring_context.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/scope.hpp>
#include <exec/timed_scheduler.hpp>
#include <exec/when_any.hpp>

#endif  // NET_SRC_STDEXEC_HPP_