# Create storm-pars.
add_executable(storm-pars-cli ${PROJECT_SOURCE_DIR}/src/storm-pars-cli/storm-pars.cpp
                              ${PROJECT_SOURCE_DIR}/src/storm-pars-cli/print.cpp
                              ${PROJECT_SOURCE_DIR}/src/storm-pars-cli/monotonicity.cpp
                              ${PROJECT_SOURCE_DIR}/src/storm-pars-cli/feasibility.cpp
)
target_link_libraries(storm-pars-cli storm-pars storm-cli-utilities)
target_include_directories(storm-pars-cli PRIVATE "${PROJECT_SOURCE_DIR}/src")
set_target_properties(storm-pars-cli PROPERTIES OUTPUT_NAME "storm-pars")
set_target_properties(storm-pars-cli PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
storm_target_precompile_headers(storm-pars-cli REUSE_FROM storm-cli)

add_dependencies(binaries storm-pars-cli)

# installation
install(TARGETS storm-pars-cli EXPORT storm_Targets RUNTIME DESTINATION bin LIBRARY DESTINATION lib OPTIONAL)
