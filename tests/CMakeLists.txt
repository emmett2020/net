add_compile_options(-fno-access-control)
cmake_minimum_required(VERSION 3.22.0)

set (LIBS Catch2::Catch2WithMain fmt)

add_executable(test_basic_socket test_basic_socket.cpp)
target_link_libraries(test_basic_socket ${LIBS})

add_executable(test_address_v4 test_address_v4.cpp)
target_link_libraries(test_address_v4  ${LIBS})

add_executable(test_address_v6 test_address_v6.cpp)
target_link_libraries(test_address_v6  ${LIBS})

add_executable(test_address test_address.cpp)
target_link_libraries(test_address  ${LIBS})

add_executable(test_basic_endpoint test_basic_endpoint.cpp)
target_link_libraries(test_basic_endpoint  ${LIBS})

add_executable(test_socket_option test_socket_option.cpp)
target_link_libraries(test_socket_option  ${LIBS})

add_executable(test_io_control test_io_control.cpp)
target_link_libraries(test_io_control  ${LIBS})

add_executable(test_socket_base test_socket_base.cpp)
target_link_libraries(test_socket_base  ${LIBS})

add_executable(test_basic_socket_acceptor test_basic_socket_acceptor.cpp)
target_link_libraries(test_basic_socket_acceptor ${LIBS})

add_executable(test_basic_stream_socket test_basic_stream_socket.cpp)
target_link_libraries(test_basic_stream_socket ${LIBS})

add_executable(test_basic_datagram_socket test_basic_datagram_socket.cpp)
target_link_libraries(test_basic_datagram_socket ${LIBS})

add_executable(test_epoll_socket_io_base_op test_epoll_socket_io_base_op.cpp)
target_link_libraries(test_epoll_socket_io_base_op ${LIBS})

add_executable(test_epoll_socket_accept_op test_epoll_socket_accept_op.cpp)
target_link_libraries(test_epoll_socket_accept_op ${LIBS})

add_executable(test_epoll_socket_recv_some_op test_epoll_socket_recv_some_op.cpp)
target_link_libraries(test_epoll_socket_recv_some_op ${LIBS})

add_executable(test_ip_tcp test_ip_tcp.cpp)
target_link_libraries(test_ip_tcp ${LIBS})

add_executable(test_ip_udp test_ip_udp.cpp)
target_link_libraries(test_ip_udp ${LIBS})

add_executable(test_epoll_context test_epoll_context.cpp)
target_link_libraries(test_epoll_context ${LIBS})

add_executable(test_eventfd_interrupter test_eventfd_interrupter.cpp)
target_link_libraries(test_eventfd_interrupter ${LIBS})

add_executable(test_monotonic_clock test_monotonic_clock.cpp)
target_link_libraries(test_monotonic_clock ${LIBS})

add_executable(test_buffer test_buffer.cpp)
target_link_libraries(test_buffer ${LIBS})

add_executable(test_buffer_sequence_adapter test_buffer_sequence_adapter.cpp)
target_link_libraries(test_buffer_sequence_adapter ${LIBS})