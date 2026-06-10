add_library(sylvan STATIC)
add_library(sylvan::sylvan ALIAS sylvan)

set(SYLVAN_HDRS
        sylvan.h
        sylvan_bdd.h
        sylvan_cache.h
        sylvan_config.h
        sylvan_common.h
        sylvan_hash.h
        sylvan_int.h
        sylvan_ldd.h
        sylvan_ldd_int.h
        sylvan_mt.h
        sylvan_mtbdd.h
        sylvan_mtbdd_int.h
        sylvan_obj.hpp
        sylvan_stats.h
        sylvan_table.h
        sylvan_tls.h
        sylvan_zdd.h
        sylvan_zdd_int.h
)

target_sources(sylvan
        PRIVATE
        sha2.c
        sylvan_bdd.c
        sylvan_cache.c
        sylvan_common.c
        sylvan_hash.c
        sylvan_ldd.c
        sylvan_mt.c
        sylvan_mtbdd.c
        sylvan_obj.cpp
        sylvan_refs.c
        sylvan_sl.c
        sylvan_stats.c
        sylvan_table.c
        sylvan_zdd.c
        ${SYLVAN_HDRS}
)

## STORMCHANGED BEGIN
set(SYLVAN_STORM_HEADERS
        lace.h
        lace_config.h
        storm_wrapper.h
        sylvan_bdd_storm.h
        sylvan_mtbdd_storm.h
        sylvan_storm_rational_function.h
        sylvan_storm_rational_number.h
)
set(SYLVAN_STORM_SOURCES
        lace.c # TODO this should no longer be necessary.
        storm_wrapper.cpp
        sylvan_bdd_storm.c
        sylvan_mtbdd_storm.c
        sylvan_obj_storm.cpp
        sylvan_storm_rational_function.c
        sylvan_storm_rational_number.c
)
target_sources(sylvan PRIVATE
               ${SYLVAN_SYLVAN_STORM_HEADERS} ${SYLVAN_STORM_SOURCES})
## STORMCHANGED why are the headers additionally added as private sources here?
## -- to comply with what is done with the SYLVAN_HDRS above.

## END STORMCHANGED

option(SYLVAN_GMP "Include custom MTBDD type GMP")
if(SYLVAN_GMP)
    # We only want to include the custom MTBDD type GMP if we actually have the GMP library
    find_package(GMP REQUIRED)
    set(SYLVAN_HDRS ${SYLVAN_HDRS} sylvan_gmp.h)
    target_sources(sylvan PRIVATE sylvan_gmp.c sylvan_gmp.h)
    target_include_directories(sylvan PRIVATE ${GMP_INCLUDE_DIR})
    target_link_libraries(sylvan PUBLIC ${GMP_LIBRARIES})
    set(PKGC_LINK_GMP -lgmp)
else()
    set(PKGC_LINK_GMP "")
endif()

set_target_properties(sylvan PROPERTIES VERSION ${sylvan_VERSION} SOVERSION ${sylvan_VERSION_MAJOR})
## Stormchanged line below added SYLVAN_STORM_HEADERS
set_target_properties(sylvan PROPERTIES PUBLIC_HEADER "${SYLVAN_HDRS};${SYLVAN_STORM_HEADERS}")

# STORMCHANGED This is changed by storm as carl requires c++14
target_compile_features(sylvan PUBLIC c_std_11 cxx_std_14)

# STORMCHANGED Removed the extra warnings from the debug build, temporarily.
# STORMCHANGED Removed -march=native from this line and added the ability to set it below.
target_compile_options(sylvan PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe
        #$<$<CONFIG:Debug>:-O0 -Wall -Wextra>>
        $<$<CONFIG:Debug>:-O0>>
        $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Debug>:/Od /Wall /Zi>>
)
#STORMCHANGED Added ability to disable portable flag
option(SYLVAN_PORTABLE "Build portable" OFF)
if(NOT SYLVAN_PORTABLE)
    target_compile_options(sylvan PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
            -march=native>)
endif()
            
target_include_directories(sylvan PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>
)

# STORMCHANGED lace::lace removed.
target_link_libraries(sylvan PUBLIC m pthread)

option(SYLVAN_USE_MMAP "Let Sylvan use mmap to allocate (virtual) memory" ON)
if(SYLVAN_USE_MMAP)
    include(CheckSymbolExists)
    check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
    if(NOT HAVE_MMAP)
        message(WARNING "Sylvan - mmap not found: disabling mmap support")
        set(SYLVAN_USE_MMAP OFF)
    else()
        set_target_properties(sylvan PROPERTIES COMPILE_DEFINITIONS "SYLVAN_USE_MMAP")
    endif()
endif()

# Do we want to collect BDD statistics?
option(SYLVAN_STATS "Let Sylvan collect statistics at runtime" OFF)
if(SYLVAN_STATS)
    set_target_properties(sylvan PROPERTIES COMPILE_DEFINITIONS "SYLVAN_STATS")
endif()

set_target_properties(sylvan PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
)


## STORMCHANGED BEGIN

# TODO this is really ugly.
target_include_directories(sylvan PUBLIC
        $<BUILD_INTERFACE:${STORM_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${STORM_INCLUDE_INSTALL_DIR}>
)
target_include_directories(sylvan PUBLIC
        $<BUILD_INTERFACE:${STORM_BINARY_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${STORM_INCLUDE_INSTALL_DIR}>
)

option(USE_CARL "Sets whether carl should be included." ON)
if(USE_CARL)
    message(STATUS "Sylvan - Using CArL.")
    #    find_package(carl REQUIRED PATHS ${carl_DIR} NO_DEFAULT_PATH)
    target_link_libraries(sylvan PUBLIC lib_carl)
else()
    message(STATUS "Sylvan - Not using CArL.")
endif()

## STORMCHANGED END

## STORMCHANGED START
# TODO changed to sylvan_prefixed variables. Probably worth PR'ing Sylvan such that this is no longer necessary to change.
install(TARGETS sylvan
        EXPORT sylvan-targets
        ARCHIVE DESTINATION ${SYLVAN_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${SYLVAN_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${SYLVAN_INSTALL_BINDIR}
        INCLUDES DESTINATION ${SYLVAN_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${SYLVAN_INSTALL_INCLUDEDIR}
)

install(EXPORT sylvan-targets
        FILE sylvan-targets.cmake
        NAMESPACE sylvan::
        DESTINATION ${SYLVAN_CMAKE_INSTALL_DIR}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/sylvan-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/sylvan-config.cmake
        INSTALL_DESTINATION ${SYLVAN_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/sylvan-config-version.cmake
        VERSION ${SYLVAN_VERSION}
        COMPATIBILITY SameMinorVersion
)


#Remark: this file has not been updated, so I guess we could disable this.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sylvan.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/sylvan.pc" @ONLY)

install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/sylvan-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/sylvan-config-version.cmake
        DESTINATION ${SYLVAN_CMAKE_INSTALL_DIR}
)

#Remark: this file has not been updated, so I guess we could disable this.
install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/sylvan.pc
        DESTINATION ${SYLVAN_INSTALL_LIBDIR}/pkgconfig
)

export(EXPORT sylvan-targets
        FILE ${CMAKE_CURRENT_BINARY_DIR}/cmake/sylvan-targets.cmake
        NAMESPACE sylvan::
)
## STORMCHANGED END