#pragma once

#include <iostream>

#include "storm/exceptions/FileIoException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace io {

/*!
 * Open the given file for writing.
 *
 * @param filepath Path and name of the file to be written to.
 * @param filestream Contains the file handler afterwards.
 * @param append If true, the new content is appended instead of clearing the existing content.
 */
inline void openFile(std::string const& filepath, std::ofstream& filestream, bool append = false, bool silent = false) {
    if (append) {
        filestream.open(filepath, std::ios::app);
    } else {
        filestream.open(filepath);
    }
    STORM_LOG_THROW(filestream, storm::exceptions::FileIoException, "Could not open file " << filepath << ".");
    filestream.precision(std::cout.precision());
    if (!silent) {
        STORM_LOG_INFO("Write to file " << filepath << ".\n");
    }
}

/*!
 * Open the given file for reading.
 *
 * @param filepath Path and name of the file to be tested.
 * @param filestream Contains the file handler afterwards.
 */
inline void openFile(std::string const& filepath, std::ifstream& filestream) {
    filestream.open(filepath);
    STORM_LOG_THROW(filestream, storm::exceptions::FileIoException, "Could not open file " << filepath << ".");
}

/*!
 * Close the given file after writing.
 *
 * @param stream Contains the file handler to close.
 */
inline void closeFile(std::ofstream& stream) {
    stream.close();
}

/*!
 * Close the given file after reading.
 *
 * @param stream Contains the file handler to close.
 */
inline void closeFile(std::ifstream& stream) {
    stream.close();
}

/*!
 * Tests whether the given file exists and is readable.
 *
 * @param filename Path and name of the file to be tested.
 * @return True iff the file exists and is readable.
 */
inline bool fileExistsAndIsReadable(std::string const& filename) {
    // Test by opening an input file stream and testing the stream flags.
    std::ifstream filestream;
    filestream.open(filename);
    return filestream.good();
}

/*!
 * Overloaded getline function which handles different types of newline (\n and \r).
 * @param input Input stream.
 * @param str Output string.
 * @return Remaining stream.
 */
template<class CharT, class Traits, class Allocator>
inline std::basic_istream<CharT, Traits>& getline(std::basic_istream<CharT, Traits>& input, std::basic_string<CharT, Traits, Allocator>& str) {
    auto& res = std::getline(input, str);
    // Remove linebreaks
    std::string::reverse_iterator rit = str.rbegin();
    while (rit != str.rend()) {
        if (*rit == '\r' || *rit == '\n') {
            ++rit;
            str.pop_back();
        } else {
            break;
        }
    }
    return res;
}

}  // namespace io
}  // namespace storm
