# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# EverCrypt is used in Open Enclave performance testing.
# Note that Open Enclave also provides crypto libraries (mbedtls) but currently they lack in performance.

set(EVERCRYPT_DIST ${PROJECT_SOURCE_DIR}/external/evercrypt/dist)
set(EVERCRYPT_ROOT ${EVERCRYPT_DIST}/gcc-compatible)
set(KREMLIN_ROOT ${EVERCRYPT_DIST}/kremlin)

if (MSVC)
    set(VARIANT "msvc.asm")
elseif (APPLE)
    set(VARIANT "darwin.S")
else()
    set(VARIANT "linux.S")
endif()

file(GLOB_RECURSE EVERCRYPT_SRC_C CONFIGURE_DEPENDS "${EVERCRYPT_ROOT}/*.c" )
file(GLOB_RECURSE EVERCRYPT_SRC_ASM CONFIGURE_DEPENDS "${EVERCRYPT_ROOT}/*-${VARIANT}")

list(REMOVE_ITEM EVERCRYPT_SRC_C "${EVERCRYPT_ROOT}/evercrypt_bcrypt.c")
list(REMOVE_ITEM EVERCRYPT_SRC_C "${EVERCRYPT_ROOT}/evercrypt_openssl.c")

add_library(evercrypt STATIC
    ${EVERCRYPT_SRC_C}
    ${EVERCRYPT_SRC_ASM}
    )
target_compile_options(evercrypt PRIVATE
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -mavx2
    )
target_include_directories(evercrypt PUBLIC
    ${EVERCRYPT_ROOT}
    ${KREMLIN_ROOT}/include
    ${KREMLIN_ROOT}/kremlib/dist/minimal
    )
set_property(TARGET evercrypt PROPERTY POSITION_INDEPENDENT_CODE ON)
