# Find cvc5
# Once done this will define
#  CVC5_FOUND - System has cvc5
#  CVC5_INCLUDE_DIRS - The cvc5 include directories
#  CVC5_LIBRARIES - The libraries needed to use cvc5
#  CVC5_VERSION - The version of cvc5 that was found

if (CVC5_ROOT)
    find_path(CVC5_INCLUDE_DIR cvc5/cvc5.h PATHS "${CVC5_ROOT}/include" NO_DEFAULT_PATH)
    find_library(CVC5_LIBRARY cvc5 PATHS "${CVC5_ROOT}/lib" NO_DEFAULT_PATH)
else()
    find_path(CVC5_INCLUDE_DIR cvc5/cvc5.h)
    find_library(CVC5_LIBRARY cvc5)
endif()

# If library found, check the version
if (CVC5_INCLUDE_DIR AND CVC5_LIBRARY)
    # Check the version by compiling and running a small test program. cvc5 exposes its version via
    # cvc5::Solver::getVersion(), which returns a string like "1.0.3".
    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/cvc5.cpp" "
    #include <iostream>
    #include <cvc5/cvc5.h>

    int main() {
      cvc5::Solver solver;
      std::cout << solver.getVersion() << std::endl;
      return 0;
    }
  ")

    try_run(
            VERSION_TEST_EXITCODE
            VERSION_TEST_COMPILED
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/cvc5.cpp
            COMPILE_DEFINITIONS
            -I"${CVC5_INCLUDE_DIR}"
            LINK_LIBRARIES ${CVC5_LIBRARY}
            CMAKE_FLAGS
            -DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}
            RUN_OUTPUT_VARIABLE
            VERSION_TEST_RUN_OUTPUT
    )

    if (NOT VERSION_TEST_COMPILED)
        unset(CVC5_INCLUDE_DIR CACHE)
        unset(CVC5_LIBRARY CACHE)
    elseif (NOT ("${VERSION_TEST_EXITCODE}" EQUAL 0))
        unset(CVC5_INCLUDE_DIR CACHE)
        unset(CVC5_LIBRARY CACHE)
    else()
        if ("${VERSION_TEST_RUN_OUTPUT}" MATCHES "([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(CVC5_VERSION "${CMAKE_MATCH_1}")
            # Only reject the library if the caller actually asked for a minimum version. Callers that need a
            # particular version check CVC5_VERSION themselves.
            if (CVC5_FIND_VERSION)
                if ("${CVC5_VERSION}" VERSION_LESS "${CVC5_FIND_VERSION}")
                    unset(CVC5_INCLUDE_DIR CACHE)
                    unset(CVC5_LIBRARY CACHE)
                elseif (CVC5_FIND_VERSION_EXACT AND NOT ("${CVC5_VERSION}" VERSION_EQUAL "${CVC5_FIND_VERSION}"))
                    unset(CVC5_INCLUDE_DIR CACHE)
                    unset(CVC5_LIBRARY CACHE)
                endif()
            endif()
        else()
            unset(CVC5_INCLUDE_DIR CACHE)
            unset(CVC5_LIBRARY CACHE)
        endif()
    endif()
endif()

set(CVC5_LIBRARIES ${CVC5_LIBRARY})
set(CVC5_INCLUDE_DIRS ${CVC5_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CVC5 DEFAULT_MSG CVC5_LIBRARY CVC5_INCLUDE_DIR)

mark_as_advanced(CVC5_INCLUDE_DIR CVC5_LIBRARY)
