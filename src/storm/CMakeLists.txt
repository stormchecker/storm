#############################################################
##
##	Source file aggregation and clustering
##
#############################################################
file(GLOB_RECURSE STORM_SOURCES ${PROJECT_SOURCE_DIR}/src/storm/*/*.cpp)
file(GLOB_RECURSE STORM_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm" ${PROJECT_SOURCE_DIR}/src/storm/*.h )

# Additional include files like the storm-config.h
file(GLOB_RECURSE STORM_BUILD_HEADERS ${PROJECT_BINARY_DIR}/include/storm-config.h)

###############################################################################
##                                                                            
##	Binary creation
##
###############################################################################
# Create libstorm.
add_library(storm SHARED)
target_sources(storm PRIVATE ${STORM_SOURCES})
target_sources(storm PUBLIC FILE_SET fs_storm_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${STORM_HEADERS})
target_sources(storm PUBLIC FILE_SET fs_storm_configured_headers TYPE HEADERS BASE_DIRS "${PROJECT_BINARY_DIR}/include" FILES ${STORM_BUILD_HEADERS})
storm_target_precompile_headers(storm PRIVATE ${STORM_PRECOMPILED_HEADERS})
set_target_properties(storm PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
set_target_properties(storm PROPERTIES DEFINE_SYMBOL "") # to avoid problems with pch on linux.
set_target_properties(storm PROPERTIES BUILD_RPATH ${STORM_3RDPARTY_BINARY_DIR}/spot/lib/)
add_dependencies(storm storm_resources)
#The library that needs symbols must be first, then the library that resolves the symbol.
target_link_libraries(storm PUBLIC ${STORM_DEP_TARGETS} ${STORM_DEP_IMP_TARGETS} ${STORM_FETCHED_TARGETS})
list(APPEND STORM_TARGETS storm)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm
        EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR}
        FILE_SET fs_storm_configured_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
