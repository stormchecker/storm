if(PROJECT_TOP_LEVEL)
    add_custom_target(resources)
    add_dependencies(resources storm_resources)
endif()
add_custom_target(test-resources)

include(FetchContent)

message(STATUS "Storm - Building external resources with ${STORM_RESOURCES_BUILD_JOBCOUNT} job(s) in parallel.")

#############################################################
##
##	l3pp
##
#############################################################

# Do not take a branch, needs internet connection.
message(STATUS "Storm - Including l3pp logging.")
ExternalProject_Add(
        l3pp_ext
        GIT_REPOSITORY https://github.com/hbruintjes/l3pp.git
        GIT_TAG 513c7589232396e4b8975113b7709a8f39b46c85
        SOURCE_DIR ${STORM_3RDPARTY_BINARY_DIR}/l3pp
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_INSTALL ON
        LOG_DOWNLOAD ON
)
add_library(l3pp INTERFACE) # Not imported, we are in control of the sources.
add_dependencies(l3pp l3pp_ext)
target_include_directories(l3pp INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_BINARY_DIR}/l3pp>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/l3pp>
)
install(TARGETS l3pp EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_BINARY_DIR}/l3pp/ DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/l3pp FILES_MATCHING PATTERN "*.h" PATTERN ".git" EXCLUDE)
add_dependencies(storm_resources l3pp)
list(APPEND STORM_DEP_TARGETS l3pp)

#############################################################
##
## LibArchive (optional)
##
############################################################# 

if(NOT STORM_DISABLE_LIBARCHIVE)
    if(MACOSX AND LibArchive_ROOT STREQUAL "")
        # macOS already provides libarchive but no include files.
        # We catch the common case where libarchive is installed via homebrew following https://stackoverflow.com/a/76916842.
        # Homebrew ships libarchive keg only. We try to get a hint where libarchive is located
        find_program(_LibArchive_BREW_EXECUTABLE brew)
        if (_LibArchive_BREW_EXECUTABLE)
            execute_process(
                    COMMAND "${_LibArchive_BREW_EXECUTABLE}" --prefix libarchive
                    ERROR_QUIET # Ignore any error, e.g. if brew is not installed.
                    OUTPUT_VARIABLE LibArchive_ROOT
                    OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif ()
        unset(_LibArchive_BREW_EXECUTABLE)
    endif()
    find_package(LibArchive QUIET)
    if(LibArchive_FOUND)
        SET(STORM_HAVE_LIBARCHIVE ON)
        message (STATUS "Storm - Linking with LibArchive (version: ${LibArchive_VERSION}, libraries: ${LibArchive_LIBRARIES}, include: ${LibArchive_INCLUDE_DIRS}).")
        list(APPEND STORM_DEP_IMP_TARGETS LibArchive::LibArchive)
    else()
        SET(STORM_HAVE_LIBARCHIVE OFF)
        message (STATUS "Storm - LibArchive not found. Loading and exporting UMB models and compressed files will not be supported.")
	endif()
else()
    SET(STORM_HAVE_LIBARCHIVE OFF)
    message (STATUS "Storm - Not linking with LibArchive. Loading and exporting UMB models and compressed files will not be supported.")
endif()

#############################################################
##
##	gmm
##
#############################################################

if(NOT STORM_DISABLE_GMM)
    set(GMM_VERSION "5.4.4")
    message(STATUS "Storm - Including gmm ${GMM_VERSION}.")
    ExternalProject_Add(
            gmm_src
            URL https://download-mirror.savannah.gnu.org/releases/getfem/stable/gmm-${GMM_VERSION}.tar.gz
            SOURCE_DIR ${STORM_3RDPARTY_BINARY_DIR}/gmm
            # Try to apply patch and otherwise check that patch was already applied
            # Needed because while -N ignores already applied patches it still returns with exit code 1
            PATCH_COMMAND patch -N -p1 -s -i ${STORM_3RDPARTY_SOURCE_DIR}/patches/gmm544.patch || patch --dry-run -N -p1 -s -i ${STORM_3RDPARTY_SOURCE_DIR}/patches/gmm544.patch | grep "Reversed (or previously applied) patch detected!"
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_INSTALL ON
            LOG_DOWNLOAD ON
    )
    add_library(gmm INTERFACE) # Not imported, we are in control of the sources.
    add_dependencies(gmm gmm_src)
    target_include_directories(gmm INTERFACE
            $<BUILD_INTERFACE:${STORM_3RDPARTY_BINARY_DIR}/gmm/include>
            $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}>
    )
    install(TARGETS gmm EXPORT storm_Targets)
    install(DIRECTORY ${STORM_3RDPARTY_BINARY_DIR}/gmm/include/gmm
            DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
            FILES_MATCHING PATTERN "*.h" PATTERN ".git" EXCLUDE)
    list(APPEND STORM_DEP_TARGETS gmm)
    set(STORM_HAVE_GMM ON)
