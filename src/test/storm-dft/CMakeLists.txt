# Base path for test files
set(STORM_TESTS_BASE_PATH "${PROJECT_SOURCE_DIR}/src/test/storm-dft")

# Note that the tests also need the source files, except for the main file
include_directories(${GTEST_INCLUDE_DIR})
include_directories(${GMOCK_INCLUDE_DIR})

foreach (testsuite api bdd simulator storage transformations)

	file(GLOB_RECURSE TEST_${testsuite}_FILES ${STORM_TESTS_BASE_PATH}/${testsuite}/*.h ${STORM_TESTS_BASE_PATH}/${testsuite}/*.cpp ${STORM_TESTS_BASE_PATH}/../storm_gtest.cpp)
	add_executable (test-dft-${testsuite} ${TEST_${testsuite}_FILES} ${STORM_TESTS_BASE_PATH}/storm-test.cpp)
	target_link_libraries(test-dft-${testsuite} storm-dft storm-parsers)
	target_link_libraries(test-dft-${testsuite} ${STORM_TEST_LINK_LIBRARIES})
	target_include_directories(test-dft-${testsuite} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	storm_target_precompile_headers(test-dft-${testsuite} REUSE_FROM test-builder)

	add_dependencies(test-dft-${testsuite} test-resources)
	add_test(NAME run-test-dft-${testsuite} COMMAND $<TARGET_FILE:test-dft-${testsuite}>)
	add_dependencies(tests test-dft-${testsuite})

endforeach ()
