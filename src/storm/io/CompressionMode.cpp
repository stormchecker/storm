#include "CompressionMode.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm::io {

CompressionMode getCompressionModeFromString(std::string const& input) {
    using enum CompressionMode;
    if (input == "default") {
        return Default;
    } else if (input == "none") {
        return None;
    } else if (input == "gzip") {
        return Gzip;
    } else if (input == "xz") {
        return Xz;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The compression mode '" << input << "' does not match any known mode.");
    return Default;
}

std::string toString(CompressionMode const& input) {
    using enum CompressionMode;
    switch (input) {
        case Default:
            return "default";
        case None:
            return "none";
        case Gzip:
            return "gzip";
        case Xz:
            return "xz";
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unhandled model export format.");
    return "";
}

CompressionMode getCompressionModeFromFileExtension(std::filesystem::path const& filename) {
    if (filename.extension() == ".gz") {
        return CompressionMode::Gzip;
    } else if (filename.extension() == ".xz") {
        return CompressionMode::Xz;
    }
    return CompressionMode::None;
}

}  // namespace storm::io
