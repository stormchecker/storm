#pragma once

#include <string>

namespace storm {
namespace io {

enum class ModelExportFormat { Dot, Drdd, Drn, Json, Umb };

/*!
 * @return The ModelExportFormat whose string representation matches the given input
 * @throws InvalidArgumentException if the input doesn't match any known format
 */
ModelExportFormat getModelExportFormatFromString(std::string const& input);

/*!
 * @return The string representation of the given input
 */
std::string toString(ModelExportFormat const& input);

/*!
 * @return The ModelExportFormat whose string representation matches the extension of the given file
 * @throws InvalidArgumentException if there is no file extension or if it doesn't match any known format.
 */
ModelExportFormat getModelExportFormatFromFileExtension(std::string const& filename);
}  // namespace io
}  // namespace storm