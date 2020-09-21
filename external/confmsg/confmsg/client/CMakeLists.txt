# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_library(${PROJECT_NAME}_client STATIC
    api.cc
    )

target_include_directories(${PROJECT_NAME}_client
    PRIVATE
    ${ROOT_INTERNAL_INCLUDE_DIR}
    PUBLIC
    ${ROOT_PUBLIC_INCLUDE_DIR}
    )

target_link_libraries(${PROJECT_NAME}_client PRIVATE
    ${PROJECT_NAME}_shared
    evercrypt
    mbedx509
    flatbuffers
    )
if (BUILD_ENCLAVE)
    target_link_libraries(${PROJECT_NAME}_client PRIVATE
        oe-enclave
        )
endif()

# TODO doesn't belong here
if (NOT BUILD_ENCLAVE)
    if (NOT TARGET openenclave::oeedger8r)
        # Import OE optionally.
        find_package(openenclave CONFIG)
        if (openenclave_FOUND)
            message(STATUS "Using Open Enclave ${openenclave_VERSION} from ${openenclave_CONFIG}")
        endif()
    endif()
endif()

if (NOT BUILD_ENCLAVE)
    if (TARGET openenclave::oehostverify)
        target_compile_definitions(${PROJECT_NAME}_client PRIVATE HAVE_OE_HOST_VERIFY)
        target_link_libraries(${PROJECT_NAME}_client PRIVATE openenclave::oehostverify)
        message(STATUS "openenclave::oehostverify found, building with host-side quote verification")
    else()
        message(WARNING "openenclave::oehostverify not found, building without host-side quote verification")
    endif()
endif()