else()
    message(STATUS "Storm - Not linking with gmm.")
    set(STORM_HAVE_GMM OFF)
endif()

#############################################################
##
##	Eigen
##
#############################################################

# Checkout Eigen
# https://gitlab.com/libeigen/eigen
set(STORM_EIGEN_GIT_TAG "bc3b39870ecb690a623a3f49149a358b95c5781d")
message(STATUS "Storm - Including Eigen commit ${STORM_EIGEN_GIT_TAG} (Version 5.0.1).")
ExternalProject_Add(
        eigen_src
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG ${STORM_EIGEN_GIT_TAG}
        SOURCE_DIR ${STORM_3RDPARTY_BINARY_DIR}/StormEigen/StormEigen
        # First check whether patch was already applied (--reverse --check), otherwise apply patch
        PATCH_COMMAND git apply ${STORM_3RDPARTY_SOURCE_DIR}/patches/eigen501.patch --reverse --check -q || git apply ${STORM_3RDPARTY_SOURCE_DIR}/patches/eigen501.patch
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_INSTALL ON
)
add_library(eigenstorm INTERFACE) # Not imported, we are in control of the sources.
add_dependencies(eigenstorm eigen_src)
if(STORM_WARNING_AS_ERROR AND STORM_COMPILER_GCC)
        # Ignore warning in Eigen
        target_compile_options(eigenstorm INTERFACE -Wno-class-memaccess)
endif()
# note that we include without StormEigen
target_include_directories(eigenstorm INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_BINARY_DIR}/StormEigen>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/StormEigen>
)
install(TARGETS eigenstorm EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_BINARY_DIR}/StormEigen/StormEigen
        DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.h" PATTERN ".git" EXCLUDE)
add_dependencies(storm_resources eigenstorm)
list(APPEND STORM_DEP_TARGETS eigenstorm)

#############################################################
##
##	Boost
##
#############################################################

# Boost Option variables
set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost 1.70.0 QUIET REQUIRED NO_MODULE)
if(${Boost_VERSION} VERSION_GREATER_EQUAL "1.81.0")
    message(STATUS "Storm - Using workaround for Boost >= 1.81")
    add_compile_options(-DBOOST_PHOENIX_STL_TUPLE_H_) # or add compile_definitions?
    # TODO Only do this for a specific target?
endif()

if(${Boost_VERSION} VERSION_LESS "1.75.0") # Debian 12 ships with Boost 1.74.0
    message(STATUS "Storm - Loading Boost.PFR as it is not part of Boost < 1.75.0")
    ExternalProject_Add(
            boost_pfr_src
            GIT_REPOSITORY https://github.com/boostorg/pfr.git
            GIT_TAG 2.2.0
            SOURCE_DIR ${STORM_3RDPARTY_BINARY_DIR}/boost_pfr
            PATCH_COMMAND ""
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
            LOG_INSTALL ON
    )
    add_library(boost_pfr INTERFACE) # Not imported, we are in control of the sources.
    add_dependencies(boost_pfr boost_pfr_src)
    target_include_directories(boost_pfr INTERFACE
            $<BUILD_INTERFACE:${STORM_3RDPARTY_BINARY_DIR}/boost_pfr/include>
            $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}>
    )
    install(TARGETS boost_pfr EXPORT storm_Targets)
    install(DIRECTORY ${STORM_3RDPARTY_BINARY_DIR}/boost_pfr/include/boost
            DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
            PATTERN ".git" EXCLUDE)
    add_dependencies(storm_resources boost_pfr)
    list(APPEND STORM_DEP_TARGETS boost_pfr)
