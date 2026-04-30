cmake_minimum_required (VERSION 3.23)
if (NOT ${CMAKE_VERSION} VERSION_LESS 3.24)
	cmake_policy(VERSION 3.24)
endif()

# Set project name
project(storm
	VERSION 1.12.0.1
	# Version x.y.z.1 should be read as: This is an updated version of x.y.z that has not been released as x.y.z+1 yet.
	LANGUAGES CXX C
	DESCRIPTION "Storm - A Modern Probabilistic Model Checker"
	HOMEPAGE_URL "https://www.stormchecker.org"
)

# Add the resources/cmake folder to Module Search Path
set(STORM_CMAKE_FIND_MODULES "${PROJECT_SOURCE_DIR}/resources/cmake/find_modules")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${STORM_CMAKE_FIND_MODULES}" "${PROJECT_SOURCE_DIR}/resources/cmake/macros")
add_custom_target(storm_resources)
if(PROJECT_IS_TOP_LEVEL)
	add_custom_target(resources)
	add_dependencies(resources storm_resources)
endif()


include(ExternalProject)
include(imported)
include(CheckCXXSourceCompiles)
include(CheckCSourceCompiles)
include(ProcessorCount)

set(CMAKE_CXX_STANDARD 20)

#############################################################
##
##	CMake options of Storm
##
#############################################################
## Compilation options.
option(STORM_DEVELOPER "Sets whether the development mode is used." OFF)
option(STORM_ALLWARNINGS "Compile with even more warnings" OFF)
option(STORM_WARNING_AS_ERROR "Sets whether compile warnings are treated as errors." OFF)
MARK_AS_ADVANCED(STORM_WARNING_AS_ERROR)
option(STORM_USE_LTO "Sets whether link-time optimizations are enabled." ON)
MARK_AS_ADVANCED(STORM_USE_LTO)
option(STORM_USE_THIN_LTO "Sets whether thin link-time optimizations are enabled (faster compile times than normal LTO)." OFF)
MARK_AS_ADVANCED(STORM_USE_THIN_LTO)
option(STORM_PORTABLE "Sets whether a build needs to be portable." OFF)
MARK_AS_ADVANCED(STORM_PORTABLE)
# Force POPCNT is helpful for portable code targetting platforms with SSE4.2 operation support.
option(STORM_FORCE_POPCNT "Sets whether the popcnt instruction is forced to be used (advanced)." OFF)
MARK_AS_ADVANCED(STORM_FORCE_POPCNT)
option(STORM_COMPILE_WITH_CCACHE "Compile using CCache [if found]" ON)
mark_as_advanced(STORM_COMPILE_WITH_CCACHE)
option(STORM_COMPILE_WITH_PCH "Enable pre-compiled headers" ON)
mark_as_advanced(STORM_COMPILE_WITH_PCH)
option(STORM_LOG_DISABLE_DEBUG "Disable log and trace message support" OFF)
option(STORM_COMPILE_WITH_ADDRESS_SANITIZER "Sets whether to compile with AddressSanitizer enabled" OFF)
option(STORM_COMPILE_WITH_ALL_SANITIZERS "Sets whether to compile with all sanitizers enabled" OFF)
option(STORM_COMPILE_WITH_COMPILATION_PROFILING "Compile with output to profile compilation process" OFF)
MARK_AS_ADVANCED(STORM_COMPILE_WITH_COMPILATION_PROFILING)
option(STORM_USE_CLN_EA "Sets whether CLN instead of GMP numbers should be used for exact arithmetic." OFF)
export_option(STORM_USE_CLN_EA)
option(STORM_USE_CLN_RF "Sets whether CLN instead of GMP numbers should be used for rational functions." ON)
export_option(STORM_USE_CLN_RF)

## Configuration setup.
option(STORM_BUILD_TESTS "If switched off, tests cannot be built." ON )
export_option(STORM_BUILD_TESTS)
MARK_AS_ADVANCED(STORM_BUILD_TESTS)
option(STORM_BUILD_EXECUTABLES "If switched off, binaries cannot be built." ON)
export_option(STORM_BUILD_EXECUTABLES)
MARK_AS_ADVANCED(STORM_BUILD_EXECUTABLES)
option(STORM_LOAD_QVBS "Sets whether the Quantitative Verification Benchmark Set (QVBS) should be downloaded." OFF)
set(STORM_QVBS_ROOT "" CACHE STRING "The root directory of the Quantitative Verification Benchmark Set (QVBS) in case it should not be downloaded (optional).")
MARK_AS_ADVANCED(STORM_QVBS_ROOT)

