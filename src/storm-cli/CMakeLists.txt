# Create main binary storm.
add_executable(storm-cli ${PROJECT_SOURCE_DIR}/src/storm-cli/storm-cli.cpp)
target_link_libraries(storm-cli storm storm-cli-utilities)
set_target_properties(storm-cli PROPERTIES OUTPUT_NAME "storm")
set_target_properties(storm-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
storm_target_precompile_headers(storm-cli PRIVATE ${STORM_PRECOMPILED_HEADERS})

add_dependencies(binaries storm-cli)

# installation
install(TARGETS storm-cli EXPORT storm_Targets
        RUNTIME DESTINATION bin LIBRARY DESTINATION lib OPTIONAL)
