#include <catch2/catch_test_macros.hpp>
#include "eventfd_interrupter.hpp"
#include "ip/socket_types.hpp"

using namespace std;
using namespace net;

TEST_CASE("[default constructor should open eventfd successfully]", "[eventfd_interrupter.ctor]") {
  eventfd_interrupter interrupter{};
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.read_descriptor_ == interrupter.write_descriptor_);
}

TEST_CASE("[destructor should close two descriptors]", "[eventfd_interrupter.dtor]") {
  eventfd_interrupter interrupter;
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);

  interrupter.close_descriptors();
  CHECK(interrupter.read_descriptor_ == invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ == invalid_socket_fd);
}

TEST_CASE(
    "[Call interrupt to notify the reader of the connection. Call reset to receive data and reset "
    "the read status of the connection]",
    "[eventfd_interrupter.interrupt]") {
  eventfd_interrupter interrupter{};
  CHECK(interrupter.read_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.write_descriptor_ != invalid_socket_fd);
  CHECK(interrupter.interrupt());
  CHECK(interrupter.reset());
}