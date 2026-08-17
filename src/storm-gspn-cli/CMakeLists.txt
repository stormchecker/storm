# Create storm-gspn.
add_executable(storm-gspn-cli ${PROJECT_SOURCE_DIR}/src/storm-gspn-cli/storm-gspn.cpp)
target_link_libraries(storm-gspn-cli storm-gspn storm-cli-utilities)
set_target_properties(storm-gspn-cli PROPERTIES OUTPUT_NAME "storm-gspn")
set_target_properties(storm-gspn-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
storm_target_precompile_headers(storm-gspn-cli REUSE_FROM storm-cli)

add_dependencies(binaries storm-gspn-cli)

# installation
install(TARGETS storm-gspn-cli
        EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        OPTIONAL
)
