#pragma once

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "storm-pars/analysis/ConstraintCollector.h"
#include "storm-pars/analysis/GraphPreservationStatus.h"
#include "storm-pars/analysis/RegionConstraintChecker.h"
#include "storm-pars/settings/modules/RegionSettings.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/ModelBase.h"
#include "storm/models/sparse/Model.h"
#include "storm/settings/SettingsManager.h"
#include "storm/utility/macros.h"

namespace storm::pars {

/*!
 * Collects the constraints of the model. Returns std::nullopt if the model is not a sparse parametric model.
 */
template<typename ValueType>
std::optional<storm::pars::analysis::ConstraintCollector> collectConstraints(std::shared_ptr<storm::models::ModelBase> const& model) {
    if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        if (model && model->isSparseModel()) {
            try {
                return storm::pars::analysis::ConstraintCollector(*model->template as<storm::models::sparse::Model<ValueType>>());
            } catch (storm::exceptions::BaseException const& exception) {
                STORM_LOG_WARN("Could not collect the constraints of the model; they will be treated as undecided: " << exception.what());
            }
        }
    }
    return std::nullopt;
}

/*!
 * Checks the well-formedness constraints or the graph-preservation constraints over the region.
 * Graph preservation includes well-formedness.
 * Returns std::nullopt if they cannot be checked, e.g. if the region does not cover all constrained variables.
 */
template<typename ValueType>
std::optional<storm::pars::analysis::RegionCheckResult> checkRegionConstraints(std::optional<storm::pars::analysis::ConstraintCollector> const& collector,
                                                                               storm::storage::ParameterRegion<ValueType> const& region, bool wellformedness) {
    if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        if (collector) {
            storm::pars::analysis::RegionConstraintChecker checker(*collector);
            try {
                return wellformedness ? checker.checkWellformedness(region) : checker.checkGraphPreservation(region);
            } catch (storm::exceptions::BaseException const& exception) {
                STORM_LOG_WARN("Could not check the constraints for region " << region << "; treating them as undecided: " << exception.what());
            }
        }
    }
    return std::nullopt;
}

/*!
 * Describes a violated constraint, including the point at which it fails.
 */
inline std::string describeViolation(storm::pars::analysis::RegionCheckResult const& result) {
    std::stringstream message;
    message << "the constraint '" << result.violatedConstraint->toString() << "' is violated";
    if (result.witness) {
        message << " at the point " << *result.witness;
    }
    return message.str();
}

/*!
 * Determines to what extent the regions are known to be well-defined and graph-preserving.
 *
 * Graph preservation implies well-definedness. If graph preservation is assumed, we only warn about regions that
 * are provably not graph-preserving (or not well-formed).
 * Otherwise, if `graphPreservationRequired` is set, a proof is required and an exception is raised if there is
 * none. If it is not set (e.g. for robust parameter lifting, which handles non-graph-preserving regions itself),
 * only well-definedness is determined.
 */
template<typename ValueType>
storm::pars::analysis::GraphPreservationStatus establishGraphPreservation(std::shared_ptr<storm::models::ModelBase> const& model,
                                                                          std::vector<storm::storage::ParameterRegion<ValueType>> const& regions,
                                                                          bool graphPreservationRequired) {
    bool const assumed = storm::settings::getModule<storm::settings::modules::RegionSettings>().isAssumeGraphPreservingSet();
    bool const needsGraphPreservation = assumed || graphPreservationRequired;
    auto const collector = collectConstraints<ValueType>(model);

    storm::pars::analysis::GraphPreservationStatus status =
        needsGraphPreservation ? storm::pars::analysis::GraphPreservationStatus::GraphPreserving : storm::pars::analysis::GraphPreservationStatus::WellDefined;
    for (auto const& region : regions) {
        auto const result = checkRegionConstraints<ValueType>(collector, region, !needsGraphPreservation);
        if (result && result->verdict == storm::pars::analysis::ConstraintVerdict::Sat) {
            continue;
        }
        bool const violated = result && result->verdict == storm::pars::analysis::ConstraintVerdict::Unsat;

        if (!needsGraphPreservation) {
            // Ill-defined regions are handled by the engine itself, so we do not warn.
            if (violated) {
                status = storm::pars::analysis::GraphPreservationStatus::None;
            } else if (status != storm::pars::analysis::GraphPreservationStatus::None) {
                status = storm::pars::analysis::GraphPreservationStatus::Unknown;
            }
        } else if (assumed) {
            STORM_LOG_WARN_COND(!violated, "Region " << region << " does not preserve the graph structure of the parametric model: "
                                                     << describeViolation(*result) << ". Set --assume-graph-preserving false.");
        } else {
            STORM_LOG_THROW(!violated, storm::exceptions::InvalidSettingsException,
                            "Region " << region << " is provably not graph-preserving: " << describeViolation(*result)
                                      << ". Pass --assume-graph-preserving true to proceed nevertheless.");
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Cannot prove that region " << region
                                                        << " preserves the graph structure of the parametric model. This is only possible for sparse "
                                                           "parametric models with decidable (e.g. linear) constraints. Pass --assume-graph-preserving "
                                                           "true to skip this check.");
        }
    }
    return status;
}

}  // namespace storm::pars
