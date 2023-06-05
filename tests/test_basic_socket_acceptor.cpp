#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <status-code/system_code.hpp>
#include <system_error>

#include "basic_socket_acceptor.hpp"
#include "execution_context.hpp"
#include "mock_protocol.hpp"

using namespace std;

TEST_CASE("[Default constructor]", "[basic_socket_acceptor.ctor]") {
  execution_context ctx{};
  basic_socket_acceptor<mock_protocol> acceptor{ctx};
  CHECK(!acceptor.is_open());
  CHECK(acceptor.state() == 0);
  CHECK(acceptor.protocol_ == std::nullopt);
}

TEST_CASE("[Construct acceptor using an endpoint]", "[basic_socket_acceptor.ctor]") {
  execution_context ctx{};
  mock_protocol::endpoint endpoint{address_v4::any(), 80};
  system_error2::system_code ec;
  basic_socket_acceptor<mock_protocol> acceptor{ctx, endpoint, ec, false};
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  CHECK(acceptor.close().success());
}

TEST_CASE("[Construct acceptor using an endpoint and reuse the address]",
          "[basic_socket_acceptor.ctor]") {
  execution_context ctx{};
  mock_protocol::endpoint endpoint{address_v4::any(), 80};
  system_error2::system_code ec;
  basic_socket_acceptor<mock_protocol> acceptor{ctx, endpoint, ec};
  CHECK(ec.success());
  CHECK(acceptor.is_open());
  socket_base::reuse_address option{};
  CHECK(acceptor.get_option(option).success());
  CHECK(option.value() == 1);
  CHECK(acceptor.close().success());
}