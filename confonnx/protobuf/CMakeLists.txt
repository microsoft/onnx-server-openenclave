# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.s

# Generate .h and .cc files from protobuf file

add_library(server_proto predict.proto)
target_include_directories(server_proto PUBLIC 
    $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
    "${CMAKE_CURRENT_BINARY_DIR}"
    )
target_compile_definitions(server_proto PUBLIC
    $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_COMPILE_DEFINITIONS>
    )
target_link_libraries(server_proto PUBLIC onnx_proto)
onnxruntime_protobuf_generate(APPEND_PATH
    # for onnx-ml.proto
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/../external/onnxruntime/onnxruntime/core/protobuf
    TARGET server_proto
    )

if (BUILD_ENCLAVE)
    target_link_libraries(server_proto PRIVATE
        openenclave::oeenclave
        openenclave::oelibcxx
    )
endif()