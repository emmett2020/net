cmake_minimum_required(VERSION 3.22.1)

# Project version.
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
message("[net version]: " ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# Project properties.
project(net VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}" LANGUAGES CXX)

# Cpp standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS  True)

# Set variable for source files.
set(INCLUDE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(THIRDPARTY_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparties)
set(TEST_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(EXAMPLE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/examples)
message("[include      path]: "  ${INCLUDE_DIR})
message("[thirdparties path]: "  ${THIRDPARTY_DIR})
message("[unittests    path]: "  ${TEST_DIR})
message("[examples     path]: "  ${EXAMPLE_DIR})

# Include necessary directoires.
include_directories(${INCLUDE_DIR}/)
include_directories(${THIRDPARTY_DIR}/stdexec/include/)
include_directories(${THIRDPARTY_DIR}/status-code/include/)


# Collect source files.
file (GLOB_RECURSE  NET_FILES ${INCLUDE_DIR}/*.hpp)

# Export the main library.
add_library(net INTERFACE ${NET_FILES})

set_target_properties(net PROPERTIES LINKER_LANGUAGE CXX)

option(BUILD_NET_TESTING "build networking unittests" ON)
if (BUILD_NET_TESTING)
    message("build networking unittests on")
    add_subdirectory(${THIRDPARTY_DIR}/catch2/)
    add_subdirectory(${THIRDPARTY_DIR}/fmt)
    add_subdirectory(${TEST_DIR})
endif()

option(BUILD_NET_EXAMPLES "build networking examples" ON)
if (BUILD_NET_EXAMPLES)
    message("build networking examples on")

    # Add libfmt for a convenice usage.
    if (NOT BUILD_NET_TESTING)
        add_subdirectory(${THIRDPARTY_DIR}/fmt)
    endif()

    add_subdirectory(${EXAMPLE_DIR})
endif()



