file(GLOB_RECURSE STORM_PARSER_SOURCES ${PROJECT_SOURCE_DIR}/src/storm-parsers/*/*.cpp)
file(GLOB_RECURSE STORM_PARSER_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/src/storm-parsers" ${PROJECT_SOURCE_DIR}/src/storm-parsers/*.h ${PROJECT_SOURCE_DIR}/src/storm-parsers/*/*.h)

# The heaviest parser translation units (esp. the boost::spirit grammars) compile close to the memory limit already in
# parallel builds. Disable the Debug compiler flags for them to lessen memory consumption during compilation; the
# generated grammars are hard to debug anyway.
SET_SOURCE_FILES_PROPERTIES(
        ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParser.cpp
        ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParserGrammar.cpp
        ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParserGrammar.cpp
        ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParser.cpp
        ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ExpressionParser.cpp
        PROPERTIES COMPILE_FLAGS -g0)

# On macOS x86_64 (Intel), a C++ exception unwinding within the boost::spirit parsers can crash the unwinder itself.
# -fno-omit-frame-pointer preserves the frame-chain information the unwinder needs.
# Is paired with the -Wl,-keep_dwarf_unwind link flag below.
if (MACOSX AND NOT APPLE_SILICON)
    set_source_files_properties(
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParser.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParserGrammar.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParserGrammar.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParser.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ExpressionParser.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ExpressionCreator.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ImcaMarkovAutomatonParserGrammar.cpp
            ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ImcaMarkovAutomatonParser.cpp
            PROPERTIES COMPILE_OPTIONS -fno-omit-frame-pointer)
endif()

# With STORM_COMPILE_WITH_COMPILATION_PROFILING, src/CMakeLists.txt adds -ftime-trace to the whole build. The profiler
# emits one trace event per template instantiation, which roughly doubles the compile time of the boost::spirit grammars
# above. Since clang offers no way to negate -ftime-trace per source file, drop it from this directory's scope and
# re-add it to the remaining translation units of this library, which do not suffer from this overhead.
if (STORM_COMPILE_WITH_COMPILATION_PROFILING)
    if (CLANG)
        get_property(_storm_parsers_compile_options DIRECTORY PROPERTY COMPILE_OPTIONS)
        list(REMOVE_ITEM _storm_parsers_compile_options "-ftime-trace")
        set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${_storm_parsers_compile_options}")
        set(STORM_PARSERS_SOURCES_WITHOUT_PROFILING
                ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParser.cpp
                ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/PrismParserGrammar.cpp
                ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParserGrammar.cpp
                ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/FormulaParser.cpp
                ${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/ExpressionParser.cpp)
        foreach(_storm_parser_source ${STORM_PARSER_SOURCES})
            if(NOT _storm_parser_source IN_LIST STORM_PARSERS_SOURCES_WITHOUT_PROFILING)
                set_source_files_properties(${_storm_parser_source} PROPERTIES COMPILE_OPTIONS -ftime-trace)
            endif()
        endforeach()
    endif()
endif()

# Create storm-parsers.
add_library(storm-parsers SHARED)
target_sources(storm-parsers
        PRIVATE
        ${STORM_PARSER_SOURCES}
        PUBLIC
        FILE_SET fs_storm_parsers_headers TYPE HEADERS BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES ${STORM_PARSER_HEADERS})
# Put the boost::spirit headers into the precompiled header: they are large and are included (transitively) by most
# translation units of this library, dominating the header-parsing time. SpiritParserDefinitions.h must be used (instead
# of the raw spirit headers), as it defines the required macros (BOOST_SPIRIT_UNICODE, BOOST_SPIRIT_USE_PHOENIX_V3)
# before including the spirit headers.
storm_target_precompile_headers(storm-parsers PRIVATE ${STORM_PRECOMPILED_HEADERS}
        "${PROJECT_SOURCE_DIR}/src/storm-parsers/parser/SpiritParserDefinitions.h")
set_target_properties(storm-parsers PROPERTIES VERSION ${STORM_VERSION} SOVERSION ${STORM_VERSION})
set_target_properties(storm-parsers PROPERTIES DEFINE_SYMBOL "")  # to avoid problems with pch on linux.
# Only symbols marked STORM_PARSERS_API (see storm-parsers-api.h) are exported.
set_target_properties(storm-parsers PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
# Drop code not reachable from the exported symbols.
if(APPLE)
    # ld64's -dead_strip works on clang's default .subsections_via_symbols.
    target_link_options(storm-parsers PRIVATE "LINKER:-dead_strip")
elseif(UNIX AND NOT APPLE)
    # --gc-sections needs each function/data object in its own section.
    target_compile_options(storm-parsers PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(storm-parsers PRIVATE "LINKER:--gc-sections")
endif()
# Strip symbols to keep the binary small; disable STORM_STRIP_PARSER_BINARY for debugging. On Linux -S drops DWARF sections.
if(STORM_STRIP_PARSER_BINARY)
    if(APPLE)
        add_custom_command(TARGET storm-parsers POST_BUILD COMMAND ${CMAKE_STRIP} -x "$<TARGET_FILE:storm-parsers>" COMMENT "Stripping storm-parsers")
    elseif(UNIX)
        add_custom_command(TARGET storm-parsers POST_BUILD COMMAND ${CMAKE_STRIP} -S "$<TARGET_FILE:storm-parsers>" COMMENT "Stripping storm-parsers")
    endif()
endif()
target_link_libraries(storm-parsers PUBLIC storm)
if (MACOSX AND NOT APPLE_SILICON)
    # Linker flags for the unwinding issue on macOS Intel. See the matching -fno-omit-frame-pointer comment above.
    target_link_options(storm-parsers PRIVATE -Wl,-keep_dwarf_unwind)
endif()

list(APPEND STORM_TARGETS storm-parsers)
set(STORM_TARGETS ${STORM_TARGETS} PARENT_SCOPE)

# installation
install(TARGETS storm-parsers EXPORT storm_Targets
        RUNTIME DESTINATION ${STORM_BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${STORM_LIB_INSTALL_DIR}
        FRAMEWORK DESTINATION  ${STORM_LIB_INSTALL_DIR}
        FILE_SET fs_storm_parsers_headers DESTINATION ${STORM_INCLUDE_INSTALL_DIR})
