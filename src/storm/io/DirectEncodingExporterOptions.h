#pragma once

#include <optional>

#include "storm/io/CompressionMode.h"

namespace storm::io {

struct DirectEncodingExporterOptions {
    /*!
     * Allow placeholders for rational functions in the exported DRN file.
     */
    bool allowPlaceholders = true;

    /*!
     * If set, the output precision for floating point numbers in the exported DRN file is set to the given number of digits.
     */
    std::optional<std::size_t> outputPrecision = std::nullopt;

    /*!
     * The type of compression used for the exported DRN file.
     */
    storm::io::CompressionMode compression = storm::io::CompressionMode::None;
};
}  // namespace storm::io