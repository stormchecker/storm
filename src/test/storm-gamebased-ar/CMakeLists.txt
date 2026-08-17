# Base path for test files
set(STORM_TESTS_BASE_PATH "${PROJECT_SOURCE_DIR}/src/test/storm-gamebased-ar")

# Note that the tests also need the source files, except for the main file
include_directories(${GTEST_INCLUDE_DIR})

foreach (testsuite abstraction modelchecker)

	file(GLOB_RECURSE TEST_${testsuite}_FILES ${STORM_TESTS_BASE_PATH}/${testsuite}/*.h ${STORM_TESTS_BASE_PATH}/${testsuite}/*.cpp ${STORM_TESTS_BASE_PATH}/../storm_gtest.cpp)
	add_executable(test-gamebased-ar-${testsuite} ${TEST_${testsuite}_FILES} ${STORM_TESTS_BASE_PATH}/storm-test.cpp)
	target_link_libraries(test-gamebased-ar-${testsuite} storm-gamebased-ar storm-parsers)
	target_link_libraries(test-gamebased-ar-${testsuite} ${STORM_TEST_LINK_LIBRARIES})
	target_include_directories(test-gamebased-ar-${testsuite} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	storm_target_precompile_headers(test-gamebased-ar-${testsuite} REUSE_FROM test-builder)

	add_dependencies(test-gamebased-ar-${testsuite} test-resources)
	add_test(NAME run-test-gamebased-ar-${testsuite} COMMAND $<TARGET_FILE:test-gamebased-ar-${testsuite}>)
	add_dependencies(tests test-gamebased-ar-${testsuite})

endforeach ()
