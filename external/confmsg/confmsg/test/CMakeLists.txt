# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# TODO add host-only tests
if (NOT ENABLE_ENCLAVE_TESTS)
    return()
endif()

if (NOT BUILD_ENCLAVE)
    include(${ENCLAVE_BUILD_DIR}/targets-test.cmake)
endif()

add_subdirectory(test_enclave)

if (BUILD_ENCLAVE)
    export(
        TARGETS ${PROJECT_NAME}_test_enclave
        FILE ${CMAKE_BINARY_DIR}/targets-test.cmake)
endif()

if (NOT BUILD_ENCLAVE)
    add_executable(${PROJECT_NAME}_tests
        test_main.cc
        test_config.h
        test_config.cc
        integration_tests.cc
        )
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        ${PROJECT_NAME}_test_enclave_host
        ${PROJECT_NAME}_client
        ${PROJECT_NAME}_server
        ${PROJECT_NAME}_shared
        oe-host
        gtest
        )
    target_include_directories(${PROJECT_NAME}_tests PRIVATE
        ${ROOT_INTERNAL_INCLUDE_DIR}
        ${ROOT_PUBLIC_INCLUDE_DIR}
        )

    set(enclave_path $<TARGET_FILE:${PROJECT_NAME}_test_enclave>)

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests
        TEST_PREFIX confmsg-
        EXTRA_ARGS ${enclave_path}
        PROPERTIES ENVIRONMENT "OE_LOG_LEVEL=INFO"
    )
endif()
