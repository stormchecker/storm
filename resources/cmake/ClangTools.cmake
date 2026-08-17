# Developer-tooling targets for clang-format and clang-tidy.
# Included from the top-level CMakeLists.txt (only when PROJECT_IS_TOP_LEVEL).
# The STORM_ENABLE_CLANG_FORMAT / STORM_ENABLE_CLANG_TIDY options are defined there.

if(STORM_ENABLE_CLANG_FORMAT)
	# Runs clang-format (as configured in .clang-format) over the Storm sources.
	# Delegates to auto-format.sh, which picks the clang-format version pinned in
	# resources/scripts/clang-versions and honors .clang-format-ignore.
	add_custom_target(format
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/auto-format.sh
		USES_TERMINAL
		COMMENT "Running clang-format over the Storm sources.")
	# Like format, but only checks the sources without modifying them.
	add_custom_target(format-check
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/auto-format.sh --check
		USES_TERMINAL
		COMMENT "Checking the code formatting.")
endif()

if(STORM_ENABLE_CLANG_TIDY)
	# Ensure that a compile_commands.json is generated, since clang-tidy relies on it.
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	# Bake the compiler's implicit include directories into the recorded compile commands. Fixes problems with e.g. Nix.
	set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
	set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
	# Runs clang-tidy (as configured in .clang-tidy) over the Storm sources.
	# Delegates to clang-tidy.sh, which picks the run-clang-tidy version pinned in
	# resources/scripts/clang-versions. USES_TERMINAL keeps the progress of clang-tidy visible.
	add_custom_target(clang-tidy
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/clang-tidy.sh -p ${CMAKE_BINARY_DIR} -j ${STORM_RESOURCES_BUILD_JOBCOUNT}
			-source-filter ${CMAKE_SOURCE_DIR}/src --source-dir ${CMAKE_SOURCE_DIR}
		USES_TERMINAL
		COMMENT "Running clang-tidy over the Storm sources.")
	# Like clang-tidy, but automatically applies the suggested fixes.
	add_custom_target(clang-tidy-fix
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/clang-tidy.sh -p ${CMAKE_BINARY_DIR} -j ${STORM_RESOURCES_BUILD_JOBCOUNT}
			-source-filter ${CMAKE_SOURCE_DIR}/src --source-dir ${CMAKE_SOURCE_DIR} --fix
		USES_TERMINAL
		COMMENT "Running clang-tidy with automatic fixes over the Storm sources.")
	# Like clang-tidy, but only checks the files changed in git (staged, unstaged and untracked).
	add_custom_target(clang-tidy-changed
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/clang-tidy.sh -p ${CMAKE_BINARY_DIR} -j ${STORM_RESOURCES_BUILD_JOBCOUNT}
			-source-filter ${CMAKE_SOURCE_DIR}/src --source-dir ${CMAKE_SOURCE_DIR} --git-ref=HEAD
		USES_TERMINAL
		COMMENT "Running clang-tidy over the files changed in git.")
	# Like clang-tidy-changed, but automatically applies the suggested fixes.
	add_custom_target(clang-tidy-changed-fix
		COMMAND ${CMAKE_SOURCE_DIR}/resources/scripts/clang-tidy.sh -p ${CMAKE_BINARY_DIR} -j ${STORM_RESOURCES_BUILD_JOBCOUNT}
			-source-filter ${CMAKE_SOURCE_DIR}/src --source-dir ${CMAKE_SOURCE_DIR} --git-ref=HEAD --fix
		USES_TERMINAL
		COMMENT "Running clang-tidy with automatic fixes over the files changed in git.")
endif()
