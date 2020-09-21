# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_subdirectory(core)

oeedl_file(${EDL_PATH} host edl_host_src)

set(edl_include_dir ${CMAKE_CURRENT_BINARY_DIR})

add_library(${CMAKE_PROJECT_NAME}_server_host_lib
    ${edl_host_src}
    enclave_error.h
    enclave_error.cc
    enclave.h
    enclave.cc
    environment.h
    environment.cc
    json_handling.h
    json_handling.cc
    request_handler.h
    request_handler.cc
    threading.cc
)

target_include_directories(${CMAKE_PROJECT_NAME}_server_host_lib
    PRIVATE
    ${edl_include_dir}
    PUBLIC
    ${ROOT_INCLUDE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}_server_host_lib PUBLIC
    ${CMAKE_PROJECT_NAME}_shared
    onnxruntime_server_http_core_lib
    spdlog::spdlog
    openenclave::oehost
)

add_executable(${CMAKE_PROJECT_NAME}_server_host
    main.cc
    server_configuration.h
)

target_link_libraries(${CMAKE_PROJECT_NAME}_server_host PRIVATE
    ${CMAKE_PROJECT_NAME}_server_host_lib
)

install(TARGETS ${CMAKE_PROJECT_NAME}_server_host
        RUNTIME DESTINATION bin)
