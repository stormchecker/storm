#pragma once

#include <string>

#include "storm-conv/converter/options/JaniConversionOptions.h"

namespace storm {
namespace converter {

struct PrismToJaniConverterOptions {
    PrismToJaniConverterOptions();

    bool allVariablesGlobal;
    std::string suffix;
    JaniConversionOptions janiOptions;
};
}  // namespace converter
}  // namespace storm
