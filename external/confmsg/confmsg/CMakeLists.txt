# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_subdirectory(shared)
if (BUILD_SERVER_LIB)
    add_subdirectory(server)
    set(server_target_name ${PROJECT_NAME}_server)
endif()
if (BUILD_CLIENT_LIB)
    add_subdirectory(client)
    set(client_target_name ${PROJECT_NAME}_client)
endif()

# For import in dependent projects.
if (BUILD_ENCLAVE)
    set(extra_exports oe-enclave)
endif()
export(
    TARGETS
        ${server_target_name}
        ${client_target_name}
        ${PROJECT_NAME}_shared
        # Need to be included as we build them in this project.
        evercrypt
        mbedx509
        mbedcrypto
        flatbuffers
        ${extra_exports}
    NAMESPACE ${PROJECT_NAME}::
    FILE ${CMAKE_BINARY_DIR}/targets.cmake)

if (BUILD_TESTING)
    add_subdirectory(test)
endif()