## Configuring dependencies
set(STORM_CARL_GIT_REPO "https://github.com/stormchecker/carl-storm.git" CACHE STRING "For fetching carl: which git repo to use")
option(STORM_CARL_GIT_TAG "For fetching carl: which git tag to use" "14.34")
MARK_AS_ADVANCED(STORM_CARL_GIT_TAG)
option(STORM_DISABLE_CUDD "Sets whether linking with CUDD should be disabled." OFF)
option(STORM_DISABLE_GLPK "Sets whether linking with GLPK should be disabled." OFF)
set(GLPK_ROOT "" CACHE STRING "A hint to the root directory of GLPK.")
MARK_AS_ADVANCED(GLPK_ROOT)
option(STORM_DISABLE_GMM "Sets whether linking with GMM should be disabled." OFF)
option(STORM_DISABLE_GUROBI "Sets whether linking with Gurobi should be disabled." OFF)
set(GUROBI_ROOT "" CACHE STRING "A hint to the root directory of Gurobi.")
MARK_AS_ADVANCED(GUROBI_ROOT)
option(STORM_DISABLE_LIBARCHIVE "Sets whether linking with LibArchive should be disabled." OFF)
set(LibArchive_ROOT "" CACHE STRING "A hint to the root directory of LibArchive.")
MARK_AS_ADVANCED(LibArchive_ROOT)
option(STORM_DISABLE_MATHSAT "Sets whether linking with MathSAT should be disabled." OFF)
set(MATHSAT_ROOT "" CACHE STRING "A hint to the root directory of MathSAT.")
MARK_AS_ADVANCED(MATHSAT_ROOT)
option(STORM_DISABLE_SOPLEX "Sets whether linking with Soplex should be disabled." OFF)
set(SOPLEX_ROOT "" CACHE STRING "A hint to the root directory of Soplex.")
MARK_AS_ADVANCED(SOPLEX_ROOT)
option(STORM_DISABLE_SPOT "Sets whether linking with Spot should be disabled." OFF)
option(STORM_SPOT_FORCE_SHIPPED "Sets whether system versions of Spot should be ignored (if available)." OFF)
set(SPOT_ROOT "" CACHE STRING "A hint to the root directory of Spot.")
MARK_AS_ADVANCED(SPOT_ROOT)
option(STORM_DISABLE_SYLVAN "Sets whether linking with Sylvan should be disabled." OFF)
option(STORM_DISABLE_XERCES "Sets whether linking with Xerces should be disabled." OFF)
set(XERCESC_ROOT "" CACHE STRING "A hint to the root directory of Xerces (optional).")
mark_as_advanced(XERCESC_ROOT)
option(STORM_DISABLE_Z3  "Sets whether linking with Z3 should be disabled." OFF)
set(Z3_ROOT "" CACHE STRING "A hint to the root directory of Z3.")
mark_as_advanced(Z3_ROOT)

option(STORM_DEBUG_CUDD "Build CUDD in debug mode." OFF)
MARK_AS_ADVANCED(STORM_DEBUG_CUDD)
option(STORM_DEBUG_SPOT "Build Spot in debug mode." OFF)
MARK_AS_ADVANCED(STORM_DEBUG_SPOT)
option(STORM_DEBUG_SYLVAN "Build Sylvan in debug mode." OFF)
MARK_AS_ADVANCED(STORM_DEBUG_SYLVAN)

if (STORM_COMPILE_WITH_ALL_SANITIZERS)
    set(STORM_COMPILE_WITH_ADDRESS_SANITIZER ON)
endif()

# Get an approximation of the number of available processors (used for parallel build of shipped resources)
ProcessorCount(STORM_RESOURCES_BUILD_JOBCOUNT_DEFAULT)
# To be safe, we only take a little more than half of the resources.
# This also correctly deals with the case where ProcessorCount is unable to find the correct number (and thus returns 0)
MATH(EXPR STORM_RESOURCES_BUILD_JOBCOUNT_DEFAULT "${STORM_RESOURCES_BUILD_JOBCOUNT_DEFAULT}/2 + 1")
set(STORM_RESOURCES_BUILD_JOBCOUNT "${STORM_RESOURCES_BUILD_JOBCOUNT_DEFAULT}" CACHE STRING "The number of jobs used when building external resources")
mark_as_advanced(STORM_RESOURCES_BUILD_JOBCOUNT)
if(NOT STORM_RESOURCES_BUILD_JOBCOUNT GREATER 0)
	message(FATAL_ERROR "STORM_RESOURCES_BUILD_JOBCOUNT must be a positive number. Got '${STORM_RESOURCES_BUILD_JOBCOUNT}' instead." )
