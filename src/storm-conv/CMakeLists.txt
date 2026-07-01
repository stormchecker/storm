file(GLOB_RECURSE STORM_CONV_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-conv/*/*.cpp)
file(GLOB_RECURSE STORM_CONV_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-conv" ${PROJECT_SOURCE_DIR}/src/storm-conv/*/*.h)

# Create storm-conv.
add_library(storm-conv SHARED)
target_sources(storm-conv
        PRIVATE
        ${STORM_CONV_SOURCES}
        PUBLIC
        FILE_SET fs_storm_conv_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_CONV_HEADERS})
storm_target_precompile_headers(storm-conv REUSE_FROM storm)
set_target_properties(storm-conv PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
target_link_libraries(storm-conv PUBLIC storm)
set_target_properties(storm-conv PROPERTIES DEFINE_SYMBOL "") # to avoid problems with pch on linux.

list(APPEND STORM_TARGETS storm-conv)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)


# installation
install(TARGETS storm-conv EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_conv_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
