# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_executable(${CMAKE_PROJECT_NAME}_encrypt_model
    encrypt_model.cc
    )
target_link_libraries(${CMAKE_PROJECT_NAME}_encrypt_model PRIVATE
    confmsg::confmsg_shared
    )
target_include_directories(${CMAKE_PROJECT_NAME}_encrypt_model PRIVATE
    ${ROOT_INCLUDE_DIR}
)
install(TARGETS ${CMAKE_PROJECT_NAME}_encrypt_model RUNTIME DESTINATION tools)