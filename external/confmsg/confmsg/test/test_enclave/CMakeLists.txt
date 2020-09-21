# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

include(oeedl_file)

if (BUILD_ENCLAVE)
    # Enclave image of test enclave.
    oeedl_file(${CMAKE_CURRENT_SOURCE_DIR}/test.edl enclave gen)

    add_executable(${PROJECT_NAME}_test_enclave
        enclave.cc
        props.cc
        ${gen}
        )

    # https://github.com/openenclave/openenclave/issues/2080
    set_source_files_properties(
        props.cc
        PROPERTIES COMPILE_FLAGS -Wno-error=missing-field-initializers
    )

    target_link_libraries(${PROJECT_NAME}_test_enclave PRIVATE
        ${PROJECT_NAME}_server
        oe-enclave
        )
    target_include_directories(${PROJECT_NAME}_test_enclave PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${ROOT_INTERNAL_INCLUDE_DIR}
        ${ROOT_PUBLIC_INCLUDE_DIR}
        )
else()
    # Host part of test enclave.
    oeedl_file(${CMAKE_CURRENT_SOURCE_DIR}/test.edl host gen)

    # Currently no actual library, may flesh out later.
    add_library(${PROJECT_NAME}_test_enclave_host
        ${gen}
        )
    target_include_directories(${PROJECT_NAME}_test_enclave_host PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        )
    target_link_libraries(${PROJECT_NAME}_test_enclave_host PRIVATE
        oe-host
        )
endif()