endif()

message(STATUS "Storm - Using boost ${Boost_VERSION} (library version ${Boost_LIB_VERSION}).")
list(APPEND STORM_DEP_IMP_TARGETS Boost::boost)

#############################################################
##
##	ExprTk
##
#############################################################

# Use the shipped version of ExprTK
message(STATUS "Storm - Including ExprTk.")
add_library(exprTk INTERFACE) # Not imported, we are in control of the sources.
target_include_directories(exprTk INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_SOURCE_DIR}/exprtk>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/exprtk>
)
install(TARGETS exprTk EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_SOURCE_DIR}/exprtk
        DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN ".git" EXCLUDE)
list(APPEND STORM_DEP_TARGETS exprTk)

#############################################################
##
##	Parallel Hashmap
##
#############################################################

# Use the shipped version of Parallel Hashmap
message(STATUS "Storm - Including Parallel Hashmap.")
add_library(phmap INTERFACE) # Not imported, we are in control of the sources.
# Note that include directory is parallel-hashmap/parallel_hashmap/
target_include_directories(phmap INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_SOURCE_DIR}/parallel-hashmap>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/parallel_hashmap>)
install(TARGETS phmap EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_SOURCE_DIR}/parallel-hashmap/parallel_hashmap
        DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.h" PATTERN ".git" EXCLUDE)
list(APPEND STORM_DEP_TARGETS phmap)

#############################################################
##
##	cpphoafparser
##
#############################################################

# Use the shipped version of cpphoafparser
message(STATUS "Storm - Including cpphoafparser")
add_library(cppHOAparser INTERFACE) # Not imported, we are in control of the sources.
target_include_directories(cppHOAparser INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_SOURCE_DIR}/cpphoafparser/include>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}>)
install(TARGETS cppHOAparser EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_SOURCE_DIR}/cpphoafparser/include/cpphoafparser
        DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.h" PATTERN ".git" EXCLUDE)
list(APPEND STORM_DEP_TARGETS cppHOAparser)

#############################################################
##
##	ModernJSON
##
#############################################################

#use the shipped version of modernjson
message(STATUS "Storm - Including ModernJSON.")
add_library(ModernJSON INTERFACE) # Not imported, we are in control of the sources.
target_include_directories(ModernJSON INTERFACE
        $<BUILD_INTERFACE:${STORM_3RDPARTY_SOURCE_DIR}/modernjson/include>
        $<INSTALL_INTERFACE:${STORM_RESOURCE_INCLUDE_INSTALL_DIR}>)
install(TARGETS ModernJSON EXPORT storm_Targets)
install(DIRECTORY ${STORM_3RDPARTY_SOURCE_DIR}/modernjson/include/nlohmann
        DESTINATION ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN ".git" EXCLUDE)
list(APPEND STORM_DEP_TARGETS ModernJSON)

#############################################################
##
##	Z3 (optional)
##
#############################################################