endif()

# Set some CMAKE Variables as advanced
mark_as_advanced(CMAKE_OSX_ARCHITECTURES)
mark_as_advanced(CMAKE_OSX_SYSROOT)
mark_as_advanced(CMAKE_OSX_DEPLOYMENT_TARGET)

# Offer the user the choice of overriding the installation directories
include(GNUInstallDirs)
set(STORM_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/storm CACHE PATH "Installation directory for header files" )
set(STORM_LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(STORM_BIN_INSTALL_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(STORM_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/storm CACHE PATH  "Installation directory for CMake files")
set(STORM_RESOURCE_INCLUDE_INSTALL_DIR ${STORM_INCLUDE_INSTALL_DIR}/resources CACHE PATH "Installation directory for dependency header files")
set(STORM_RESOURCE_LIBRARY_INSTALL_DIR ${STORM_LIB_INSTALL_DIR} CACHE PATH "Installation directory for dependency library files")
if(PROJECT_IS_TOP_LEVEL)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

# If the STORM_DEVELOPER option was turned on, by default we target a debug version, otherwise a release version.
if (STORM_DEVELOPER)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "DEBUG")
    endif()
    # TODO Do this more targeted
    add_definitions(-DSTORM_DEV)
    set(STORM_DEBUG_CUDD ON)
    set(STORM_DEBUG_SPOT ON)
    set(STORM_DEBUG_SYLVAN ON)
    set(STORM_WARNING_AS_ERROR ON)
	# Turn off PCH for faster ccache usage on warm caches.
	set(STORM_COMPILE_WITH_PCH OFF)
else()
    set(STORM_LOG_DISABLE_DEBUG ON)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "RELEASE")
    endif()
endif()
set(CMAKE_COMPILE_WARNING_AS_ERROR ${STORM_WARNING_AS_ERROR})

message(STATUS "Storm - Building ${CMAKE_BUILD_TYPE} version.")

# PCH compiled files cannot be properly cached currently. Thus disabling it is recommended when doing many warm ccache builds.
if(NOT STORM_COMPILE_WITH_PCH)
	set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
	message(STATUS "Storm - Disabling precompiled headers because of ccache compatibility.")
endif()

if(STORM_COMPILE_WITH_CCACHE)
	find_program(CCACHE_FOUND ccache)
	mark_as_advanced(CCACHE_FOUND)
	if(CCACHE_FOUND)
		message(STATUS "Storm - Using ccache")
		set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros,include_file_mtime,include_file_ctime")
		set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)

		if(STORM_COMPILE_WITH_PCH)
			message(STATUS "STORM - PCH is enabled which will lead to significantly lower ccache hit rates. Consider disabling PCH if you plan to rebuild storm multiple times.")
		endif()
	else()
		message(STATUS "Storm - Could not find ccache.")
	endif()
else()
    message(STATUS "Storm - Disabled use of ccache.")
endif()

# Directory for test resources.
set(STORM_TEST_RESOURCES_DIR "${PROJECT_SOURCE_DIR}/resources/examples/testfiles")

# Auto-detect operating system.
set(MACOSX 0)
set(LINUX 0)
set(APPLE_SILICON 0)
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# Mac OS
	set(OPERATING_SYSTEM "Mac OS")
        set(MACOSX 1)
	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES arm64)
		set(APPLE_SILICON 1)
	endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	# Linux
	set(OPERATING_SYSTEM "Linux")
        set(LINUX 1)
elseif(WIN32)
	# Assuming Windows.
	set(OPERATING_SYSTEM "Windows")
else()
	message(WARNING "We are unsure about your operating system.")
	set(OPERATING_SYSTEM "Linux")
	set(LINUX 1)
ENDIF()
message(STATUS "Storm - Detected operating system ${OPERATING_SYSTEM}.")

