# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# Due to an OE bug, calling find_package twice would fail,
# which is bad if this project is consumed elsewhere.
# TODO remove once https://github.com/Microsoft/openenclave/issues/1730 is fixed
if (NOT TARGET openenclave::oeedger8r)
    find_package(openenclave REQUIRED CONFIG)
    message(STATUS "Using Open Enclave ${openenclave_VERSION} from ${openenclave_CONFIG}")
endif()

add_library(oe-host INTERFACE)
target_link_libraries(oe-host INTERFACE
    openenclave::oehost
)

add_library(oe-enclave INTERFACE)
target_link_libraries(oe-enclave INTERFACE
    openenclave::oeenclave
    openenclave::oelibcxx
)

# Obtain default compiler include directory to gain access to intrinsics and other
# headers like cpuid.h.
execute_process(
    COMMAND /bin/bash ${CMAKE_CURRENT_LIST_DIR}/oe_c_compiler_inc_dir.sh ${CMAKE_C_COMPILER}
    OUTPUT_VARIABLE ONNXRUNTIME_C_COMPILER_INC
    ERROR_VARIABLE ONNXRUNTIME_ERR
)
if (NOT ONNXRUNTIME_ERR STREQUAL "")
    message(FATAL_ERROR ${ONNXRUNTIME_ERR})
endif()

# Works around https://gitlab.kitware.com/cmake/cmake/issues/19227#note_570839.
get_filename_component(dir_name ${ONNXRUNTIME_C_COMPILER_INC} NAME)
set(ONNXRUNTIME_C_COMPILER_INC ${ONNXRUNTIME_C_COMPILER_INC}/../${dir_name})

# The compiler include dir must come *after* OE's standard library folders,
# otherwise there will be type re-definitions. This is not possible when
# adding to oe-enclave, so let's add to the existing target instead.
target_include_directories(openenclave::oelibc_includes SYSTEM INTERFACE
    ${ONNXRUNTIME_C_COMPILER_INC}
    )
