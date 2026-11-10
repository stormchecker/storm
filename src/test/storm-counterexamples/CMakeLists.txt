# Base path for test files
set(STORM_TESTS_BASE_PATH "${PROJECT_SOURCE_DIR}/src/test/storm-counterexamples")

# Note that the tests also need the source files, except for the main file
include_directories(${GTEST_INCLUDE_DIR})

foreach (testsuite counterexamples)

	file(GLOB_RECURSE TEST_${testsuite}_FILES ${STORM_TESTS_BASE_PATH}/${testsuite}/*.h ${STORM_TESTS_BASE_PATH}/${testsuite}/*.cpp)
	add_executable(test-counterexamples-${testsuite} ${TEST_${testsuite}_FILES} ${STORM_TESTS_BASE_PATH}/storm-test.cpp ${STORM_TESTS_BASE_PATH}/../storm_gtest.cpp)
	target_link_libraries(test-counterexamples-${testsuite} storm-counterexamples storm-parsers)
	target_link_libraries(test-counterexamples-${testsuite} ${STORM_TEST_LINK_LIBRARIES})
	target_include_directories(test-counterexamples-${testsuite} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	storm_target_precompile_headers(test-counterexamples-${testsuite} REUSE_FROM test-builder)

	add_dependencies(test-counterexamples-${testsuite} test-resources)
	add_test(NAME run-test-counterexamples-${testsuite} COMMAND $<TARGET_FILE:test-counterexamples-${testsuite}>)
	add_dependencies(tests test-counterexamples-${testsuite})

endforeach ()
