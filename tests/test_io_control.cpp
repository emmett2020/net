#include <asm-generic/ioctls.h>
#include <catch2/catch_test_macros.hpp>
#include "io_control.hpp"

using namespace net;

TEST_CASE("[default constructor]", "[io_control.ctor]") {
  io_control::bytes_readable ctrl{};
  CHECK(ctrl.name() == FIONREAD);
  CHECK(ctrl.get() == 0);
}

TEST_CASE("[set value]", "[io_control.set]") {
  io_control::bytes_readable ctrl{};
  ctrl.set(12);
  CHECK(ctrl.get() == 12);
}