# Disable clang-tidy due to issue with included header that can't be ignored.
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    set(CMAKE_CXX_CLANG_TIDY)
endif()

pybind11_add_module(${CMAKE_PROJECT_NAME}_py SYSTEM python.cc)
target_compile_definitions(${CMAKE_PROJECT_NAME}_py PRIVATE
    PY_MODULE_NAME=${CMAKE_PROJECT_NAME}_py)
target_include_directories(${CMAKE_PROJECT_NAME}_py PRIVATE
    ${ROOT_INTERNAL_INCLUDE_DIR}
    )
# TODO build a confonnx client lib and link to that
target_link_libraries(${CMAKE_PROJECT_NAME}_py PRIVATE
    confmsg::confmsg_client
    )

# libpython is (on purpose) *not* linked to the extension module.
# To allow that, unresolved symbols are allowed during linking.
# This is bad as it hides unresolved symbols not related to Python
# and errors will only show up at runtime. Here we add the missing check.
#add_custom_command(TARGET ${CMAKE_PROJECT_NAME}_py POST_BUILD
#    COMMAND TODO
#    USES_TERMINAL
#    )

# https://github.com/pybind/pybind11/issues/1604
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${CMAKE_PROJECT_NAME}_py PRIVATE -fsized-deallocation)
endif()

# Store into confonnx subfolder during build so that unit tests
# can add the parent folder to PYTHONPATH and have it discovered by Python
# during import.
set(python_pkg_dir "${CMAKE_BINARY_DIR}/python")
set_target_properties(${CMAKE_PROJECT_NAME}_py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${python_pkg_dir}/confonnx)

# For tests, same as above.
file(COPY confonnx DESTINATION ${python_pkg_dir})

install(FILES setup.py
        DESTINATION lib/python)

install(DIRECTORY confonnx
        DESTINATION lib/python)

install(TARGETS ${CMAKE_PROJECT_NAME}_py
        LIBRARY DESTINATION lib/python/confonnx)