if(NOT STORM_DISABLE_Z3)
    find_package(Z3 QUIET 4.8.12 NO_MODULE)
    # Notice that such old z3 versions do not provide config files, so we could bump this version ahead.
    if(TARGET z3::libz3)
        set(STORM_HAVE_Z3 ON)
        set(STORM_Z3_CONFIG ON) # For the config file, did we include z3 via a config (yes)

        set(Z3_VERSION "${Z3_VERSION_MAJOR}.${Z3_VERSION_MINOR}.${Z3_VERSION_PATCH}")

        if("${Z3_VERSION}" VERSION_EQUAL "4.10.1")
            message(WARNING "Storm cannot be linked against Z3 version 4.10.1, see issue 252.")
            message(WARNING "Storm - Could not link with z3. Building of Prism/JANI models will not be supported.")
            set(Z3_FOUND FALSE)
        else()
            message(STATUS "Storm - Linking with Z3 (version ${Z3_VERSION}).")
            list(APPEND STORM_DEP_IMP_TARGETS z3::libz3)
        endif()
    else()
        # Debian and Ubuntu packages are as of Debian 12 and Ubuntu 24.04 very old,
        # such that no Cmake config files are provided. For the time being, we use a FindZ3.cmake script.
        find_package(Z3 QUIET 4.8.12)

        if(Z3_FOUND)

            set(STORM_HAVE_Z3 ON)
            set(STORM_Z3_CONFIG OFF) # For the config file, did we include z3 via a config (no)

            if("${Z3_VERSION}" VERSION_EQUAL "4.10.1")
                message(WARNING "Storm cannot be linked against Z3 version 4.10.1, see issue 252.")
                message(WARNING "Storm - Could not link with z3. Building of Prism/JANI models will not be supported.")
                set(Z3_FOUND FALSE)
            else()
                message(STATUS "Storm - Linking with Z3 (version ${Z3_VERSION}).")

                add_library(z3 SHARED IMPORTED)
                set_target_properties(
                        z3
                        PROPERTIES
                        IMPORTED_LOCATION ${Z3_LIBRARIES}
                        INTERFACE_INCLUDE_DIRECTORIES ${Z3_INCLUDE_DIR}
                )
                list(APPEND STORM_DEP_IMP_TARGETS z3)
            endif()
        else()
            message(WARNING "Storm - Z3 not found. Building of Prism/JANI models will not be supported.")
        endif()
    endif()
else()
    message(WARNING "Storm - Not linking with Z3. Building of Prism/JANI models will not be supported.")
endif()


#############################################################
##
##	GLPK (optional)
##
#############################################################

if(NOT STORM_DISABLE_GLPK)
    set(STORM_HAVE_GLPK OFF)
    find_package(GLPK QUIET)

    if(GLPK_FOUND)
        set(STORM_HAVE_GLPK ON)
        message(STATUS "Storm - Linking with GLPK ${GLPK_VERSION_STRING}")

        add_library(glpk SHARED IMPORTED)
        set_target_properties(
                glpk
                PROPERTIES
                IMPORTED_LOCATION ${GLPK_LIBRARIES}
                INTERFACE_INCLUDE_DIRECTORIES ${GLPK_INCLUDE_DIR}
        )
        list(APPEND STORM_DEP_IMP_TARGETS glpk)
    else()
        message(WARNING "Storm - Not linking with GLPK.")
    endif()
else()
    message(STATUS "Storm - Not linking with GLPK.")
    set(STORM_HAVE_GLPK OFF)
endif()


#############################################################
##
##	Gurobi (optional)
##
#############################################################

if(NOT STORM_DISABLE_GUROBI)
    find_package(GUROBI QUIET)
    if(GUROBI_FOUND)
        set(STORM_HAVE_GUROBI ${GUROBI_FOUND})
        if(EXISTS ${GUROBI_LIBRARY})
            message(STATUS "Storm - Linking with Gurobi (${GUROBI_CXX_LIBRARY}).")
            add_library(GUROBI UNKNOWN IMPORTED)
            set_target_properties(
                    GUROBI
                    PROPERTIES
                    IMPORTED_LOCATION ${GUROBI_LIBRARY}
                    INTERFACE_INCLUDE_DIRECTORIES ${GUROBI_INCLUDE_DIRS}
            )
            list(APPEND STORM_DEP_IMP_TARGETS GUROBI)
        else()
            # The FindGurobi.cmake script needs to be updated every now and then as the library file contains the version number...
            message(FATAL_ERROR "Gurobi Library ${GUROBI_LIBRARY} not found. If your Gurobi Version is higher then 13.0.0, please contact the Storm developers.")
        endif()
    else()
        message(STATUS "Storm - Not linking with Gurobi.")
        set(STORM_HAVE_GUROBI OFF)
    endif()
