# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if (NOT ENABLE_ENCLAVE_TESTS)
    return()
endif()

add_subdirectory(test_enclave)
if (BUILD_ENCLAVE)
    export(TARGETS ${CMAKE_PROJECT_NAME}_test_enclave FILE targets.cmake)
    # No further enclave-side parts below.
    return()
endif()

if (NOT BUILD_SERVER)
    # Tests require the server.
    message(STATUS "Skipping unit tests as server is not built")
    return()
endif()

# C++ unit & integration tests

add_executable(${CMAKE_PROJECT_NAME}_tests
    helpers/helpers.h
    helpers/pb_diff.h
    helpers/pb_diff.cc
    helpers/pb_loader.h
    helpers/protobuf_parsing_utils.h
    helpers/protobuf_parsing_utils.cc
    helpers/prettyprint.h
    helpers/prettyprint.cc
    helpers/tensorproto_converter.h
    helpers/tensorproto_converter.cc
    helpers/tensorproto_util.cc
    helpers/tensorproto_util.h
    helpers/crypto_helpers.h
    helpers/crypto_helpers.cc
    helpers/env.h
    helpers/env.cc
    test_main.cc
    test_config.h
    test_config.cc
    test_key_vault_config.cc
    predict_request_tests.cc
    key_vault_tests.cc
    curl_tests.cc
    # FIXME create library for unit tests (or don't run on host, like HSM)
    ../server/enclave/key_vault_provider.cc
    )
if (WITH_LIBSKR)
    target_sources(${CMAKE_PROJECT_NAME}_tests 
        key_vault_hsm_tests.cc
        key_vault_hsm_host_provider.h
        key_vault_hsm_host_provider.cc
    )
endif()
target_link_libraries(${CMAKE_PROJECT_NAME}_tests
    ${CMAKE_PROJECT_NAME}_server_host_lib
    ${CMAKE_PROJECT_NAME}_test_enclave_host
    ${CMAKE_PROJECT_NAME}_shared
    confmsg::confmsg_client
    onnx
    onnx_proto
    server_proto
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    gtest
    )
# Normally, the executable would export all symbols and make them
# available to the dynamic loader. This includes symbols of dependencies.
# Without going into all details, this is problematic here because:
# - The test executable links against:
#   - libprotobuf.a from the ONNX RT tree
#   - libsgx_urts.so, which links against libprotobuf.so.10 from the system
# - The two protobuf libraries are different versions.
# - During loading of the executable, the symbols of both libraries are
#   available for lookup and in one namespace.
# - The first one that is found wins, which then leads to segfaults.
# The solution is to control symbol visibility so that the two protobuf
# versions can be used at the same time. This trick would not work
# if we had used two dynamically linked protobuf versions.
# Since we don't want to link against the executable, we simply turn all
# its exported symbols into local/hidden symbols. If this was a shared library
# then we would have to additionally list all public symbols that should
# be exported (which would exclude any of the dependencies).
target_link_options(${CMAKE_PROJECT_NAME}_tests PRIVATE
    LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/no_symbols.txt
    )

install(TARGETS ${CMAKE_PROJECT_NAME}_tests
        RUNTIME  DESTINATION tests)

set(server_enclave_path $<TARGET_FILE:${CMAKE_PROJECT_NAME}_server_enclave>)
set(test_enclave_path $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test_enclave>)

set(onnxrt_testdata_dir ${CMAKE_SOURCE_DIR}/../external/onnxruntime/onnxruntime/test/testdata)

include(GoogleTest)
gtest_discover_tests(${CMAKE_PROJECT_NAME}_tests
    TEST_PREFIX confonnx-
    EXTRA_ARGS ${server_enclave_path} ${test_enclave_path} ${onnxrt_testdata_dir}
    PROPERTIES
      ENVIRONMENT "OE_LOG_LEVEL=INFO"
      SKIP_RETURN_CODE 42
)

# Python tests

if (BUILD_CLIENT)
    if ("$ENV{VERBOSE}" STREQUAL 1)
        # always print output, not just on test failures
        set(pytest_output -s)
    else()
        # Python warnings often come from dependencies
        set(pytest_output --disable-warnings)
    endif()
    if (NOT "$ENV{TEST_FILTER}" STREQUAL "")
        set(pytest_filter -k $ENV{TEST_FILTER})
    endif()
    add_test(NAME ${CMAKE_PROJECT_NAME}_pytests
        COMMAND ${PYTHON_EXECUTABLE} -u -m pytest ${pytest_filter} -v --durations=0 ${pytest_output}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    set(PY_CLIENT_DIR "${CMAKE_BINARY_DIR}/python")
    set(server_host_path $<TARGET_FILE:${CMAKE_PROJECT_NAME}_server_host>)
    set_property(
        TEST ${CMAKE_PROJECT_NAME}_pytests
        PROPERTY ENVIRONMENT
            PYTHONPATH=${PY_CLIENT_DIR}
            REPO_ROOT=${REPO_ROOT_DIR}
            SERVER_HOST_PATH=${server_host_path}
            SERVER_ENCLAVE_PATH=${server_enclave_path}
    )
    if (pytest_filter)
        # pytest exits with 5 if no tests were collected, ignore that when filtering.
        # https://github.com/pytest-dev/pytest/blob/77a995ffada/src/_pytest/main.py#L41
        set_property(
            TEST ${CMAKE_PROJECT_NAME}_pytests
            PROPERTY SKIP_RETURN_CODE 5
        )
    endif()
endif()