set(STATIC_EXT ".a")
set(LIB_PREFIX "lib")
if(MACOSX)
	set(DYNAMIC_EXT ".dylib")
else()
	set(DYNAMIC_EXT ".so")
endif()
message(VERBOSE "Storm - Assuming extension for shared libraries: ${DYNAMIC_EXT}")
message(VERBOSE "Storm - Assuming extension for static libraries: ${STATIC_EXT}")

#############################################################
##
##	Compiler detection and initial configuration
##
#############################################################
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	# using Clang
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
		message(FATAL_ERROR "Clang version must be at least 15.0, currently you are using version ${CMAKE_CXX_COMPILER_VERSION}.")
	endif()

	set(STORM_COMPILER_CLANG ON)
	set(CLANG ON)
	set(STORM_COMPILER_ID "clang")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
	# using AppleClang
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0.3)
		message(FATAL_ERROR "AppleClang version must be at least 14.0.3 (Xcode version 14.3).")
	elseif (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0.0)
		message(WARNING "Disabling stack checks for AppleClang version < 15.0")
		# Stack checks are known to produce errors with the following Clang versions:
		# 11.0.0: Runtime errors (stack_not_16_byte_aligned_error) when invoking storm in release mode
		# 11.0.3 and  12.0.0: Catching exceptions thrown within PRISM parser does not work (The exception just falls through)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-stack-check")
	endif()

	set(STORM_COMPILER_APPLECLANG ON)
	set(CLANG ON)
	set(STORM_COMPILER_ID "AppleClang")
	set(CMAKE_MACOSX_RPATH ON)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(GCC ON)
	# using GCC
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
		message(FATAL_ERROR "gcc version must be at least 10.0.")
	endif()

	set(STORM_COMPILER_GCC ON)
	set(STORM_COMPILER_ID "gcc")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	message(FATAL_ERROR "Intel compiler is currently not supported.")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	message(FATAL_ERROR "Visual Studio compiler is currently not supported.")
else()
    message(FATAL_ERROR "Unknown compiler '${CMAKE_CXX_COMPILER_ID}' is not supported")
endif()
set(STORM_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})

if(CCACHE_FOUND)
	set(STORM_COMPILER_ID "${STORM_COMPILER_ID} (ccache)")
endif()


#############################################################
##
##	Some compiler flags
##
#############################################################
if (STORM_COMPILER_CLANG OR STORM_COMPILER_APPLECLANG)
	if (LINUX)
		set(CLANG_STDLIB libstdc++)
    else()
		set(CLANG_STDLIB libc++)
    endif()

    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=${CLANG_STDLIB} -ftemplate-depth=1024")

    if(LINUX)
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
    elseif(MACOSX)
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-export_dynamic")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-export_dynamic")
    endif()
