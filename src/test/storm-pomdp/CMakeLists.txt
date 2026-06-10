# Base path for test files
set(STORM_TESTS_BASE_PATH "${PROJECT_SOURCE_DIR}/src/test/storm-pomdp")

# Note that the tests also need the source files, except for the main file
include_directories(${GTEST_INCLUDE_DIR})

foreach (testsuite analysis api modelchecker tracking transformation generator)

	file(GLOB_RECURSE TEST_${testsuite}_FILES ${STORM_TESTS_BASE_PATH}/${testsuite}/*.h ${STORM_TESTS_BASE_PATH}/${testsuite}/*.cpp ${STORM_TESTS_BASE_PATH}/../storm_gtest.cpp)
	add_executable(test-pomdp-${testsuite} ${TEST_${testsuite}_FILES} ${STORM_TESTS_BASE_PATH}/storm-test.cpp)
	target_link_libraries(test-pomdp-${testsuite} storm-pomdp storm-parsers)
	target_link_libraries(test-pomdp-${testsuite} ${STORM_TEST_LINK_LIBRARIES})
	target_include_directories(test-pomdp-${testsuite} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	storm_target_precompile_headers(test-pomdp-${testsuite} REUSE_FROM test-builder)

	add_dependencies(test-pomdp-${testsuite} test-resources)
	add_test(NAME run-test-pomdp-${testsuite} COMMAND $<TARGET_FILE:test-pomdp-${testsuite}>)
	add_dependencies(tests test-pomdp-${testsuite})

endforeach ()
