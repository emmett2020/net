#include <sys/socket.h>
#include <catch2/catch_test_macros.hpp>

#include "ip/tcp.hpp"
#include "meta.hpp"

using namespace std;
using namespace net;

TEST_CASE("[tcp must satisfied the concept of transport_protocol]", "[tcp.concept]") {
  CHECK(transport_protocol<net::ip::tcp>);
}

TEST_CASE("[IPv4 constructor]", "[tcp.ctor]") {
  auto proto = net::ip::tcp::v4();
  CHECK(proto.family() == AF_INET);
  CHECK(proto.type() == SOCK_STREAM);
  CHECK(proto.protocol() == IPPROTO_TCP);
}

TEST_CASE("[IPv6 constructor]", "[tcp.ctor]") {
  auto proto = net::ip::tcp::v6();
  CHECK(proto.family() == AF_INET6);
  CHECK(proto.type() == SOCK_STREAM);
  CHECK(proto.protocol() == IPPROTO_TCP);
}

TEST_CASE("[comparision of tcp]", "[tcp.operator=]") {
  CHECK(net::ip::tcp::v4() == net::ip::tcp::v4());
  CHECK(net::ip::tcp::v6() != net::ip::tcp::v4());
}
