
####
#### Find autotools for cudd update step
set(CUDD_AUTOTOOLS_LOCATIONS "")
foreach (TOOL_VAR AUTORECONF ACLOCAL AUTOMAKE AUTOCONF AUTOHEADER)
	string(TOLOWER ${TOOL_VAR} PROG_NAME)
	find_program(${TOOL_VAR} ${PROG_NAME})
	if (NOT ${TOOL_VAR})
		message(FATAL_ERROR "Cannot find ${PROG_NAME}, cannot compile cudd3.")
	endif()
    mark_as_advanced(${TOOL_VAR})
	string(APPEND CUDD_AUTOTOOLS_LOCATIONS "${TOOL_VAR}=${${TOOL_VAR}};")
endforeach()

set(CUDD_LIB_DIR ${STORM_3RDPARTY_BINARY_DIR}/cudd-3.0.0/lib)

# create CUDD compilation flags
if (NOT STORM_DEBUG_CUDD)
	set(STORM_CUDD_FLAGS "-O3")
else()
	message(WARNING "Storm - Building CUDD in DEBUG mode.")
	set(STORM_CUDD_FLAGS "-O0 -g")
endif()
set(STORM_CUDD_FLAGS "CFLAGS=${STORM_CUDD_FLAGS} -w -DPIC -DHAVE_IEEE_754 -fno-common -ffast-math -fno-finite-math-only")
if (NOT STORM_PORTABLE AND (NOT APPLE_SILICON OR (STORM_COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)))
	set(STORM_CUDD_FLAGS "${STORM_CUDD_FLAGS} -march=native")
endif()

# Set sysroot to circumvent problems in macOS "Mojave" (or higher) where the header files are no longer in /usr/include
set(CUDD_INCLUDE_FLAGS "")
if (CMAKE_OSX_SYSROOT)
    set(CUDD_INCLUDE_FLAGS "CPPFLAGS=--sysroot=${CMAKE_OSX_SYSROOT}")
endif()

set(CUDD_CXX_COMPILER "${CMAKE_CXX_COMPILER}")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0.12000032)
		if (CMAKE_HOST_SYSTEM_VERSION VERSION_GREATER_EQUAL 20.1.0)
			message(WARNING "There are some known issues compiling CUDD on some setups. We implemented a workaround that mostly works, but if you still have problems compiling CUDD, especially if you do not use the default compiler of your system, please contact the Storm developers.")
			# The issue is known to occur using the Command Line Tools for XCode 12.2. Apparently, it is fixed in the beta for XCode 12.3. 
			set(CUDD_CXX_COMPILER "c++")
		endif()
	endif()
endif()

# to also create shared library, do  --enable-shared
ExternalProject_Add(
        cudd3_src
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${STORM_3RDPARTY_SOURCE_DIR}/cudd-3.0.0
        PREFIX ${STORM_3RDPARTY_BINARY_DIR}/cudd-3.0.0
        PATCH_COMMAND ${CMAKE_COMMAND} -E env ${CUDD_AUTOTOOLS_LOCATIONS} ${AUTORECONF}
        CONFIGURE_COMMAND ${STORM_3RDPARTY_SOURCE_DIR}/cudd-3.0.0/configure --enable-obj --with-pic=yes --prefix=${STORM_3RDPARTY_BINARY_DIR}/cudd-3.0.0 --libdir=${CUDD_LIB_DIR} CC=${CMAKE_C_COMPILER} CXX=${CUDD_CXX_COMPILER} ${CUDD_INCLUDE_FLAGS}
	# Multi-threaded compilation could lead to compile issues
        BUILD_COMMAND make -j1 ${STORM_CUDD_FLAGS} ${CUDD_AUTOTOOLS_LOCATIONS}
        INSTALL_COMMAND make install -j1 ${CUDD_AUTOTOOLS_LOCATIONS}
        BUILD_IN_SOURCE 0
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        BUILD_BYPRODUCTS ${CUDD_LIB_DIR}/libcudd${STATIC_EXT}
        LOG_OUTPUT_ON_FAILURE ON
)

# Do not use system CUDD, Storm has a modified version
set(CUDD_INCLUDE_DIR ${STORM_3RDPARTY_BINARY_DIR}/cudd-3.0.0/include)
set(CUDD_STATIC_LIBRARY ${CUDD_LIB_DIR}/libcudd${STATIC_EXT})
set(CUDD_VERSION_STRING 3.0.0)
set(CUDD_INSTALL_DIR ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/cudd/)


file(MAKE_DIRECTORY ${CUDD_INCLUDE_DIR}) # Workaround https://gitlab.kitware.com/cmake/cmake/-/issues/15052
add_library(cudd3 STATIC IMPORTED)
set_target_properties(
		cudd3
		PROPERTIES
		IMPORTED_LOCATION ${CUDD_STATIC_LIBRARY}
		INTERFACE_INCLUDE_DIRECTORIES ${CUDD_INCLUDE_DIR}
)
install(FILES ${CUDD_STATIC_LIBRARY} DESTINATION ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
install(DIRECTORY ${CUDD_INCLUDE_DIR}/ DESTINATION ${CUDD_INSTALL_DIR}
		FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN ".git" EXCLUDE)

add_dependencies(storm_resources cudd3)
add_dependencies(cudd3 cudd3_src)
list(APPEND STORM_DEP_IMP_TARGETS cudd3)

message(STATUS "Storm - Linking with CUDD ${CUDD_VERSION_STRING}.")
