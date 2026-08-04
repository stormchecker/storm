#############################################################
##
##	Compiler settings for storm
##
#############################################################
if (STORM_FORCE_POPCNT)
    add_compile_options(-mpopcnt)
endif()

if (STORM_USE_THIN_LTO)
    if (STORM_COMPILER_CLANG OR STORM_COMPILER_APPLECLANG)
        add_link_options(-flto=thin)
        message(STATUS "Storm - Enabling link-time optimizations using ThinLTO.")
    else()
        message(FATAL_ERROR "Storm - ThinLTO only supported for Clang. Use regular LTO instead.")
    endif()
elseif (STORM_USE_LTO)
    add_link_options(-flto)

    # Fix for problems that occurred when using LTO on gcc. This should be removed when it
    # is not needed anymore as it makes the already long link-step potentially longer.
    if (STORM_COMPILER_GCC)
        add_link_options(-flto-partition=none)
    endif()

    message(STATUS "Storm - Enabling link-time optimizations.")
else()
    message(STATUS "Storm - Disabling link-time optimizations.")
endif()

if (STORM_COMPILE_WITH_ADDRESS_SANITIZER)
    message(STATUS "Storm - Enabling AddressSanitizer")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# Allow for disabling pre compiled headers
function(storm_target_precompile_headers target)
	if(NOT CMAKE_DISABLE_PRECOMPILE_HEADERS)
		target_precompile_headers(${target} ${ARGN})
	endif()
endfunction()

# In release mode, we turn on even more optimizations if we do not have to provide a portable binary.
if (NOT STORM_PORTABLE AND (NOT APPLE_SILICON OR (STORM_COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)))
    add_compile_options($<$<CONFIG:RELEASE>:-march=native>)
endif()

if (STORM_COMPILE_WITH_COMPILATION_PROFILING)
    if (CLANG)
        # Allow profiling of compilation times,
        # outputs json with chromium-tracing information along every object file (open in chrome://tracing/).
        # overhead is very limited
        add_compile_options("-ftime-trace")
    endif()
endif()

if (STORM_DEVELOPER)
    add_compile_options("-pedantic")
    if (STORM_ALLWARNINGS)
        # With gcc, we already set (almost) all warnings.
        # With Clang, we can even go further using Weverything, which is not always helpful, which is why these options are guarded by  STORM_ALL_WARNINGS
        if (CLANG)
            # Enable strictly every warning and then disable selected ones.
            add_compile_options(-Weverything)
            add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic)
            add_compile_options(-Wno-old-style-cast)
            add_compile_options(-Wno-reserved-id-macro)
            add_compile_options(-Wno-newline-eof)
            add_compile_options(-Wno-documentation)
            add_compile_options(-Wno-weak-vtables)
            add_compile_options(-Wno-global-constructors)
            add_compile_options(-Wno-exit-time-destructors)
            add_compile_options(-Wno-switch-enum)
            add_compile_options(-Wno-covered-switch-default)
            add_compile_options(-Wno-padded)
            add_compile_options(-Wno-float-equal)
            add_compile_options(-Wno-unused-local-typedef)
            add_compile_options(-Wno-missing-variable-declarations)
            add_compile_options(-Wno-undefined-func-template)
            add_compile_options(-Wno-double-promotion)

            # Reenable soon
            add_compile_options(-Wno-non-virtual-dtor)
            add_compile_options(-Wno-conditional-uninitialized)
            add_compile_options(-Wno-float-conversion)
            add_compile_options(-Wno-unreachable-code-break)
            add_compile_options(-Wno-ctad-maybe-unsupported)
            add_compile_options(-Wno-unused-template)

            # Requires adapter
            add_compile_options(-Wno-comma)
            add_compile_options(-Wno-used-but-marked-unused)

            # Requires adapter for gmock
            add_compile_options(-Wno-undef)


            # Conflicts with warnings in gcc
            add_compile_options(-Wno-unreachable-code-return)

            # ?? unclear semantics
            add_compile_options(-Wno-ctad-maybe-unsupported)

            # Potentially useful, but just too many right now
            add_compile_options(-Wno-unused-exception-parameter)
            add_compile_options(-Wno-zero-as-null-pointer-constant)
            add_compile_options(-Wno-shadow-field-in-constructor)
            add_compile_options(-Wno-inconsistent-missing-destructor-override)
            add_compile_options(-Wno-suggest-destructor-override)
            add_compile_options(-Wno-deprecated-dynamic-exception-spec)
            add_compile_options(-Wno-extra-semi)
            add_compile_options(-Wno-shorten-64-to-32)
            add_compile_options(-Wno-sign-conversion)
            add_compile_options(-Wno-suggest-override)
            add_compile_options(-Wno-deprecated-copy-with-dtor)
            add_compile_options(-Wno-shadow-field)
            add_compile_options(-Wno-shadow)
            add_compile_options(-Wno-documentation-unknown-command)
            add_compile_options(-Wno-missing-noreturn)
            add_compile_options(-Wno-missing-prototypes)
        endif ()
    else ()
        add_compile_options(-Wall -Wextra)
        add_compile_options(-Wno-unknown-pragmas)
        add_compile_options(-Wno-unused-local-typedefs)
    endif ()
else()
    add_compile_options($<$<CONFIG:RELEASE>:-fomit-frame-pointer>)
endif()

if(STORM_WARNING_AS_ERROR)
    # Reenable soon
    add_compile_options(-Wno-unused-parameter)
endif()


add_custom_target(binaries)


add_subdirectory(storm)
add_subdirectory(storm-counterexamples)
add_subdirectory(storm-permissive)
add_subdirectory(storm-gamebased-ar)
add_subdirectory(storm-parsers)
add_subdirectory(storm-version-info)
add_subdirectory(storm-conv)
add_subdirectory(storm-dft)
add_subdirectory(storm-gspn)
add_subdirectory(storm-pars)
add_subdirectory(storm-pomdp)
# We also build the CLI utilities, as other libraries may be built upon them.
add_subdirectory(storm-cli-utilities)

if(STORM_BUILD_EXECUTABLES)
    message(STATUS "Storm - Enabled building executables." )
    # Typically an idea to switch this off, if you want to build a library on top of Storm.
    add_subdirectory(storm-cli)
    add_subdirectory(storm-conv-cli)
    add_subdirectory(storm-dft-cli)
    add_subdirectory(storm-gspn-cli)
    add_subdirectory(storm-pars-cli)
    add_subdirectory(storm-pomdp-cli)
else()
    message(STATUS "Storm - Disabled building executables." )
endif()

if (PROJECT_IS_TOP_LEVEL AND STORM_BUILD_TESTS)
    message(STATUS "Storm - Enabled building tests." )
    add_subdirectory(test)
else()
    message(STATUS "Storm - Disabled building tests." )
endif()

set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)
