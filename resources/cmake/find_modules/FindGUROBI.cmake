#### Taken from http://www.openflipper.org/svnrepo/CoMISo/trunk/CoMISo/cmake/FindGUROBI.cmake
#### This file needs to be updated regularly to include new Gurobi releases.
#### Also update the error message in the Gurobi section of ../resources/CMakeLists.txt

# - Try to find GUROBI
# Once done this will define
#  GUROBI_FOUND - System has Gurobi
#  GUROBI_INCLUDE_DIRS - The Gurobi include directories
#  GUROBI_LIBRARIES - The libraries needed to use Gurobi

if (GUROBI_INCLUDE_DIR)
  # in cache already
  set(GUROBI_FOUND TRUE)
  set(GUROBI_INCLUDE_DIRS "${GUROBI_INCLUDE_DIR}" )
  set(GUROBI_LIBRARIES "${GUROBI_LIBRARY};${GUROBI_CXX_LIBRARY}" )
else (GUROBI_INCLUDE_DIR)

find_path(GUROBI_INCLUDE_DIR 
          NAMES gurobi_c++.h
          PATHS "$ENV{GUROBI_HOME}/include"
                    "/Library/gurobi1300/macos_universal2/include"
                    "/Library/gurobi1202/macos_universal2/include"
                    "/Library/gurobi1200/macos_universal2/include"
                    "/Library/gurobi1102/macos_universal2/include"
                    "/Library/gurobi1100/macos_universal2/include"
                    "/Library/gurobi1000/macos_universal2/include"
                    "/Library/gurobi951/macos_universal2/include"
                    "/Library/gurobi950/mac64/include"
                    "/Library/gurobi912/mac64/include"
                    "/Library/gurobi911/mac64/include"
                    "/Library/gurobi900/mac64/include"
                    "/Library/gurobi811/mac64/include"
                    "/Library/gurobi810/mac64/include"
                    "/Library/gurobi801/mac64/include"
                    "/Library/gurobi702/mac64/include"
                    "/Library/gurobi652/mac64/include"
                    "/Library/gurobi651/mac64/include"
                    "/Library/gurobi650/mac64/include"
                    "/Library/gurobi605/mac64/include"
                    "/Library/gurobi604/mac64/include"
                    "/Library/gurobi602/mac64/include"
                    "/Library/gurobi502/mac64/include"
                    "${GUROBI_ROOT}/include"
          )

find_library( GUROBI_LIBRARY 
              NAMES gurobi
        gurobi130
        gurobi120
        gurobi110
        gurobi100
        gurobi95
        gurobi91
        gurobi90
        gurobi81
        gurobi80
        gurobi75
        gurobi70
        gurobi65
        gurobi60
        gurobi56
        gurobi55
        gurobi52
        gurobi51
        gurobi50
        gurobi46
        gurobi45
              PATHS "$ENV{GUROBI_HOME}/lib"
                    "/Library/gurobi1300/macos_universal2/lib"
                    "/Library/gurobi1202/macos_universal2/lib"
                    "/Library/gurobi1200/macos_universal2/lib"
                    "/Library/gurobi1102/macos_universal2/lib"
                    "/Library/gurobi1100/macos_universal2/lib"
                    "/Library/gurobi1000/macos_universal2/lib"
                    "/Library/gurobi951/macos_universal2/lib"
                    "/Library/gurobi950/mac64/lib"
                    "/Library/gurobi912/mac64/lib"
                    "/Library/gurobi911/mac64/lib"
                    "/Library/gurobi900/mac64/lib"
                    "/Library/gurobi811/mac64/lib"
                    "/Library/gurobi810/mac64/lib"
                    "/Library/gurobi801/mac64/lib"
                    "/Library/gurobi702/mac64/lib"
                    "/Library/gurobi652/mac64/lib"
                    "/Library/gurobi651/mac64/lib"
                    "/Library/gurobi650/mac64/lib"
                    "/Library/gurobi605/mac64/lib"
                    "/Library/gurobi604/mac64/lib"
                    "/Library/gurobi602/mac64/lib"
                    "/Library/gurobi502/mac64/lib"
                    "${GUROBI_ROOT}/lib"
              )

find_library( GUROBI_CXX_LIBRARY 
              NAMES gurobi_c++
              PATHS "$ENV{GUROBI_HOME}/lib"
                    "/Library/gurobi1300/macos_universal2/lib"
                    "/Library/gurobi1202/macos_universal2/lib"
                    "/Library/gurobi1200/macos_universal2/lib"
                    "/Library/gurobi1102/macos_universal2/lib"
                    "/Library/gurobi1100/macos_universal2/lib"
                    "/Library/gurobi951/macos_universal2/lib"
                    "/Library/gurobi950/mac64/lib"
                    "/Library/gurobi912/mac64/lib"
                    "/Library/gurobi911/mac64/lib"
                    "/Library/gurobi900/mac64/lib"
                    "/Library/gurobi811/mac64/lib"
                    "/Library/gurobi810/mac64/lib"
                    "/Library/gurobi801/mac64/lib"
                    "/Library/gurobi702/mac64/lib"
                    "/Library/gurobi652/mac64/lib"
                    "/Library/gurobi651/mac64/lib"
                    "/Library/gurobi650/mac64/lib"
                    "/Library/gurobi605/mac64/lib"
                    "/Library/gurobi604/mac64/lib"
                    "/Library/gurobi602/mac64/lib"
                    "/Library/gurobi502/mac64/lib"

                    "${GUROBI_ROOT}/lib"
              )

set(GUROBI_INCLUDE_DIRS "${GUROBI_INCLUDE_DIR}" )
set(GUROBI_LIBRARIES "${GUROBI_LIBRARY};${GUROBI_CXX_LIBRARY}" )

# use c++ headers as default
# set(GUROBI_COMPILER_FLAGS "-DIL_STD" CACHE STRING "Gurobi Compiler Flags")

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBCPLEX_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GUROBI  DEFAULT_MSG
                                  GUROBI_LIBRARY GUROBI_CXX_LIBRARY GUROBI_INCLUDE_DIR)

mark_as_advanced(GUROBI_INCLUDE_DIR GUROBI_LIBRARY GUROBI_CXX_LIBRARY)

endif(GUROBI_INCLUDE_DIR)
