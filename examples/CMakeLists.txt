cmake_minimum_required(VERSION 3.22.1)

# Add a directory to save the generated executable file. 
# If the directory is not specified, it is stored with the intermediate
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/examples/)

# General libraries.
set (LIBS fmt)

# Example: Echo Server.
message("building example: echo server")
set(ECHO_SERVER_SRC echo_server/echo_server.cpp)
add_executable(echo_server ${ECHO_SERVER_SRC})
target_link_libraries(echo_server ${LIBS})
# target_compile_options(echo_server PRIVATE -fconcepts-diagnostics-depth=3)

