# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

project(confmsg C CXX ASM) # ASM needed for evercrypt

set(BUILD_MODE "enclave" CACHE STRING "'enclave' or 'host'")
set(ENCLAVE_BUILD_DIR "" CACHE STRING "Build directory of enclave build (required for tests)")
option(BUILD_CLIENT_LIB "Builds the client library" ON)
option(BUILD_SERVER_LIB "Builds the server library" ON)
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_ENCLAVE_TESTS "Test enclave-mode client/server libs using SGX hardware" ON)
option(COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (BUILD_MODE STREQUAL "enclave")
    set(BUILD_ENCLAVE ON)
elseif (BUILD_MODE STREQUAL "host")
    set(BUILD_ENCLAVE OFF)
else()
    message(FATAL_ERROR "BUILD_MODE invalid")
endif()

if (BUILD_TESTING)
    include(CTest)
    if (NOT BUILD_CLIENT_LIB)
        set(BUILD_CLIENT_LIB ON CACHE BOOL "" FORCE)
        message(STATUS "Enabling BUILD_CLIENT_LIB because tests require it")
    endif()
    if (NOT BUILD_SERVER_LIB)
        set(BUILD_SERVER_LIB ON CACHE BOOL "" FORCE)
        message(STATUS "Enabling BUILD_SERVER_LIB because tests require it")
    endif()
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

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if (BUILD_ENCLAVE OR (BUILD_TESTING AND ENABLE_ENCLAVE_TESTS))
    include(openenclave)
endif()
include(evercrypt)

set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/mbedtls EXCLUDE_FROM_ALL)

set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/flatbuffers EXCLUDE_FROM_ALL)

if (NOT BUILD_ENCLAVE)
    add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
endif()

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-7 clang-tidy-8)
if (CLANG_TIDY)
    message(STATUS "clang-tidy found, running checks")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
else()
    message(STATUS "clang-tidy NOT found, skipping checks")
endif()

set(ROOT_INTERNAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME})
set(ROOT_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Turn dependencies into OE libraries by monkey-patching.
if (BUILD_ENCLAVE)
    cmake_policy(SET CMP0079 NEW)

    set(oe_libraries
        evercrypt
        flatbuffers
      )
    foreach(oe_lib ${oe_libraries})
      target_link_libraries(${oe_lib} PUBLIC oe-enclave)
    endforeach()
    
    set(oe_libraries_plain
        mbedx509
        mbedcrypto
        )
    foreach(oe_lib ${oe_libraries_plain})
      target_link_libraries(${oe_lib} oe-enclave)
    endforeach()
endif()

add_subdirectory(confmsg)
