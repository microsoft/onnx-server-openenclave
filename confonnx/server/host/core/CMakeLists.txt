# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

add_library(onnxruntime_server_http_core_lib STATIC
  context.h
  http_server.cc
  http_server.h
  listener.cc
  listener.h
  request_id.cc
  request_id.h
  routes.cc
  routes.h
  session.cc
  session.h
  util.cc
  util.h
)
target_include_directories(onnxruntime_server_http_core_lib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  
)
target_link_libraries(onnxruntime_server_http_core_lib
  PRIVATE
    re2::re2
  PUBLIC
    Boost::Boost
)
