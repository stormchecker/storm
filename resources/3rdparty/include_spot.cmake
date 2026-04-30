set(STORM_HAVE_SPOT OFF)
set(STORM_SHIPPED_SPOT OFF)

if(NOT STORM_DISABLE_SPOT)
    # try to find Spot on the system
    if (NOT STORM_SPOT_FORCE_SHIPPED)
        if (NOT "${SPOT_ROOT}" STREQUAL "")
            message(STATUS "Storm - searching for Spot in ${SPOT_ROOT}")
            find_package(Spot QUIET PATHS ${SPOT_ROOT} NO_DEFAULT_PATH)
        endif()
        if (NOT Spot_FOUND)
            find_package(Spot QUIET)
        endif()

        if (Spot_FOUND)
            get_filename_component(Spot_LIB_DIR ${Spot_LIBRARIES} DIRECTORY)
            find_library(BUDDY_LIBRARY NAMES libbddx bddx PATHS ${Spot_LIB_DIR} NO_DEFAULT_PATH)
            if(NOT BUDDY_LIBRARY)
                message(FATAL_ERROR "Storm - Did not find BUDDY library that should ship with Spot. To work around this, you may force the shipped version of Spot with '-DSTORM_SHIPPED_SPOT=ON'.")
            endif()

            add_library(Storm::Spot-bddx UNKNOWN IMPORTED)
            set_target_properties(Storm::Spot-bddx PROPERTIES
                    IMPORTED_LOCATION ${BUDDY_LIBRARY})

            add_library(Storm::Spot UNKNOWN IMPORTED)
            set_target_properties(Storm::Spot PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${Spot_INCLUDE_DIR}"
                    IMPORTED_LOCATION "${Spot_LIBRARIES}"
                    INTERFACE_LINK_LIBRARIES Storm::Spot-bddx
            )

            message(STATUS "Storm - Using system version of Spot ${Spot_VERSION} (include: ${Spot_INCLUDE_DIR}, library: ${Spot_LIBRARIES}).")
            list(APPEND STORM_DEP_IMP_TARGETS Storm::Spot)

            set(STORM_HAVE_SPOT ON)
        endif()
    endif()

    if(NOT STORM_HAVE_SPOT)
        # Spot does not set library IDs with an rpath but with an absolute path.
        if (MACOSX)
            # We need to work on these .0 versions as otherwise we cannot fix the RPATHs.
            # What we fix here is to ensure that the ids are of the form @rpath/libname
            # We also update the entry of buddy in spot such that the spot lib looks for buddy in the same folder where it resides.
            set(SPOT_RPATH_FIX_COMMAND1 "install_name_tool;-id;@rpath/libspot.0.dylib;${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}")
            set(SPOT_RPATH_FIX_COMMAND2 "install_name_tool;-add_rpath;@loader_path;${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}")
            set(BDDX_RPATH_FIX_COMMAND1 "install_name_tool;-change;${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx.0.dylib;@loader_path/libbddx.0.dylib;${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}")
            set(BDDX_RPATH_FIX_COMMAND2 "install_name_tool;-id;@rpath/libbddx.0.dylib;${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT}")

        else()
            set(SPOT_RPATH_FIX_COMMAND1 "true") #no op
            set(SPOT_RPATH_FIX_COMMAND2 "true") #no op
            set(BDDX_RPATH_FIX_COMMAND1 "true") #no op
            set(BDDX_RPATH_FIX_COMMAND2 "true") #no op
        endif()

        # download and install shipped Spot as shared libraries.
        # set Spot version
        set(SPOT_SHIPPED_VERSION 2.14.3)
        set(STORM_SPOT_FLAGS "--disable-python;--enable-shared;--disable-static")
        if (NOT STORM_DEBUG_SPOT)
            set(STORM_SPOT_FLAGS "${STORM_SPOT_FLAGS};--disable-devel;--disable-debug;--enable-optimizations")
        else()
            message(WARNING "Storm - Building Spot in DEBUG mode.")
            set(STORM_SPOT_FLAGS "${STORM_SPOT_FLAGS};--disable-devel;--enable-debug;--disable-optimizations")
        endif()
        if (CCACHE_FOUND)
            set(STORM_SPOT_FLAGS "${STORM_SPOT_FLAGS};CC=ccache\\ ${CMAKE_C_COMPILER};CXX=ccache\\ ${CMAKE_CXX_COMPILER}")
        endif()
        ExternalProject_Add(Spot
                URL https://www.lre.epita.fr/dload/spot/spot-${SPOT_SHIPPED_VERSION}.tar.gz https://www.lrde.epita.fr/dload/spot/spot-${SPOT_SHIPPED_VERSION}.tar.gz
                DOWNLOAD_NO_PROGRESS TRUE
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE # Prevents reconfiguration, see https://gitlab.kitware.com/cmake/cmake/-/issues/24003
                DOWNLOAD_DIR ${STORM_3RDPARTY_BINARY_DIR}/spot_src
                SOURCE_DIR ${STORM_3RDPARTY_BINARY_DIR}/spot_src
                PREFIX ${STORM_3RDPARTY_BINARY_DIR}/spot
                CONFIGURE_COMMAND ${STORM_3RDPARTY_BINARY_DIR}/spot_src/configure --prefix=${STORM_3RDPARTY_BINARY_DIR}/spot ${STORM_SPOT_FLAGS}
                BUILD_COMMAND make -j${STORM_RESOURCES_BUILD_JOBCOUNT}
                INSTALL_COMMAND make install -j${STORM_RESOURCES_BUILD_JOBCOUNT}
                COMMAND ${SPOT_RPATH_FIX_COMMAND1}
                COMMAND ${SPOT_RPATH_FIX_COMMAND2}
                COMMAND ${BDDX_RPATH_FIX_COMMAND1}
                COMMAND ${BDDX_RPATH_FIX_COMMAND2}
                LOG_DOWNLOAD ON
                LOG_CONFIGURE ON
                LOG_BUILD ON
                LOG_INSTALL ON
                LOG_OUTPUT_ON_FAILURE ON
                BUILD_BYPRODUCTS ${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT};${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT}
        )
        add_dependencies(storm_resources Spot)

        set(Spot_INCLUDE_DIR "${STORM_3RDPARTY_BINARY_DIR}/spot/include/")
        set(Spot_LIBRARIES "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT};${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT}")
        set(Spot_INSTALL_DIR ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/spot/)
        set(STORM_HAVE_SPOT ON)
        set(STORM_SHIPPED_SPOT ON)

        file(MAKE_DIRECTORY ${Spot_INCLUDE_DIR}) # Workaround https://gitlab.kitware.com/cmake/cmake/-/issues/15052
        add_library(Storm::Spot-bddx SHARED IMPORTED)
        target_include_directories(Storm::Spot-bddx INTERFACE "${STORM_3RDPARTY_BINARY_DIR}/spot/include/")
        set_target_properties(Storm::Spot-bddx PROPERTIES
                IMPORTED_LOCATION ${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT})

        add_library(Storm::Spot SHARED IMPORTED)
        target_link_libraries(Storm::Spot INTERFACE Storm::Spot-bddx)
        target_include_directories(Storm::Spot  INTERFACE "${STORM_3RDPARTY_BINARY_DIR}/spot/include/")
        set_target_properties(Storm::Spot PROPERTIES
                IMPORTED_LOCATION ${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}
        )

        install(FILES ${Spot_LIBRARIES} DESTINATION ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
        # Spot_LIBRARIES are symlinks, install the actual library files as well
        if (MACOSX)
                install(FILES "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot.0${DYNAMIC_EXT}" "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx.0${DYNAMIC_EXT}" DESTINATION ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
        else()
                install(FILES "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}.0" "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT}.0" DESTINATION ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
                install(FILES "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libspot${DYNAMIC_EXT}.0.0.0" "${STORM_3RDPARTY_BINARY_DIR}/spot/lib/libbddx${DYNAMIC_EXT}.0.0.0" DESTINATION ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
        endif()
        install(DIRECTORY ${Spot_INCLUDE_DIR}/ DESTINATION ${Spot_INSTALL_DIR}
                FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN ".git" EXCLUDE)

        list(APPEND STORM_DEP_IMP_TARGETS Storm::Spot)
        add_dependencies(storm_resources Storm::Spot )

        message(STATUS "Storm - Using shipped version of Spot ${SPOT_SHIPPED_VERSION} (include: ${Spot_INCLUDE_DIR}, library ${Spot_LIBRARIES}).")
        message(WARNING "Configuring and building SPOT takes significant time in which the build process does not provide any output.")

    endif()
endif()
if(NOT STORM_HAVE_SPOT)
    message("Storm - Not linking with Spot. Model checking of LTL formulas (beyond PCTL) will not be supported.")
endif()
