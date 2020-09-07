# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(confonnx C CXX)

set(BUILD_MODE "enclave" CACHE STRING "'enclave' or 'host'")
set(ENCLAVE_BUILD_DIR "" CACHE STRING "Build directory of enclave build (used in host mode for testing)")
option(BUILD_SERVER "Build the server" ON)
option(BUILD_CLIENT "Build the client (Python with native extension)" OFF)
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_ENCLAVE_TESTS "Test the server enclave using SGX hardware" ON)
option(WITH_LIBSKR "Build with libskr library" OFF)
set(confmsg_enclave_BUILD_DIR "" CACHE STRING "Build directory of confmsg enclave build")
set(confmsg_host_BUILD_DIR "" CACHE STRING "Build directory of confmsg host build")
set(libskr_enclave_BUILD_DIR "" CACHE STRING "Build directory of libskr enclave build")
set(PYTHON_EXECUTABLE "" CACHE STRING "Python to use for building the client package")
option(COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)

if (BUILD_MODE STREQUAL "enclave")
    set(BUILD_ENCLAVE ON)
elseif (BUILD_MODE STREQUAL "host")
    set(BUILD_ENCLAVE OFF)
else()
    message(FATAL_ERROR "BUILD_MODE invalid")
endif()

# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
if (COLORED_OUTPUT)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics)
    endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND DEFINED ENV{LD})
    add_link_options(-fuse-ld=$ENV{LD})
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

if (NOT BUILD_ENCLAVE AND BUILD_SERVER AND BUILD_TESTING)
    include(${ENCLAVE_BUILD_DIR}/server/targets.cmake)
    include(${ENCLAVE_BUILD_DIR}/test/targets.cmake)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake)
if (BUILD_ENCLAVE)
    list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/../external/patches/cmake)
endif()

if (BUILD_SERVER)
    # find_package below can't be in external/ as imported targets are directory scoped.
    # OE 0.6 for the I/O subsystem (sockets, to use libcurl directly).
    find_package(openenclave 0.6 REQUIRED CONFIG)
elseif (NOT BUILD_ENCLAVE)
    # Import OE optionally.
    # 0.7 for host-side quote verification
    find_package(openenclave 0.7 CONFIG)
endif()
if (openenclave_FOUND)
    message(STATUS "Using Open Enclave ${openenclave_VERSION} from ${openenclave_CONFIG}")
endif()

add_subdirectory(../external ${CMAKE_BINARY_DIR}/external)

if (BUILD_ENCLAVE)
  include(${confmsg_enclave_BUILD_DIR}/targets.cmake)
  if (WITH_LIBSKR)
    include(${libskr_enclave_BUILD_DIR}/targets.cmake)
  endif()
else()
  include(${confmsg_host_BUILD_DIR}/targets.cmake)
endif()

find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-7 clang-tidy-8)
if (CLANG_TIDY)
    message(STATUS "clang-tidy found, running checks")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
else()
    message(STATUS "clang-tidy NOT found, skipping checks")
endif()

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

if (WITH_LIBSKR)
    add_compile_definitions(HAVE_LIBSKR)
endif()

set(REPO_ROOT_DIR ${CMAKE_SOURCE_DIR}/..)
set(ROOT_INCLUDE_DIR ${CMAKE_SOURCE_DIR})

add_subdirectory(protobuf)

if (BUILD_CLIENT)
    add_subdirectory(client)
endif()

if (BUILD_SERVER)
    add_subdirectory(server)
endif()

if (NOT BUILD_ENCLAVE)
    add_subdirectory(tools)
endif()

# Ignore warnings in test code.
set(CMAKE_CXX_CLANG_TIDY)

if (BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()
