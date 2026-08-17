# - Try to find libSpot
# Once done this will define
#  Spot_FOUND - System has glpk
#  Spot_INCLUDE_DIR - The glpk include directory
#  Spot_LIBRARIES - The libraries needed to use glpk
#  Spot_VERSION - The version of Spot

# use pkg-config to get the directories and then use these values

# in the find_path() and find_library() calls
find_package(PkgConfig QUIET)
PKG_CHECK_MODULES(PC_Spot QUIET Spot)

find_path(Spot_INCLUDE_DIR NAMES spot/misc/_config.h
   HINTS
   ${PC_Spot_INCLUDEDIR}
   ${PC_Spot_INCLUDE_DIRS}
   )

find_library(Spot_LIBRARIES NAMES spot
   HINTS
   ${PC_Spot_LIBDIR}
   ${PC_Spot_LIBRARY_DIRS}
   )

if(PC_Spot_VERSION)
    set(Spot_VERSION ${PC_Spot_VERSION})
elseif(Spot_INCLUDE_DIR AND EXISTS "${Spot_INCLUDE_DIR}/spot/misc/_config.h")
    file(STRINGS "${Spot_INCLUDE_DIR}/spot/misc/_config.h" Spot_VERSION
         REGEX "^#define[\t ]+Spot_VERSION[\t ]+\".+\"")
    string(REGEX REPLACE "^#define[\t ]+Spot_VERSION[\t ]+\"(.+)\"" "\\1" Spot_VERSION "${Spot_VERSION}")
endif()

# handle the QUIETLY and REQUIRED arguments and set Spot_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Spot
                                  REQUIRED_VARS Spot_LIBRARIES Spot_INCLUDE_DIR
                                  VERSION_VAR Spot_VERSION)

mark_as_advanced(Spot_INCLUDE_DIR Spot_LIBRARIES)
