file(GLOB_RECURSE STORM_POMDP_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-pomdp/*/*.cpp)
file(GLOB_RECURSE STORM_POMDP_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-pomdp"  ${PROJECT_SOURCE_DIR}/src/storm-pomdp/*/*.h)

# Create storm-pomdp.
add_library(storm-pomdp SHARED)
target_sources(storm-pomdp
		PRIVATE
		${STORM_POMDP_SOURCES}
		PUBLIC
		FILE_SET fs_storm_pomdp_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_POMDP_HEADERS})
storm_target_precompile_headers(storm-pomdp REUSE_FROM storm)
set_target_properties(storm-pomdp PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
set_target_properties(storm-pomdp PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.
target_link_libraries(storm-pomdp PUBLIC storm storm-parsers storm-pars)

list(APPEND STORM_TARGETS storm-pomdp)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-pomdp  EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_pomdp_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
