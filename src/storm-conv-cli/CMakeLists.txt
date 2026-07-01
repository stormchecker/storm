# Create storm-conv.
add_executable(storm-conv-cli ${PROJECT_SOURCE_DIR}/src/storm-conv-cli/storm-conv.cpp)
target_link_libraries(storm-conv-cli storm-conv storm-cli-utilities)
set_target_properties(storm-conv-cli PROPERTIES OUTPUT_NAME "storm-conv")
set_target_properties(storm-conv-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
storm_target_precompile_headers(storm-conv-cli REUSE_FROM storm-cli)

add_dependencies(binaries storm-conv-cli)

# installation
install(TARGETS storm-conv-cli
        EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        OPTIONAL
)
