# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

include(fbs_file)

fbs_file(protocol.fbs fbs_gen)

# Only run clang-tidy if using Clang, as evercrypt leaks compiler includes in its public headers,
# for example EverCrypt_AEAD.h -> libintvector.h -> immintrin.h.
# This is problematic as we manually add the compiler include folder (see openenclave.cmake).
# http://clang-developers.42468.n3.nabble.com/clang-tidy-amp-SSE-with-GCC-toolchain-td4059399.html
if (BUILD_ENCLAVE AND NOT CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    set(CMAKE_CXX_CLANG_TIDY)
endif()

add_library(${PROJECT_NAME}_shared STATIC
    crypto.cc
    util.cc
    ${fbs_gen}
    )

target_include_directories(${PROJECT_NAME}_shared
    PRIVATE
    ${ROOT_INTERNAL_INCLUDE_DIR}
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR} # flatbuffers generated header
    ${ROOT_PUBLIC_INCLUDE_DIR}
    )

target_link_libraries(${PROJECT_NAME}_shared
    PRIVATE
    evercrypt
    flatbuffers
    )

if (BUILD_ENCLAVE)
    target_link_libraries(${PROJECT_NAME}_shared PRIVATE openenclave::mbedx509)
else()
    target_link_libraries(${PROJECT_NAME}_shared PRIVATE mbedx509)
endif()