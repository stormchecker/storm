file(GLOB_RECURSE STORM_CLI_UTIL_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-cli-utilities/*.cpp)
file(GLOB_RECURSE STORM_CLI_UTIL_HEADERS  RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-cli-utilities" ${PROJECT_SOURCE_DIR}/src/storm-cli-utilities/*.h)


# Create storm-cli-utilities.
add_library(storm-cli-utilities SHARED)
target_sources(storm-cli-utilities
		PRIVATE
		${STORM_CLI_UTIL_SOURCES}
		PUBLIC
		FILE_SET fs_storm_cli_utilities_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_CLI_UTIL_HEADERS})
storm_target_precompile_headers(storm-cli-utilities REUSE_FROM storm)
set_target_properties(storm-cli-utilities PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
target_link_libraries(storm-cli-utilities PUBLIC storm storm-counterexamples storm-gamebased-ar storm-parsers storm-version-info)
set_target_properties(storm-cli-utilities PROPERTIES DEFINE_SYMBOL "") # to avoid problems with pch on linux.
list(APPEND STORM_TARGETS storm-cli-utilities)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-cli-utilities EXPORT storm_Targets
		RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
		LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
		FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
		FILE_SET fs_storm_cli_utilities_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})

