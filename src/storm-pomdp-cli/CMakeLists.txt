# Create storm-pomdp.
file(GLOB_RECURSE STORM_POMDP_CLI_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-pomdp-cli/*/*.cpp)

add_executable(storm-pomdp-cli ${PROJECT_SOURCE_DIR}/src/storm-pomdp-cli/storm-pomdp.cpp ${STORM_POMDP_CLI_SOURCES})
target_link_libraries(storm-pomdp-cli storm-pomdp storm-cli-utilities)
set_target_properties(storm-pomdp-cli PROPERTIES OUTPUT_NAME "storm-pomdp")
target_include_directories(storm-pomdp-cli PRIVATE "${PROJECT_SOURCE_DIR}/src")
storm_target_precompile_headers(storm-pomdp-cli REUSE_FROM storm-cli)
set_target_properties(storm-pomdp-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})

add_dependencies(binaries storm-pomdp-cli)

# installation
install(TARGETS storm-pomdp-cli
        EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        OPTIONAL
)
