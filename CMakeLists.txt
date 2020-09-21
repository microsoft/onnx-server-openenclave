# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# We need to build the project twice in two build modes: enclave and host.
# Combining both into one build is not easy as we want to partly use
# the same libraries in the enclave and the host, and currently we simply
# monkey-patch the CMake targets to make them enclave-compatible.
# Once a library target is patched, it cannot be used for host targets anymore.
# An alternative is to create forks of the respective libraries with
# target names renamed, but this has its own problems.

cmake_minimum_required(VERSION 3.13)

project(confonnx-superbuild C CXX)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    # See external/onnxruntime/cmake/external/openenclave.cmake.
    # The work-around used there breaks with 3.16.
    message(FATAL_ERROR "Only CMake < 3.16 is currently supported")
endif()

option(BUILD_CLIENT "Builds the Python client package (with native extension)" ON)
option(BUILD_SERVER "Builds the server including the enclave binary" ON)
option(BUILD_TESTING "Build and run tests" ON)
option(WITH_LIBSKR "Build with libskr library" OFF)
option(ENABLE_CONFMSG_TESTS "Build and run confmsg tests" ON)
option(ENABLE_ENCLAVE_TESTS "Build and run tests that require SGX hardware" ON)
option(ENABLE_CMAKE_GRAPHVIZ "Generate target dependency graphs." OFF)
option(COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)
set(PYTHON_EXECUTABLE "" CACHE STRING "Python to use for building the client package")

if (NOT BUILD_CLIENT AND NOT BUILD_SERVER)
    message(FATAL_ERROR "BUILD_CLIENT and BUILD_SERVER cannot be both OFF")
endif()

