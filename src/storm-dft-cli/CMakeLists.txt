# Create storm-dft.
add_executable(storm-dft-cli ${PROJECT_SOURCE_DIR}/src/storm-dft-cli/storm-dft.cpp)
target_link_libraries(storm-dft-cli storm-dft storm-cli-utilities)
set_target_properties(storm-dft-cli PROPERTIES OUTPUT_NAME "storm-dft")
set_target_properties(storm-dft-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
storm_target_precompile_headers(storm-dft-cli REUSE_FROM storm-cli)

add_dependencies(binaries storm-dft-cli)

# installation
install(TARGETS storm-dft-cli
        EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        OPTIONAL
)
