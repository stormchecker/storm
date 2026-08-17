include(InstallRequiredSystemLibraries)

### General settings
# General project information is taken from the project() variables
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR "Storm Developers")
set(CPACK_PACKAGE_CONTACT "support@stormchecker.org")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

set(CPACK_GENERATOR "DEB")

### Source package configuration
# The support is limited and we recommend to use "git archive" instead
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "~$;[.]swp$;/[.]git/;.gitignore;/build/;tags;cscope.*")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src")

# Debian package
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libarchive-dev, libboost-dev, libcln-dev, libginac-dev, libglpk-dev, libgmp-dev, libxerces-c-dev, libz3-dev")

include(CPack)
