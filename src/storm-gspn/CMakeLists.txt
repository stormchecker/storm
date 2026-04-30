file(GLOB_RECURSE STORM_GSPN_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-gspn/*/*.cpp)
file(GLOB_RECURSE STORM_GSPN_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-gspn" ${PROJECT_SOURCE_DIR}/src/storm-gspn/*/*.h)

add_library(storm-gspn SHARED)
target_sources(storm-gspn
		PRIVATE
		${STORM_GSPN_SOURCES}
		PUBLIC
		FILE_SET fs_storm_gspn_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_GSPN_HEADERS})
storm_target_precompile_headers(storm-gspn REUSE_FROM storm)
set_target_properties(storm-gspn PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
target_link_libraries(storm-gspn PUBLIC storm storm-conv storm-parsers ${STORM_GSPN_LINK_LIBRARIES})
set_target_properties(storm-gspn PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.

list(APPEND STORM_TARGETS storm-gspn)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-gspn EXPORT storm_Targets
		RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
		LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
		FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
		FILE_SET fs_storm_gspn_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
