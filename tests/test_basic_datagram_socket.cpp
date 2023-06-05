#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <status-code/system_code.hpp>

#include "basic_datagram_socket.hpp"
#include "mock_protocol.hpp"

using namespace std;

TEST_CASE("[Default constructor]", "[basic_datagram_socket.ctor]") {
  execution_context ctx{};
  basic_datagram_socket<mock_protocol> socket{ctx};
  CHECK(!socket.is_open());
  CHECK(socket.state() == 0);
  CHECK(socket.protocol_ == std::nullopt);
}
