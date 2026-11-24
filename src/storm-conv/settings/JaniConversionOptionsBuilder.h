#pragma once

#include "storm-conv/converter/options/JaniConversionOptions.h"

namespace storm::settings {

/*!
 * Builds storm::converter::JaniConversionOptions based on the currently registered JaniExportSettings.
 *
 * The options themselves do not know about settings; this class is the only place where the settings are
 * translated into options.
 */
class JaniConversionOptionsBuilder {
   public:
    JaniConversionOptionsBuilder() = delete;

    static storm::converter::JaniConversionOptions buildJaniConversionOptions();
};

}  // namespace storm::settings
