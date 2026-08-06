# Base path for test files
set(STORM_TESTS_BASE_PATH "${PROJECT_SOURCE_DIR}/src/test/storm-pars")

# Note that the tests also need the source files, except for the main file
include_directories(${GTEST_INCLUDE_DIR})

foreach (testsuite derivative modelchecker transformer utility)

	file(GLOB_RECURSE TEST_${testsuite}_FILES ${STORM_TESTS_BASE_PATH}/${testsuite}/*.h ${STORM_TESTS_BASE_PATH}/${testsuite}/*.cpp ${STORM_TESTS_BASE_PATH}/../storm_gtest.cpp)
	add_executable (test-pars-${testsuite} ${TEST_${testsuite}_FILES} ${STORM_TESTS_BASE_PATH}/storm-test.cpp)
	target_link_libraries(test-pars-${testsuite} storm-pars storm-parsers)
	target_link_libraries(test-pars-${testsuite} ${STORM_TEST_LINK_LIBRARIES})
	target_include_directories(test-pars-${testsuite} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	storm_target_precompile_headers(test-pars-${testsuite} REUSE_FROM test-builder)

	add_dependencies(test-pars-${testsuite} test-resources)
	add_test(NAME run-test-pars-${testsuite} COMMAND $<TARGET_FILE:test-pars-${testsuite}>)
	add_dependencies(tests test-pars-${testsuite})

endforeach ()
