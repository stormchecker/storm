# Add all targets to the build-tree export set
export(EXPORT storm_Targets FILE "${PROJECT_BINARY_DIR}/stormTargets.cmake")

set(DEP_TARGETS "")
foreach(dt ${STORM_DEP_TARGETS})
	export_target(DEP_TARGETS ${dt})
endforeach()

set(EXP_OPTIONS "")
foreach(option ${EXPORTED_OPTIONS})
    set(EXP_OPTIONS "${EXP_OPTIONS}\nset(${option} \"${${option}}\")")
endforeach()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/stormConfigVersion.cmake
     VERSION ${STORM_VERSION_MAJOR}.${STORM_VERSION_MINOR}.${STORM_VERSION_PATCH}
     COMPATIBILITY SameMinorVersion )

configure_package_config_file(
        resources/cmake/stormConfig.cmake.in
        ${PROJECT_BINARY_DIR}/stormConfig.cmake
        INSTALL_DESTINATION ${PROJECT_BINARY_DIR}
        PATH_VARS PROJECT_BINARY_DIR
)

configure_package_config_file(
    resources/cmake/stormConfig.cmake.install.in
    ${PROJECT_BINARY_DIR}/stormConfig.cmake.install
    INSTALL_DESTINATION ${STORM_CMAKE_INSTALL_DIR}
    PATH_VARS STORM_INCLUDE_INSTALL_DIR
)

configure_file(
    resources/cmake/stormOptions.cmake.in
    ${PROJECT_BINARY_DIR}/stormOptions.cmake
)