# clang-format all files
file(GLOB_RECURSE src_files
    confonnx/*.cc
    confonnx/*.h
    external/confmsg/confmsg/*.cc
    external/confmsg/confmsg/*.h
    )
find_program(CLANG_FORMAT NAMES clang-format clang-format-7 clang-format-8)
if (CLANG_FORMAT)
    message(STATUS "clang-format found, will format source files now")
    execute_process(COMMAND ${CLANG_FORMAT} -i -style=file ${src_files})
else()
    message(STATUS "clang-format NOT found, skipping automatic formatting")
endif()

include(CTest)
include(ExternalProject)

if (NOT BUILD_TESTING)
    set(ENABLE_CONFMSG_TESTS OFF CACHE BOOL "" FORCE)
endif()

if ("$ENV{VERBOSE}" STREQUAL "1")
    set(ctest_out_arg -V)
else()
    set(ctest_out_arg --output-on-failure)
endif()

if (NOT "$ENV{TEST_FILTER}" STREQUAL "")
    set(ctest_filter --tests-regex $ENV{TEST_FILTER})
endif()

set(COMMON_CMAKE_CACHE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
)
if (ENABLE_CMAKE_GRAPHVIZ)
    set(COMMON_CMAKE_ARGS
        --graphviz=graphviz.dot
    )
endif()

# We need protoc within the enclave build, however we can't build it
# then because we're monkey-patching the protobuf libraries and disable
# building protoc. Therefore, let's build it separately first.
ExternalProject_Add(protobuf-compiler
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/onnxruntime/cmake/external/protobuf/cmake
    CMAKE_CACHE_ARGS
        ${COMMON_CMAKE_CACHE_ARGS}
        -DCMAKE_INSTALL_PREFIX:STRING=<INSTALL_DIR>
        -Dprotobuf_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
    )

ExternalProject_Get_property(protobuf-compiler INSTALL_DIR)
set(PROTOC_PATH ${INSTALL_DIR}/bin/protoc)

# confmsg is treated as fully separate project.
if (BUILD_SERVER)
    ExternalProject_Add(confmsg-enclave
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/confmsg
        CMAKE_CACHE_ARGS
            ${COMMON_CMAKE_CACHE_ARGS}
            -DBUILD_MODE:STRING=enclave
            -DBUILD_CLIENT_LIB:BOOL=OFF
            -DBUILD_SERVER_LIB:BOOL=ON
            -DBUILD_TESTING:BOOL=${ENABLE_CONFMSG_TESTS}
            -DENABLE_ENCLAVE_TESTS:BOOL=${ENABLE_ENCLAVE_TESTS}
            -Dopenenclave_DIR:STRING=${openenclave_DIR}
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        )

    ExternalProject_Get_property(confmsg-enclave BINARY_DIR)
    set(confmsg_enclave_BUILD_DIR ${BINARY_DIR})
    set(confmsg_DEPENDS DEPENDS confmsg-enclave)
endif()

ExternalProject_Add(confmsg-host
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/confmsg
    CMAKE_CACHE_ARGS
        ${COMMON_CMAKE_CACHE_ARGS}
        -DCMAKE_INSTALL_PREFIX:STRING=<INSTALL_DIR>
        -DBUILD_MODE:STRING=host
        -DBUILD_CLIENT_LIB:BOOL=${BUILD_CLIENT}
        -DBUILD_SERVER_LIB:BOOL=OFF
        -DBUILD_TESTING:BOOL=${ENABLE_CONFMSG_TESTS}
        -DENABLE_ENCLAVE_TESTS:BOOL=${ENABLE_ENCLAVE_TESTS}
        -Dopenenclave_DIR:STRING=${openenclave_DIR}
        -DENCLAVE_BUILD_DIR:STRING=${confmsg_enclave_BUILD_DIR}
    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    ${confmsg_DEPENDS}
    )

ExternalProject_Get_property(confmsg-host BINARY_DIR)
set(confmsg_host_BUILD_DIR ${BINARY_DIR})

if (ENABLE_CONFMSG_TESTS)
    add_test(NAME confmsg-tests
        COMMAND ${CMAKE_CTEST_COMMAND}
            -C $<CONFIGURATION>
            ${ctest_filter}
            --output-log ${CMAKE_BINARY_DIR}/confmsg-tests.log
            ${ctest_out_arg}
        WORKING_DIRECTORY ${confmsg_host_BUILD_DIR}
    )
endif()

# libskr is treated as fully separate project.
if (BUILD_SERVER AND WITH_LIBSKR)
    ExternalProject_Add(libskr-enclave
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/libskr
        CMAKE_CACHE_ARGS
            ${COMMON_CMAKE_CACHE_ARGS}
            -DBUILD_ENCLAVES:BOOL=ON
            -DBUILD_NATIVE:BOOL=OFF
            -DALLOW_REPORT_FAILURE:BOOL=ON
            -DBUILD_TESTS:BOOL=OFF
            -DBUILD_SAMPLES:BOOL=OFF
            -DOpenEnclave_DIR:STRING=${openenclave_DIR}
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        )

    ExternalProject_Get_property(libskr-enclave BINARY_DIR)
    set(libskr_enclave_BUILD_DIR ${BINARY_DIR})
    set(libskr_DEPENDS libskr-enclave)
endif()

set(ONNXRT_EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external/onnxruntime/cmake/external)

if (BUILD_SERVER)
    ExternalProject_Add(confonnx-enclave
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/confonnx
        INSTALL_DIR confonnx-enclave-install
        PATCH_COMMAND
            patch --directory=${ONNXRT_EXTERNAL_DIR}/protobuf
                -i ${CMAKE_SOURCE_DIR}/external/patches/protobuf.patch
                -p 1 -N -r /dev/null || true
        CMAKE_CACHE_ARGS
            ${COMMON_CMAKE_CACHE_ARGS}
            -DCMAKE_INSTALL_PREFIX:STRING=<INSTALL_DIR>
            -DBUILD_MODE:STRING=enclave
            -DBUILD_SERVER:BOOL=${BUILD_SERVER}
            -DBUILD_CLIENT:BOOL=${BUILD_CLIENT}
            -DBUILD_TESTING:BOOL=${BUILD_TESTING}
            -DENABLE_ENCLAVE_TESTS:BOOL=${ENABLE_ENCLAVE_TESTS}
            -DWITH_LIBSKR:BOOL=${WITH_LIBSKR}
            -Dopenenclave_DIR:STRING=${openenclave_DIR}
            -Dconfmsg_enclave_BUILD_DIR:STRING=${confmsg_enclave_BUILD_DIR}
            -Dlibskr_enclave_BUILD_DIR:STRING=${libskr_enclave_BUILD_DIR}
            -DONNX_CUSTOM_PROTOC_EXECUTABLE:STRING=${PROTOC_PATH}
            -DCOLORED_OUTPUT:BOOL=${COLORED_OUTPUT}
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        BUILD_ALWAYS ON
        DEPENDS protobuf-compiler confmsg-enclave ${libskr_DEPENDS}
        )

    ExternalProject_Get_property(confonnx-enclave BINARY_DIR)
    ExternalProject_Get_property(confonnx-enclave INSTALL_DIR)

    install(DIRECTORY ${INSTALL_DIR}/ DESTINATION . USE_SOURCE_PERMISSIONS)

    set(confonnx_DEPENDS confonnx-enclave)
endif()

ExternalProject_Add(confonnx-host
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/confonnx
    INSTALL_DIR confonnx-host-install
    CMAKE_CACHE_ARGS
        ${COMMON_CMAKE_CACHE_ARGS}
        -DCMAKE_INSTALL_PREFIX:STRING=<INSTALL_DIR>
        -DBUILD_MODE:STRING=host
        -DBUILD_SERVER:BOOL=${BUILD_SERVER}
        -DBUILD_CLIENT:BOOL=${BUILD_CLIENT}
        -DBUILD_TESTING:BOOL=${BUILD_TESTING}
        -DENABLE_ENCLAVE_TESTS:BOOL=${ENABLE_ENCLAVE_TESTS}
        -DENCLAVE_BUILD_DIR:STRING=${BINARY_DIR}
        -Dopenenclave_DIR:STRING=${openenclave_DIR}
        -Dconfmsg_host_BUILD_DIR:STRING=${confmsg_host_BUILD_DIR}
        -DPYTHON_EXECUTABLE:STRING=${PYTHON_EXECUTABLE}
        -DCOLORED_OUTPUT:BOOL=${COLORED_OUTPUT}
    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
    BUILD_ALWAYS ON
    DEPENDS ${confonnx_DEPENDS} confmsg-host
    )

ExternalProject_Get_property(confonnx-host BINARY_DIR)
ExternalProject_Get_property(confonnx-host INSTALL_DIR)

install(DIRECTORY ${INSTALL_DIR}/ DESTINATION . USE_SOURCE_PERMISSIONS)

if (BUILD_TESTING)
    add_test(NAME confonnx-tests
        COMMAND ${CMAKE_CTEST_COMMAND}
            -C $<CONFIGURATION>
            ${ctest_filter}
            --exclude-regex pytest
            --output-log ${CMAKE_BINARY_DIR}/confonnx-tests.log
            ${ctest_out_arg}
        WORKING_DIRECTORY ${BINARY_DIR}
    )
    # Run monolithic pytests separately with -V to see progress.
    add_test(NAME confonnx-pytests
        COMMAND ${CMAKE_CTEST_COMMAND}
            -C $<CONFIGURATION>
            --tests-regex pytest
            --output-log ${CMAKE_BINARY_DIR}/confonnx-pytests.log
            -V
        WORKING_DIRECTORY ${BINARY_DIR}
    )
endif()
