#include <sys/socket.h>
#include <catch2/catch_test_macros.hpp>

#include "ip/udp.hpp"
#include "meta.hpp"

using namespace std;
using namespace net;

TEST_CASE("[udp must satisfied the concept of transport_protocol]", "[udp.concept]") {
  CHECK(transport_protocol<net::ip::udp>);
}

TEST_CASE("[IPv4 constructor]", "[udp.ctor]") {
  auto proto = net::ip::udp::v4();
  CHECK(proto.family() == AF_INET);
  CHECK(proto.type() == SOCK_DGRAM);
  CHECK(proto.protocol() == IPPROTO_UDP);
}

TEST_CASE("[IPv6 constructor]", "[udp.ctor]") {
  auto proto = net::ip::udp::v6();
  CHECK(proto.family() == AF_INET6);
  CHECK(proto.type() == SOCK_DGRAM);
  CHECK(proto.protocol() == IPPROTO_UDP);
}

TEST_CASE("[comparision of udp]", "[udp.operator=]") {
  CHECK(net::ip::udp::v4() == net::ip::udp::v4());
  CHECK(net::ip::udp::v6() != net::ip::udp::v4());
}
