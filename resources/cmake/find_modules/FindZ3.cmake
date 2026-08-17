# From https://github.com/SRI-CSL/sally/blob/master/cmake/FindZ3.cmake
# - Try to find Z3
# Once done this will define
#  Z3_FOUND - System has Z3
#  Z3_INCLUDE_DIRS - The Z3 include directories
#  Z3_LIBRARIES - The libraries needed to use Z3

if (Z3_ROOT)
    find_path(Z3_INCLUDE_DIR z3.h PATHS "${Z3_ROOT}/include" NO_DEFAULT_PATH)
else()
    find_path(Z3_INCLUDE_DIR z3.h)
endif()

if (Z3_ROOT)
    find_library(Z3_LIBRARY z3 PATHS "${Z3_ROOT}/lib" NO_DEFAULT_PATH)
else()
    find_library(Z3_LIBRARY z3)
endif()

# If library found, check the version
if (Z3_INCLUDE_DIR AND Z3_LIBRARY AND Z3_FIND_VERSION)

    # Check version from char *msat_get_version(void)
    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/z3.cpp" "
    #include <stdio.h>
    #include \"z3.h\"

    int main() {
      unsigned major, minor, build_number, revision_number;
      Z3_get_version(&major, &minor, &build_number, &revision_number);
      printf(\"%u.%u.%u.%u\\n\", major, minor, build_number, revision_number);
      return 0;
    }
  ")

    # Run the test program
    try_run(
            VERSION_TEST_EXITCODE
            VERSION_TEST_COMPILED
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/z3.cpp
            COMPILE_DEFINITIONS
            -I"${Z3_INCLUDE_DIR}"
            LINK_LIBRARIES ${Z3_LIBRARY} ${GMP_LIBRARY}
            CMAKE_FLAGS
            -DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}
            RUN_OUTPUT_VARIABLE
            VERSION_TEST_RUN_OUTPUT
    )

    if (NOT VERSION_TEST_COMPILED)
        unset(Z3_INCLUDE_DIR CACHE)
        unset(Z3_LIBRARY CACHE)
    elseif (NOT ("${VERSION_TEST_EXITCODE}" EQUAL 0))
        unset(Z3_INCLUDE_DIR CACHE)
        unset(Z3_LIBRARY CACHE)
    else()
        # Output is of the form: major.minor.build_number.revision
        if("${VERSION_TEST_RUN_OUTPUT}" MATCHES "([0-9]*\\.[0-9]*\\.[0-9]*\\.[0-9]*)")
            set(Z3_VERSION "${CMAKE_MATCH_1}")
            if ("${Z3_VERSION}" VERSION_LESS "${Z3_FIND_VERSION}")
                unset(Z3_INCLUDE_DIR CACHE)
                unset(Z3_LIBRARY CACHE)
            elseif (Z3_FIND_VERSION_EXACT AND NOT "${Z3_VERSION}" VERSION_EQUAL "${Z3_FIND_VERSION}")
                unset(Z3_INCLUDE_DIR CACHE)
                unset(Z3_LIBRARY CACHE)
            endif()
        else()
            unset(Z3_INCLUDE_DIR CACHE)
            unset(Z3_LIBRARY CACHE)
        endif()
    endif()
endif()

set(Z3_LIBRARIES ${Z3_LIBRARY})
set(Z3_INCLUDE_DIRS ${Z3_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Z3 DEFAULT_MSG Z3_LIBRARY Z3_INCLUDE_DIR)

mark_as_advanced(Z3_INCLUDE_DIR Z3_LIBRARY)