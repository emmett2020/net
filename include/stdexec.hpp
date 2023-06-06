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
#ifndef STDEXEC_HPP_
#define STDEXEC_HPP_

// stdexec related header summary, easy to use
#include <stdexec/__detail/__config.hpp>
#include <stdexec/__detail/__execution_fwd.hpp>
#include <stdexec/__detail/__intrusive_queue.hpp>
#include <stdexec/__detail/__meta.hpp>
#include <stdexec/concepts.hpp>
#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>
#include <stdexec/stop_token.hpp>

// The current version of stdexec's header dependency is not handled properly.
// __manual_lifetime relies on stdexec::__nothrow_constructible_from. The former
// is defined in __manual_lifetime.hpp and the latter in stdexec/execution.hpp.
// We must deal with this dependency. So we need to keep the header files in
// order.
#include <exec/__detail/__atomic_intrusive_queue.hpp>
#include <exec/__detail/__manual_lifetime.hpp>
#include <exec/linux/__detail/safe_file_descriptor.hpp>
#include <exec/linux/io_uring_context.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/scope.hpp>
#include <exec/timed_scheduler.hpp>
#include <exec/when_any.hpp>

#endif  // STDEXEC_HPP_
