#pragma once

#include <filesystem>
#include <string>

namespace storm::io {

enum class CompressionMode { Default, None, Gzip, Xz };

/*!
 * @return The compression mode whose string representation matches the given input
 * @throws InvalidArgumentException if the input doesn't match any known compression mode
 */
CompressionMode getCompressionModeFromString(std::string const& input);

/*!
 * @return The string representation of the given input
 */
std::string toString(CompressionMode const& input);

/*!
 * @return the compression that the file extension of the given filename suggests. None if it was not detected.
 */
CompressionMode getCompressionModeFromFileExtension(std::filesystem::path const& filename);

}  // namespace storm::io
