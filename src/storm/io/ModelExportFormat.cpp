#include "ModelExportFormat.h"

#include <filesystem>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/io/CompressionMode.h"
#include "storm/utility/macros.h"

namespace storm {
namespace io {

ModelExportFormat getModelExportFormatFromString(std::string const& input) {
    if (input == "dot") {
        return ModelExportFormat::Dot;
    } else if (input == "drdd") {
        return ModelExportFormat::Drdd;
    } else if (input == "drn") {
        return ModelExportFormat::Drn;
    } else if (input == "json") {
        return ModelExportFormat::Json;
    } else if (input == "umb") {
        return ModelExportFormat::Umb;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The model export format '" << input << "' does not match any known format.");
}

std::string toString(ModelExportFormat const& input) {
    switch (input) {
        case ModelExportFormat::Dot:
            return "dot";
        case ModelExportFormat::Drdd:
            return "drdd";
        case ModelExportFormat::Drn:
            return "drn";
        case ModelExportFormat::Json:
            return "json";
        case ModelExportFormat::Umb:
            return "umb";
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unhandled model export format.");
}

ModelExportFormat getModelExportFormatFromFileExtension(std::string const& filename) {
    std::filesystem::path path(filename);
    // correctly get the relevant extension (e.g. '.drn' in model.drn and model.drn.gz)
    auto compression = getCompressionModeFromFileExtension(path);
    std::string extension = compression == CompressionMode::None ? path.extension() : path.stem().extension();
    try {
        return getModelExportFormatFromString(extension.substr(1));
    } catch (storm::exceptions::InvalidArgumentException&) {
        STORM_LOG_THROW(
            false, storm::exceptions::InvalidArgumentException,
            "The file '" << filename
                         << "' does not have an extension to determine the model export format from. Add a file extension or specify the format explicitly.");
    }
}

}  // namespace io
}  // namespace storm
