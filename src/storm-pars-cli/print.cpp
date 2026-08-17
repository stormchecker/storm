#include "storm-pars-cli/print.h"
#include "storm-cli-utilities/print.h"
#include "storm-pars/api/region.h"
#include "storm-pars/settings/modules/PartitionSettings.h"

#include "storm/settings/SettingsManager.h"

namespace storm::pars {

template<typename ValueType>
void printInitialStatesResult(std::unique_ptr<storm::modelchecker::CheckResult> const &result, storm::utility::Stopwatch *watch,
                              const storm::utility::parametric::Valuation<ValueType> *valuation) {
    if (result) {
        STORM_PRINT_AND_LOG("Result (initial states)");
        if (valuation) {
            bool first = true;
            std::stringstream ss;
            for (auto const &entry : *valuation) {
                if (!first) {
                    ss << ", ";
                } else {
                    first = false;
                }
                ss << entry.first << "=" << entry.second;
            }

            STORM_PRINT_AND_LOG(" for instance [" << ss.str() << "]");
        }
        STORM_PRINT_AND_LOG(": ");

        auto const *regionCheckResult = dynamic_cast<storm::modelchecker::RegionCheckResult<ValueType> const *>(result.get());
        if (regionCheckResult != nullptr) {
            auto partitionSettings = storm::settings::getModule<storm::settings::modules::PartitionSettings>();
            std::stringstream outStream;
            if (partitionSettings.isPrintFullResultSet()) {
                regionCheckResult->writeToStream(outStream);
            } else {
                regionCheckResult->writeCondensedToStream(outStream);
            }
            outStream << '\n';
            if (!partitionSettings.isPrintNoIllustrationSet()) {
                auto const *regionRefinementCheckResult = dynamic_cast<storm::modelchecker::RegionRefinementCheckResult<ValueType> const *>(regionCheckResult);
                if (regionRefinementCheckResult != nullptr) {
                    regionRefinementCheckResult->writeIllustrationToStream(outStream);
                }
            }
            outStream << '\n';
            STORM_PRINT_AND_LOG(outStream.str());
        } else {
            STORM_PRINT_AND_LOG(*result << '\n');
        }
        if (watch) {
            STORM_PRINT_AND_LOG("Time for model checking: " << *watch << ".\n\n");
        }
    } else {
        STORM_LOG_ERROR("Property is unsupported by selected engine/settings.\n");
    }
}

template void printInitialStatesResult<storm::RationalFunction>(std::unique_ptr<storm::modelchecker::CheckResult> const &result,
                                                                storm::utility::Stopwatch *watch,
                                                                const storm::utility::parametric::Valuation<storm::RationalFunction> *valuation);
}  // namespace storm::pars