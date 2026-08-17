#include "storm-config.h"
#include "test/storm_gtest.h"

#include <string>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/io/file.h"

TEST(MappedFileTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(storm::parser::MappedFile(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"), storm::exceptions::FileIoException);
}

TEST(MappedFileTest, BasicFunctionality) {
    // Open a file and test if the content is loaded as expected.
    storm::parser::MappedFile file(STORM_TEST_RESOURCES_DIR "/txt/testStringFile.txt");
    std::string testString = "This is a test string.";
    char const* dataPtr = file.getData();
    for (char const* testStringPtr = testString.c_str(); testStringPtr - testString.c_str() < 22; testStringPtr++) {
        ASSERT_EQ(*testStringPtr, *dataPtr);
        dataPtr++;
    }
    // The next character should be an end of line character (actual character varies between operating systems).
    ASSERT_EQ(dataPtr, storm::utility::cstring::forwardToLineEnd(dataPtr));

    // The newline character should be the last contained in the string.
    ASSERT_EQ(file.getDataEnd(), storm::utility::cstring::forwardToNextLine(dataPtr));
}

TEST(MappedFileTest, ExistsAndReadble) {
    // Test the fileExistsAndIsReadable() method under various circumstances.

    // File exists and is readable.
    ASSERT_TRUE(storm::io::fileExistsAndIsReadable(STORM_TEST_RESOURCES_DIR "/txt/testStringFile.txt"));

    // File does not exist.
    ASSERT_FALSE(storm::io::fileExistsAndIsReadable(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"));

    // File exists but is not readable.
    // TODO: Find portable solution to providing a situation in which a file exists but is not readable.
    // ASSERT_FALSE(storm::io::fileExistsAndIsReadable(STORM_TEST_RESOURCES_DIR "/parser/unreadableFile.txt"));
}
