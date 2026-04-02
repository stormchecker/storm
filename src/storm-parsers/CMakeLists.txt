file(GLOB_RECURSE STORM_PARSER_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-parsers/*/*.cpp)
file(GLOB_RECURSE STORM_PARSER_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-parsers" ${PROJECT_SOURCE_DIR}/src/storm-parsers/*/*.h)

# Disable Debug compiler flags for PrismParser to lessen memory consumption during compilation
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParser.cpp PROPERTIES COMPILE_FLAGS -g0)

# Create storm-parsers.
add_library(storm-parsers SHARED)
target_sources(storm-parsers
        PRIVATE
        ${STORM_PARSER_SOURCES}
        PUBLIC
        FILE_SET fs_storm_parsers_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_PARSER_HEADERS})
storm_target_precompile_headers(storm-parsers REUSE_FROM storm)
set_target_properties(storm-parsers PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
set_target_properties(storm-parsers PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.
target_link_libraries(storm-parsers PUBLIC storm)

list(APPEND STORM_TARGETS storm-parsers)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-parsers EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_parsers_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
