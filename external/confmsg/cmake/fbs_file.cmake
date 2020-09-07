# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

function(fbs_file FBS_FILE OUT_FILES_VAR)
	get_filename_component(fbs_base ${FBS_FILE} NAME_WE)

	set(h_file ${CMAKE_CURRENT_BINARY_DIR}/${fbs_base}_generated.h)

	add_custom_command(
		OUTPUT ${h_file}
		DEPENDS ${FBS_FILE}
		COMMAND flatc --cpp -o ${CMAKE_CURRENT_BINARY_DIR} ${FBS_FILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		)

	set_source_files_properties(
		${h_file}
		PROPERTIES GENERATED TRUE
		)
	set_source_files_properties(
		${FBS_FILE}
		PROPERTIES HEADER_FILE_ONLY TRUE
		)

	list(APPEND ${OUT_FILES_VAR} ${h_file})
	set(${OUT_FILES_VAR} ${${OUT_FILES_VAR}} PARENT_SCOPE)
endfunction()