else()
    message(STATUS "Storm - Not linking with Gurobi.")
    set(STORM_HAVE_GUROBI OFF)
endif()

#############################################################
##
##	CUDD (optional)
##
#############################################################

if(NOT STORM_DISABLE_CUDD)
    include(${STORM_3RDPARTY_SOURCE_DIR}/include_cudd.cmake)
    set(STORM_HAVE_CUDD ON)
else()
    message(STATUS "Storm - Not linking with CUDD.")
    set(STORM_HAVE_CUDD OFF)
endif()

#############################################################
##
##      carl (including cln and ginac)
##
#############################################################

# Set CLN/GiNaC configuration
if(STORM_USE_CLN_EA OR STORM_USE_CLN_RF)
    set(SHIPPED_CARL_USE_CLN_NUMBERS ON)
    set(SHIPPED_CARL_USE_GINAC ON)
else()
    set(SHIPPED_CARL_USE_CLN_NUMBERS OFF)
    set(SHIPPED_CARL_USE_GINAC OFF)
endif()

# Fetch carl
FETCHCONTENT_DECLARE(
    carl
    GIT_REPOSITORY ${STORM_CARL_GIT_REPO}
    GIT_TAG ${STORM_CARL_GIT_TAG}
)

# Configure carl
SET(CARL_EXCLUDE_TESTS_FROM_ALL ON)  #carl without tests
SET(CARL_THREAD_SAFE ON) #carl thread safe code
SET(CARL_USE_CLN_NUMBERS ${SHIPPED_CARL_USE_CLN_NUMBERS})
SET(CARL_USE_GINAC ${SHIPPED_CARL_USE_GINAC})
SET(CARL_EIGEN_GIT_TAG ${STORM_EIGEN_GIT_TAG})
SET(CARL_PORTABLE ${STORM_PORTABLE})
SET(CARL_EXPORT_TO_CMAKE OFF)