elseif (STORM_COMPILER_GCC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprefetch-loop-arrays")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

	# Do not warn about problems internally to GCC 12
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow")
endif ()
#### We set more compiler flags in src/ which will not affect the subprojects/dependencies.



#############################################################
##
##	Set precompiled headers
##
#############################################################

# Selection is based on (relatively extensive, but ad-hoc) compilation profiling in late 2023.
# As changes mean recompilations of the complete code base, we only use std libraries and boost libraries here for now.
SET(STORM_PRECOMPILED_HEADERS "<any>" "<map>" "<vector>" "<unordered_set>" "<forward_list>"
		"<optional>" "<ostream>" "<istream>" "<list>" "<set>" "<fstream>" "<string>" "<boost/optional.hpp>" "<boost/variant.hpp>"
		"<boost/container/flat_set.hpp>" "<boost/dynamic_bitset.hpp>" "<boost/range/irange.hpp>")

function(storm_target_precompile_headers target)
	if(NOT CMAKE_DISABLE_PRECOMPILE_HEADERS)
		target_precompile_headers(${target} ${ARGN})
	endif()
endfunction()

#############################################################
##
##	Compiler tests during config
##
#############################################################

# Test compiler by compiling small program.
CHECK_C_SOURCE_COMPILES("
	#include <stdio.h>
	int main() {
		const char* text = \"A Storm is coming.\";
		return 0;
	}"
	COMPILER_C_WORKS
)
CHECK_CXX_SOURCE_COMPILES("
	#include <string>
	int main() {
		const std::string text = \"A Storm is coming.\";
		return 0;
	}"
	COMPILER_CXX_WORKS
)
if ((NOT COMPILER_CXX_WORKS) OR (NOT COMPILER_C_WORKS))
	if (MACOSX)
		message(FATAL_ERROR "The C/C++ compiler is not configured correctly.\nTry running 'xcode-select --install'.")
	else()
		message(FATAL_ERROR "The C/C++ compiler is not configured correctly.")
	endif()
endif()


####
# These need to be set before the RPATHs and we want to set RPATHs before building targets.
set(STORM_3RDPARTY_SOURCE_DIR ${PROJECT_SOURCE_DIR}/resources/3rdparty)
set(STORM_3RDPARTY_BINARY_DIR ${PROJECT_BINARY_DIR}/resources/3rdparty)
set(STORM_3RDPARTY_INCLUDE_DIR ${PROJECT_BINARY_DIR}/include/resources/3rdparty)
export_option(STORM_3RDPARTY_BINARY_DIR) # To support BUILD_RPATH include hacks that we want to get rid off some day.

#############################################################
##
##	RPATH settings
##
#############################################################

# don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already (but only when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

if (MACOSX)
	SET(RPATH_ORIGIN_EQUIV "@loader_path")
else()
	SET(RPATH_ORIGIN_EQUIV "$ORIGIN")
endif()
# the RPATH to be used when installing
# TODO move these to the right targets rather than setting them globally.
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/storm;${CMAKE_INSTALL_PREFIX}/lib/storm/resources;${RPATH_ORIGIN_EQUIV};${RPATH_ORIGIN_EQUIV}/resources")
# TODO this is less than ideal, but we had multiple issues with spot 2.13.1 in 25/06.
SET(CMAKE_BUILD_RPATH "${STORM_3RDPARTY_BINARY_DIR}/spot/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#############################################################
##
##	Generator specific settings
##
#############################################################
# Display information about build configuration.
message(STATUS "Storm - Using compiler configuration ${STORM_COMPILER_ID} ${STORM_COMPILER_VERSION}.")
message(STATUS "Storm - Build type: ${CMAKE_BUILD_TYPE}")

#############################################################
#############################################################
##
##	Inclusion of required libraries
##
#############################################################
#############################################################

#############################################################
##
##	Include the targets for non-system resources
##
#############################################################

# In 3rdparty, targets are being defined that can be used
# in the the system does not have a library
include(resources/3rdparty/CMakeLists.txt)

if(STORM_HAVE_GLPK OR STORM_HAVE_GUROBI OR STORM_HAVE_SOPLEX OR STORM_HAVE_Z3)
	set(STORM_HAVE_LP_SOLVER ON)
	message(STATUS "Storm - Compiled with an LP solver.")
else()
	set(STORM_HAVE_LP_SOLVER OFF)
	message(WARNING "Storm - Compiled without an LP solver. This may yield errors at run time.")
endif()
if(STORM_HAVE_CUDD OR STORM_HAVE_SYLVAN)
	set(STORM_HAVE_DD_LIBRARY ON)
	message(STATUS "Storm - Compiled with a DD library.")
else()
	set(STORM_HAVE_DD_LIBRARY OFF)
	message(WARNING "Storm - Compiled without a DD library. Symbolic model representations are not supported.")
endif()

export_option(STORM_HAVE_CUDD)
export_option(STORM_HAVE_GMM)
export_option(STORM_HAVE_GLPK)
export_option(STORM_HAVE_GUROBI)
export_option(STORM_HAVE_MATHSAT)
export_option(STORM_HAVE_SOPLEX)
export_option(STORM_HAVE_SPOT)
export_option(STORM_HAVE_SYLVAN)
export_option(STORM_HAVE_XERCES)
export_option(STORM_HAVE_Z3)
export_option(STORM_HAVE_LP_SOLVER)

if(PROJECT_IS_TOP_LEVEL)
	# Include Doxygen
	include(resources/doxygen/CMakeLists.txt)
endif()

#############################################################
##
##	CMake-generated Config File for Storm
##
#############################################################
if (PROJECT_IS_TOP_LEVEL)
	# try to obtain the current version from git.
	include(GetGitRevisionDescription)
	get_git_head_revision(STORM_VERSION_REFSPEC STORM_VERSION_GIT_HASH)
	git_describe(STORM_GIT_VERSION_STRING)

	# Trying to figure out if the git has changed.
	string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)$" STORM_VERSION_MATCH "${STORM_GIT_VERSION_STRING}")
	# No longer used, but help understanding the regex.
	#set(STORM_VERSION_MAJOR "${CMAKE_MATCH_1}")
	#set(STORM_VERSION_MINOR "${CMAKE_MATCH_2}")
	#set(STORM_VERSION_PATCH "${CMAKE_MATCH_3}")
	set(STORM_GIT_VERSION_REST "${CMAKE_MATCH_4}")
	# parse rest of the form (-label)-commitsahead-hash(-appendix)
	string(REGEX MATCH "^(\\-([a-z][a-z0-9\\.]+))?\\-([0-9]+)\\-([a-z0-9]+)(\\-.*)?$" STORM_VERSION_REST_MATCH "${STORM_GIT_VERSION_REST}")
	# No longer used, but structure helps understanding the regex.
	#set(STORM_VERSION_LABEL "${CMAKE_MATCH_2}") # might be empty
	#set(STORM_VERSION_COMMITS_AHEAD "${CMAKE_MATCH_3}")
	#set(STORM_VERSION_TAG_HASH "${CMAKE_MATCH_4}") # is not used
	set(STORM_VERSION_APPENDIX "${CMAKE_MATCH_5}") # might be empty
endif()


# check whether the git version lookup failed
if (STORM_GIT_VERSION_STRING MATCHES "NOTFOUND")
	set(STORM_VERSION_DIRTY DirtyState::Unknown)
else()
	if ("${STORM_VERSION_APPENDIX}" MATCHES "^.*dirty.*$")
		set(STORM_VERSION_DIRTY "DirtyState::Dirty")
	else()
		set(STORM_VERSION_DIRTY "DirtyState::Clean")
	endif()
endif()

# set final Storm version

set(STORM_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(STORM_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(STORM_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(STORM_VERSION "${STORM_VERSION_MAJOR}.${STORM_VERSION_MINOR}.${STORM_VERSION_PATCH}")
if(PROJECT_VERSION_TWEAK GREATER 0)
	set(STORM_VERSION_DEV 1)
else()
	set(STORM_VERSION_DEV 0)
endif()

set(STORM_VERSION_STRING "${STORM_VERSION}")
if(STORM_VERSION_DEV GREATER 0)
	set(STORM_VERSION_STRING "${STORM_VERSION_STRING} (dev)")
endif()

message(STATUS "Storm - Version is ${STORM_VERSION_STRING}.")
export_option(STORM_VERSION) # We want this information also in the more accessible stormOptions.cmake
export_option(STORM_VERSION_DEV) # We want this information also in the more accessible stormOptions.cmake

# Configure a header file to pass some of the CMake settings to the source code
configure_file (
	"${PROJECT_SOURCE_DIR}/src/storm-config.h.in"
	"${PROJECT_BINARY_DIR}/include/storm-config.h"
)

if(PROJECT_IS_TOP_LEVEL AND STORM_BUILD_TESTS)
	include(CTest)
	# Compiles all tests
	add_custom_target(tests)
	# Compiles and runs all tests
	add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
	set(CMAKE_CTEST_COMMAND_VERBOSE ${CMAKE_CTEST_COMMAND} -V)
	add_custom_target(check-verbose COMMAND ${CMAKE_CTEST_COMMAND_VERBOSE})
	add_dependencies(check tests)
	add_dependencies(check-verbose tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
	# Apply code formatting
	add_custom_target(format COMMAND ${PROJECT_SOURCE_DIR}/resources/scripts/auto-format.sh)
endif()

set(STORM_TARGETS "")
add_subdirectory(src)

# Export options
include(export)

install(DIRECTORY ${STORM_CMAKE_FIND_MODULES} DESTINATION ${STORM_CMAKE_INSTALL_DIR})
install(FILES ${PROJECT_BINARY_DIR}/stormConfig.cmake.install DESTINATION ${STORM_CMAKE_INSTALL_DIR} RENAME stormConfig.cmake)
install(FILES ${PROJECT_BINARY_DIR}/stormConfigVersion.cmake DESTINATION ${STORM_CMAKE_INSTALL_DIR})
install(EXPORT storm_Targets FILE stormTargets.cmake DESTINATION ${STORM_CMAKE_INSTALL_DIR})

include(${PROJECT_SOURCE_DIR}/resources/cmake/stormCPackConfig.cmake)
