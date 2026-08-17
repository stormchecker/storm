# copied from CARL


macro(storm_add_imported_library_interface name include install_include)
	add_library(${name} INTERFACE IMPORTED)
	if("${install_include}" STREQUAL "")
		target_include_directories(${name} INTERFACE ${include})
	else()
		target_include_directories(${name} INTERFACE
				$<BUILD_INTERFACE:${include}>
				$<INSTALL_INTERFACE:${install_include}>
		)
	endif()
endmacro(storm_add_imported_library_interface)

macro(add_imported_library name type lib include)
# Workaround from https://gitlab.kitware.com/cmake/cmake/-/issues/15052
	file(MAKE_DIRECTORY "${include}")
	if("${lib}" STREQUAL "")
		if("${type}" STREQUAL "SHARED")
			add_library(${name} INTERFACE IMPORTED)
			set_target_properties(${name} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${include}")
		endif()
	else()
		add_library(${name}_${type} ${type} IMPORTED)
		set_target_properties(${name}_${type} PROPERTIES IMPORTED_LOCATION "${lib}")
		set_target_properties(${name}_${type} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${include}")	
	endif()
endmacro(add_imported_library)

macro(export_option name)
	list(APPEND EXPORTED_OPTIONS "${name}")
endmacro(export_option)

macro(export_target output TARGET)
	get_target_property(TYPE ${TARGET} TYPE)
	if(TYPE STREQUAL "SHARED_LIBRARY")
		get_target_property(LOCATION ${TARGET} IMPORTED_LOCATION)
		get_target_property(INCLUDE ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
		set(${output} "${${output}}
add_library(${TARGET} SHARED IMPORTED)
set_target_properties(${TARGET} PROPERTIES IMPORTED_LOCATION \"${LOCATION}\")
set_target_properties(${TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${INCLUDE}\")
")
	elseif(TYPE STREQUAL "STATIC_LIBRARY")
		get_target_property(LOCATION ${TARGET} IMPORTED_LOCATION)
		if ("${LOCATION}" STREQUAL "LOCATION-NOTFOUND")

			get_target_property(LOCATION ${TARGET} IMPORTED_LOCATION_RELEASE)

		endif()
		get_target_property(INCLUDE ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
		set(${output} "${${output}}
add_library(${TARGET} STATIC IMPORTED)
set_target_properties(${TARGET} PROPERTIES IMPORTED_LOCATION \"${LOCATION}\")
set_target_properties(${TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${INCLUDE}\")
")
		if(NOT "${ARGN}" STREQUAL "")
			set(${output} "${${output}}set_target_properties(${TARGET} PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES \"${ARGN}\")
")
		endif()
	elseif(TYPE STREQUAL "INTERFACE_LIBRARY")
		get_target_property(INCLUDE ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
		set(${output} "${${output}}
add_library(${TARGET} INTERFACE IMPORTED)
set_target_properties(${TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${INCLUDE}\")
")
	else()
		message(STATUS "Unknown type ${TYPE}")
	endif()
endmacro(export_target)
