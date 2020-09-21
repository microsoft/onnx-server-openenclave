# Include submodule dependencies

set(BUILD_TESTING OFF)

cmake_policy(SET CMP0079 NEW)

if (NOT BUILD_ENCLAVE)
  if (BUILD_CLIENT)
    add_subdirectory(pybind11 EXCLUDE_FROM_ALL)
  endif()
  add_subdirectory(googletest EXCLUDE_FROM_ALL)
endif()

# Boost
if (NOT BUILD_ENCLAVE)
  include(get_boost)
endif()

# re2
if (NOT BUILD_ENCLAVE)
  add_subdirectory(re2 EXCLUDE_FROM_ALL)
  target_include_directories(re2 INTERFACE re2)
endif()

# libcurl
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_COOKIES ON CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(ENABLE_UNIX_SOCKETS OFF CACHE BOOL "" FORCE)
# CURL_CA_BUNDLE_SET & CURL_CA_PATH_SET are not reset correctly in incremental builds.
# (bug in libcurl's CMake scripts)
set(CURL_CA_BUNDLE_SET FALSE CACHE BOOL "" FORCE)
set(CURL_CA_PATH_SET FALSE CACHE BOOL "" FORCE)
if (CMAKE_BUILD_TYPE STREQUAL Debug)
  set(ENABLE_DEBUG ON CACHE BOOL "" FORCE)
endif()
if (BUILD_ENCLAVE)
  set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
  set(CMAKE_USE_MBEDTLS ON CACHE BOOL "" FORCE)
  # Satisfy curl's FindMbedTLS module, mbedtls supplied by oeenclave.
  set(MBEDTLS_LIBRARY "" CACHE STRING "" FORCE)
  set(MBEDX509_LIBRARY "" CACHE STRING "" FORCE)
  set(MBEDCRYPTO_LIBRARY "" CACHE STRING "" FORCE)
  set(MBEDTLS_INCLUDE_DIRS "" CACHE STRING "" FORCE)
  # Avoid that curl links to dl.
  set(HAVE_LIBDL OFF CACHE BOOL "" FORCE)
  set(HAVE_CLOCK_GETTIME_MONOTONIC OFF CACHE BOOL "" FORCE)
  set(HAVE_ALARM OFF CACHE BOOL "" FORCE)
  set(ENABLE_THREADED_RESOLVER OFF CACHE BOOL "" FORCE)
  add_compile_definitions(CURL_DISABLE_NETRC USE_BLOCKING_SOCKETS)
endif()
set(HAVE_GETNAMEINFO OFF CACHE BOOL "" FORCE)
add_subdirectory(curl EXCLUDE_FROM_ALL)

# nlohmann-json
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
add_subdirectory(nlohmann-json EXCLUDE_FROM_ALL)

# cpp-base64
add_library(cpp-base64 STATIC
  cpp-base64/base64.h
  cpp-base64/base64.cpp
  )
target_include_directories(cpp-base64 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp-base64
  )

# spdlog
set(SPDLOG_BUILD_EXAMPLES OFF)
add_subdirectory(spdlog EXCLUDE_FROM_ALL)

# ONNX Runtime
set(onnxruntime_USE_OPENENCLAVE ${BUILD_ENCLAVE} CACHE BOOL "" FORCE)
set(onnxruntime_OPENENCLAVE_PHASE ${BUILD_MODE} CACHE STRING "" FORCE)
set(onnxruntime_USE_AUTOML OFF CACHE BOOL "" FORCE)
set(onnxruntime_USE_FULL_PROTOBUF ON CACHE BOOL "" FORCE) # currently required by Server code
set(onnxruntime_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)

# temporary hack until https://github.com/openenclave/openenclave/issues/2082 is resolved
set(gRPC_SSL_PROVIDER "package" CACHE STRING "" FORCE)

set(ONNXRUNTIME_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/onnxruntime)
add_subdirectory(onnxruntime/cmake ${ONNXRUNTIME_BINARY_DIR} EXCLUDE_FROM_ALL)
target_include_directories(onnx_proto PUBLIC ${ONNXRUNTIME_BINARY_DIR}/onnx)

# Monkey patches for libraries used in enclave
if (BUILD_ENCLAVE)
  set(oe_stubs
    patches/stubs_undefined.c
    )

  target_link_libraries(cpp-base64 PRIVATE
    openenclave::oeenclave
    )

  target_sources(spdlog PRIVATE ${oe_stubs})
  target_compile_definitions(spdlog PRIVATE
    -DSPDLOG_NO_THREAD_ID
    -DSPDLOG_NO_DATETIME
    )

  target_link_libraries(libcurl
    openenclave::oeenclave
    openenclave::oelibcxx
    openenclave::oehostsock
    openenclave::oehostresolver
    )
  target_sources(libcurl PRIVATE ${oe_stubs})

  # Should probably be done upstream, but ONNX RT doesn't follow proper
  # CMake practices currently, so this would have to be a bigger push
  # for general improvement.
  set(ONNXRUNTIME_REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/onnxruntime)
  set(ONNXRUNTIME_INCLUDE_DIR ${ONNXRUNTIME_REPO_ROOT}/include/onnxruntime)
  target_include_directories(onnxruntime_openenclave INTERFACE
    ${ONNXRUNTIME_INCLUDE_DIR}
    # For onnxruntime_config.h.
    ${ONNXRUNTIME_BINARY_DIR}
    )
  target_link_libraries(onnxruntime_openenclave INTERFACE
    onnx
    )

  target_compile_definitions(libprotobuf INTERFACE
    GOOGLE_PROTOBUF_NO_RDTSC
    )

  # monkey-patch targets that we are re-using
  set(oe_libs
    spdlog
    )
  foreach (oe_lib ${oe_libs})
    target_link_libraries(${oe_lib} PRIVATE
      openenclave::oeenclave
      openenclave::oelibcxx)
    target_sources(${oe_lib} PRIVATE ${oe_stubs})
  endforeach()
endif()
