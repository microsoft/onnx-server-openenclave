# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

include(oeedl_file)

set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)
set(EDL_PATH ${SHARED_DIR}/server.edl)

add_subdirectory(shared)

if (BUILD_ENCLAVE)
    add_subdirectory(enclave)
    export(TARGETS ${CMAKE_PROJECT_NAME}_server_enclave FILE targets.cmake)
else()
    add_subdirectory(host)
endif()
