# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

oeedl_file(${EDL_PATH} enclave edl_enclave_src)

set(edl_include_dir ${CMAKE_CURRENT_BINARY_DIR})

add_library(${CMAKE_PROJECT_NAME}_server_enclave_lib
    key_vault_provider.h
    key_vault_provider.cc
    core/serializing/mem_buffer.h
    core/serializing/tensorprotoutils.cc
    core/serializing/tensorprotoutils.h
    core/converter.cc
    core/converter.h
    core/environment.cc
    core/environment.h
    core/executor.cc
    core/executor.h
    core/util.cc
    core/util.h
)
if (WITH_LIBSKR)
    target_sources(${CMAKE_PROJECT_NAME}_server_enclave_lib
        key_vault_hsm_provider.h
        key_vault_hsm_provider.cc
    )
endif()

target_compile_definitions(${CMAKE_PROJECT_NAME}_server_enclave_lib PUBLIC OE_BUILD_ENCLAVE)

# Ignore protobuf deprecation warnings from transitively included onnx header files.
target_compile_options(${CMAKE_PROJECT_NAME}_server_enclave_lib PRIVATE -Wno-deprecated-declarations)

target_link_libraries(${CMAKE_PROJECT_NAME}_server_enclave_lib PRIVATE
    ${CMAKE_PROJECT_NAME}_shared
    onnxruntime_openenclave
    server_proto
    spdlog::spdlog
    libcurl
    nlohmann_json::nlohmann_json
    confmsg::confmsg_server
    openenclave::oeenclave
    openenclave::oelibcxx
    openenclave::mbedx509
)
if (WITH_LIBSKR)
    target_link_libraries(${CMAKE_PROJECT_NAME}_server_enclave_lib PRIVATE skr::skr)
endif()

target_include_directories(${CMAKE_PROJECT_NAME}_server_enclave_lib PRIVATE
    ${ROOT_INCLUDE_DIR}
)

add_executable(${CMAKE_PROJECT_NAME}_server_enclave
    ${edl_enclave_src}
    enclave.cc
    threading.cc
    threading.h
    props.cc
)

set_source_files_properties(
    props.cc
    PROPERTIES COMPILE_FLAGS -Wno-error=missing-field-initializers
)

target_include_directories(${CMAKE_PROJECT_NAME}_server_enclave PRIVATE
    ${edl_include_dir}
    ${ROOT_INCLUDE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}_server_enclave PRIVATE
    ${CMAKE_PROJECT_NAME}_server_enclave_lib
    ${CMAKE_PROJECT_NAME}_shared
    onnxruntime_openenclave
    server_proto
    spdlog::spdlog
    libcurl
    confmsg::confmsg_server
    openenclave::oeenclave
    openenclave::oelibcxx
    openenclave::mbedx509
)
if (WITH_LIBSKR)
    target_link_libraries(${CMAKE_PROJECT_NAME}_server_enclave PRIVATE skr::skr)
endif()

install(TARGETS ${CMAKE_PROJECT_NAME}_server_enclave
        RUNTIME  DESTINATION bin)
