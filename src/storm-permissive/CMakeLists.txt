file(GLOB_RECURSE STORM_PERMISSIVE_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-permissive/*/*.cpp)
file(GLOB_RECURSE STORM_PERMISSIVE_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-permissive" ${PROJECT_SOURCE_DIR}/src/storm-permissive/*/*.h)

# Create storm-permissive.
add_library(storm-permissive SHARED)
target_sources(storm-permissive
        PRIVATE
        ${STORM_PERMISSIVE_SOURCES}
        PUBLIC
        FILE_SET fs_storm_permissive_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_PERMISSIVE_HEADERS})
storm_target_precompile_headers(storm-permissive REUSE_FROM storm)
set_target_properties(storm-permissive PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
set_target_properties(storm-permissive PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.
target_link_libraries(storm-permissive PUBLIC storm)

list(APPEND STORM_TARGETS storm-permissive)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-permissive EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_permissive_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
