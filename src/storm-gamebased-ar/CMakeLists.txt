file(GLOB_RECURSE STORM_GBAR_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-gamebased-ar/*/*.cpp)
file(GLOB_RECURSE STORM_GBAR_HEADERS RELATIVE " ${PROJECT_SOURCE_DIR}/src/storm-gamebased-ar" ${PROJECT_SOURCE_DIR}/src/storm-gamebased-ar/*/*.h)

add_library(storm-gamebased-ar SHARED)
target_sources(storm-gamebased-ar
		PRIVATE
		${STORM_GBAR_SOURCES}
		PUBLIC
		FILE_SET fs_storm_gbar_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_GBAR_HEADERS})
storm_target_precompile_headers(storm-gamebased-ar REUSE_FROM storm)
set_target_properties(storm-gamebased-ar PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
target_link_libraries(storm-gamebased-ar PUBLIC storm)
set_target_properties(storm-gamebased-ar PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.

list(APPEND STORM_TARGETS storm-gamebased-ar)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-gamebased-ar EXPORT storm_Targets
		RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
		LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
		FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
		FILE_SET fs_storm_gbar_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
