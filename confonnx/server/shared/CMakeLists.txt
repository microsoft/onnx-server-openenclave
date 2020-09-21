# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_library(${CMAKE_PROJECT_NAME}_shared STATIC
    ca_certificates.h
    curl_helper.h
    curl_helper.cc
    http_challenge.h
    http_challenge.cc
    key_vault_config.h
    key_vault_config.cc
    util.h
    util.cc
)
target_link_libraries(${CMAKE_PROJECT_NAME}_shared PRIVATE
    libcurl
    nlohmann_json::nlohmann_json
    cpp-base64
    confmsg::confmsg_shared
)
target_include_directories(${CMAKE_PROJECT_NAME}_shared PUBLIC
    ${ROOT_INCLUDE_DIR}
)
if (BUILD_ENCLAVE)
    target_link_libraries(${CMAKE_PROJECT_NAME}_shared PRIVATE
        openenclave::oeenclave
        openenclave::oelibcxx
        )
endif()
