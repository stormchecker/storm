cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0054 OLD)
PROJECT( cpphoaf )

INCLUDE_DIRECTORIES("../include")

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    IF ("${CMAKE_CXX_FLAGS}" STREQUAL "")
       # Only set flags if there has been no flag set
       SET (CMAKE_CXX_FLAGS "-std=c++11 -Wall" CACHE STRING "Flags used by the compiler during all build types." FORCE)
    ENDIF()
ENDIF()

ADD_EXECUTABLE ( cpphoaf cpphoaf.cc )
