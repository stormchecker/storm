# Try to find libmathsat
# Once done this will define
#  MATHSAT_FOUND - System has mathsat
#  MATHSAT_INCLUDE_DIRS - The mathsat include directory
#  MATHSAT_LIBRARIES - The libraries needed to use mathsat
find_path(MATHSAT_INCLUDE_DIRS NAMES mathsat.h
    PATHS ENV PATH INCLUDE "${MATHSAT_ROOT}/include" "/usr/include/mathsat" "/usr/local/include/mathsat/" "/opt/mathsat/include"
)
find_library(MATHSAT_LIBRARIES mathsat
    PATHS ENV PATH INCLUDE "${MATHSAT_ROOT}/lib" "/usr/include/mathsat" "/usr/local/include/mathsat/" "/opt/mathsat/lib"
)

# handle the QUIETLY and REQUIRED arguments and set MATHSAT_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MATHSAT
REQUIRED_VARS MATHSAT_LIBRARIES MATHSAT_INCLUDE_DIRS
)
mark_as_advanced(MATHSAT_INCLUDE_DIR MATHSAT_LIBRARIES)