# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

include(oeedl_file)

if (BUILD_ENCLAVE)
    # Enclave image of test enclave.
    oeedl_file(${CMAKE_CURRENT_SOURCE_DIR}/test.edl enclave gen)

    add_executable(${CMAKE_PROJECT_NAME}_test_enclave
        enclave.cc
        props.cc
        threading.cc
        ../helpers/crypto_helpers.cc
        ${gen}
        )
    # https://github.com/openenclave/openenclave/issues/2080
    set_source_files_properties(
        props.cc
        PROPERTIES COMPILE_FLAGS -Wno-error=missing-field-initializers
    )
    target_link_libraries(${CMAKE_PROJECT_NAME}_test_enclave PRIVATE
        ${CMAKE_PROJECT_NAME}_server_enclave_lib
        ${CMAKE_PROJECT_NAME}_shared
        openenclave::oeenclave
        openenclave::oelibcxx
        confmsg::confmsg_server
        )
    if (WITH_LIBSKR)
        target_link_libraries(${CMAKE_PROJECT_NAME}_test_enclave PRIVATE skr::skr)
    endif()
    target_include_directories(${CMAKE_PROJECT_NAME}_test_enclave PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${ROOT_INCLUDE_DIR}
        )
    # FIXME shouldn't be here
    target_link_options(${CMAKE_PROJECT_NAME}_test_enclave PRIVATE
        "LINKER:-z,muldefs"
        )

    install(TARGETS ${CMAKE_PROJECT_NAME}_test_enclave
        RUNTIME  DESTINATION tests)
else()
    # Host part of test enclave.
    oeedl_file(${CMAKE_CURRENT_SOURCE_DIR}/test.edl host gen)

    # Currently no actual library, may flesh out later.
    add_library(${CMAKE_PROJECT_NAME}_test_enclave_host
        ${gen}
        )
    target_include_directories(${CMAKE_PROJECT_NAME}_test_enclave_host PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        )
    target_link_libraries(${CMAKE_PROJECT_NAME}_test_enclave_host PRIVATE
        openenclave::oehost
        )
    target_sources(${CMAKE_PROJECT_NAME}_test_enclave_host PRIVATE
        threading_host.cc
    )
endif()