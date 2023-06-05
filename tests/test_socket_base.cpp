#include <netinet/in.h>
#include <sys/socket.h>
#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "socket_base.hpp"

using namespace net;

TEST_CASE("[default constructor should compile]", "[socket_base.ctor]") { socket_base base{}; }

TEST_CASE("[options should compile]", "[socket_base.open]") {
  socket_base base{};

  socket_base::broadcast{};
  socket_base::bytes_readable{};
  socket_base::debug{};
  socket_base::do_not_route{};
  socket_base::enable_connection_aborted{};
  socket_base::keep_alive{};
  socket_base::linger{};
  socket_base::out_of_band_Inline{};
  socket_base::receive_buffer_size{};
  socket_base::reuse_address{};
  socket_base::send_low_water_mark{};
}
