# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_library(${PROJECT_NAME}_server STATIC
    api.cc
    evercrypt_exit.c
    )

target_include_directories(${PROJECT_NAME}_server
    PRIVATE
    ${ROOT_INTERNAL_INCLUDE_DIR}
    PUBLIC
    ${ROOT_PUBLIC_INCLUDE_DIR}
    )

target_link_libraries(${PROJECT_NAME}_server PRIVATE
    ${PROJECT_NAME}_shared
    evercrypt
    flatbuffers
    )
if (BUILD_ENCLAVE)
    target_link_libraries(${PROJECT_NAME}_server PRIVATE
        oe-enclave
        )
endif()