# Override carl's install dirs to match system conventions
SET(CARL_LIB_INSTALL_DIR ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
SET(CARL_INCLUDE_INSTALL_DIR ${STORM_RESOURCE_INCLUDE_INSTALL_DIR})
SET(CARL_CMAKE_INSTALL_DIR ${STORM_CMAKE_INSTALL_DIR}/resources/carl)

FETCHCONTENT_MAKEAVAILABLE(carl)
include(${carl_BINARY_DIR}/carlConfig.cmake)

# Read variables from this config.
set(STORM_HAVE_CLN ${CARL_HAVE_CLN})
set(STORM_HAVE_GINAC ${CARL_HAVE_GINAC})
message(STATUS "Storm - Using carl version ${carl_VERSION} from ${carl_SOURCE_DIR} (CLN: ${STORM_HAVE_CLN}, GiNaC: ${STORM_HAVE_GINAC})")

add_dependencies(storm_resources lib_carl)
list(APPEND STORM_FETCHED_TARGETS lib_carl)

# Some checks to see whether we correctly configured carl.
if(SHIPPED_CARL_USE_CLN_NUMBERS AND NOT STORM_HAVE_CLN)
    message(FATAL_ERROR "Cannot use CLN numbers. Consider installing {cln, ginac} or run cmake with STORM_USE_CLN_EA=OFF and STORM_USE_CLN_RF=OFF. Please also consult the documentation regarding dependencies for more information about prerequisites. ")
endif()
if(SHIPPED_CARL_USE_GINAC AND NOT STORM_HAVE_GINAC)
    message(FATAL_ERROR "Cannot use CLN numbers without ginac. Consider installing ginac  or run cmake with STORM_USE_CLN_EA=OFF and STORM_USE_CLN_RF=OFF. Please also consult the documentation regarding dependencies for more information about prerequisites.")
endif()

#############################################################
##
##	MathSAT (optional)
##
#############################################################

if(NOT STORM_DISABLE_MATHSAT)
    find_package(MATHSAT QUIET)
    if(MATHSAT_FOUND)
        set(STORM_HAVE_MATHSAT ON)
        add_library(mathsat SHARED IMPORTED)
        set_target_properties(
                mathsat
                PROPERTIES
                IMPORTED_LOCATION ${MATHSAT_LIBRARIES}
                INTERFACE_INCLUDE_DIRECTORIES ${MATHSAT_INCLUDE_DIRS}
        )
        list(APPEND STORM_DEP_IMP_TARGETS mathsat)
        message(STATUS "Storm - Linking with MathSAT: (library: ${mathsat_}; include: ${MATHSAT_INCLUDE_DIRS})")
    else()
        set(STORM_HAVE_MATHSAT OFF)
        if(MATHSAT_ROOT STREQUAL "")
            message(STATUS "Storm - MathSAT not found.")
        else()
            message(WARNING "Storm - MathSAT not found, even though MATHSAT_ROOT is set.")
        endif()
    endif()
else()
    set(STORM_HAVE_MATHSAT OFF)
    message(STATUS "Storm - Not linking with MathSat.")
endif()



#############################################################
##
##	QVBS (Quantitative verification benchmark set)
##
##  TODO: Consider installing these files as well?
#############################################################

set(STORM_HAVE_QVBS OFF)
if(STORM_LOAD_QVBS)
    ExternalProject_Add(
            download_qvbs
            GIT_REPOSITORY https://github.com/ahartmanns/qcomp.git
            GIT_SHALLOW 1
            PREFIX ${PROJECT_BINARY_DIR}/qvbs
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_INSTALL ON
    )
    add_dependencies(storm_resources download_qvbs)
    set(STORM_HAVE_QVBS ON)
    ExternalProject_Get_Property(download_qvbs source_dir)
    set(STORM_QVBS_ROOT "${source_dir}/benchmarks")
elseif(NOT "${STORM_QVBS_ROOT}" STREQUAL "")
    if( EXISTS "${STORM_QVBS_ROOT}/index.json")
        set(STORM_HAVE_QVBS ON)
    else()
        message(FATAL_ERROR "No file 'index.json' in provided QVBS root directory ${STORM_QVBS_ROOT}")
    endif()
endif()
if(STORM_HAVE_QVBS)
    message(STATUS "Storm - Enabled inputs from QVBS located at ${STORM_QVBS_ROOT}")
endif()


#############################################################
##
##	Xerces (optional)
##
#############################################################

if(NOT STORM_DISABLE_XERCES)
    find_package(XercesC QUIET)
    if(TARGET XercesC::XercesC)
        set(STORM_HAVE_XERCES ON)
        message(STATUS "Storm (GSPN) - Linking with Xerces-c ${XercesC_VERSION}")
        list(APPEND STORM_GSPN_LINK_LIBRARIES XercesC::XercesC)
    else()
        set(STORM_HAVE_XERCES OFF)
        message(STATUS "Storm - Could not find Xerces, will disable parsing XML formats (for GSPNs)")
    endif()
else()
    set(STORM_HAVE_XERCES OFF)
    message(STATUS "Storm - Building without Xerces disables parsing XML formats (for GSPNs)")
endif()

#############################################################
##
##	SoPlex (optional)
##
#############################################################

if(NOT STORM_DISABLE_SOPLEX)
    find_package(soplex QUIET)

    IF(TARGET libsoplex-pic)
        set(STORM_HAVE_SOPLEX ON)
        get_target_property(soplexLOC libsoplex-pic LOCATION)
        get_target_property(soplexINCLUDE libsoplex-pic INTERFACE_INCLUDE_DIRECTORIES)
        MESSAGE(STATUS "Storm - Linking with SoPlex version ${soplex_VERSION}: (libary: ${soplexLOC}; include: ${soplexINCLUDE})")
        list(APPEND STORM_DEP_IMP_TARGETS libsoplex-pic)
    ELSE()
        set(STORM_HAVE_SOPLEX OFF)
        MESSAGE(STATUS "Storm - Not linking with SoPlex.")
    ENDIF()
else()
    set(STORM_HAVE_SOPLEX OFF)
    MESSAGE(STATUS "Storm - Not linking with SoPlex.")
endif()


#############################################################
##
##	Spot (optional)
##
#############################################################

include(${STORM_3RDPARTY_SOURCE_DIR}/include_spot.cmake)

#############################################################
##
##	Sylvan (optional)
##
#############################################################

if(NOT STORM_DISABLE_SYLVAN)
    FETCHCONTENT_DECLARE(
            sylvanfetch
            SOURCE_DIR ${STORM_3RDPARTY_SOURCE_DIR}/sylvan
    )
    SET(SYLVAN_GMP ON)
    SET(USE_CARL ON)
    set(SYLVAN_PORTABLE ${STORM_PORTABLE})
    SET(SYLVAN_INSTALL_BINDIR ${STORM_RESOURCE_BIN_INSTALL_DIR})
    SET(SYLVAN_INSTALL_LIBDIR ${STORM_RESOURCE_LIBRARY_INSTALL_DIR})
    SET(SYLVAN_INSTALL_INCLUDEDIR ${STORM_RESOURCE_INCLUDE_INSTALL_DIR}/sylvan)
    SET(SYLVAN_CMAKE_INSTALL_DIR ${STORM_CMAKE_INSTALL_DIR}/resources/sylvan)
    SET(CMAKE_POSITION_INDEPENDENT_CODE ON) # Likely only necessary on linux
    SET(STORM_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/") # TODO this is ugly.
    SET(STORM_BINARY_INCLUDE_DIR "${PROJECT_BINARY_DIR}/include/") #TODO this is ugly.
    FETCHCONTENT_MAKEAVAILABLE(sylvanfetch)
    if(STORM_WARNING_AS_ERROR)
        # Ignore explicit warning in storm_wrapper.cpp
        target_compile_options(sylvan PRIVATE -Wno-cpp)
    endif()
    add_dependencies(sylvan lib_carl)
    add_dependencies(storm_resources sylvan)
    MESSAGE(STATUS "Storm - Linking with Sylvan")
    list(APPEND STORM_FETCHED_TARGETS sylvan)
    set(STORM_HAVE_SYLVAN ON)
else()
    message(STATUS "Storm - Not linking with Sylvan.")
    set(STORM_HAVE_SYLVAN OFF)
endif()

#############################################################
##
##	Google Test gtest
##
#############################################################

if(STORM_BUILD_TESTS)
    # Use commit with workaround for char conversion warning, being recognized by Clang 21+.
    # See https://github.com/google/googletest/issues/4762
    set(GTEST_VERSION "fa8438ae6b70c57010177de47a9f13d7041a6328")
    #set(GTEST_VERSION "1.17.0")
    FETCHCONTENT_DECLARE(
            googletest
            URL https://github.com/google/googletest/archive/${GTEST_VERSION}.zip
            #URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.zip
    )
    # Tests are not installed
    set(INSTALL_GTEST OFF)
    FETCHCONTENT_MAKEAVAILABLE(googletest)

    set(GTEST_LIBRARIES GTest::gtest_main GTest::gmock)
    add_dependencies(test-resources ${GTEST_LIBRARIES})
    list(APPEND STORM_TEST_LINK_LIBRARIES ${GTEST_LIBRARIES})
endif()

#############################################################
##
##	Threads
##
#############################################################

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
list(APPEND STORM_DEP_IMP_TARGETS Threads::Threads)
