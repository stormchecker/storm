file(GLOB_RECURSE STORM_CEX_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-counterexamples/*/*.cpp)
file(GLOB_RECURSE STORM_CEX_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-counterexamples" ${PROJECT_SOURCE_DIR}/src/storm-counterexamples/*/*.h)

add_library(storm-counterexamples SHARED ${STORM_CEX_SOURCES} ${STORM_CEX_HEADERS})
target_sources(storm-counterexamples
        PRIVATE
        ${STORM_CEX_SOURCES}
        PUBLIC
        FILE_SET fs_storm_cex_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_CEX_HEADERS})
storm_target_precompile_headers(storm-counterexamples REUSE_FROM storm)
set_target_properties(storm-counterexamples PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
target_link_libraries(storm-counterexamples PUBLIC storm)
set_target_properties(storm-counterexamples PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.

list(APPEND STORM_TARGETS storm-counterexamples)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-counterexamples EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_cex_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
