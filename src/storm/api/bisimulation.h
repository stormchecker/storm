#pragma once

#include <optional>

#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/bisimulation/DeterministicModelBisimulationDecomposition.h"
#include "storm/storage/bisimulation/NondeterministicModelBisimulationDecomposition.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/bisimulation/BisimulationDecomposition.h"
#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace api {

template<typename ModelType>
std::shared_ptr<ModelType> performDeterministicSparseBisimulationMinimization(std::shared_ptr<ModelType> model,
                                                                              std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                              storm::storage::BisimulationType type, bool graphPreserving = true,
                                                                              std::optional<double> const& tolerance = std::nullopt) {
    using OptionsType = typename storm::storage::DeterministicModelBisimulationDecomposition<ModelType>::Options;
    // Falls back to the general precision setting when the caller does not deliberately choose a tolerance;
    // may be reworked to require an explicit choice throughout the API in the future.
    typename ModelType::ValueType const resolvedTolerance = storm::NumberTraits<typename ModelType::ValueType>::IsExact
                                                                ? storm::utility::zero<typename ModelType::ValueType>()
                                                                : storm::utility::convertNumber<typename ModelType::ValueType>(tolerance.value_or(
                                                                      storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision()));
    OptionsType options =
        (!formulas.empty() && graphPreserving) ? OptionsType(*model, formulas, resolvedTolerance) : OptionsType::preservingAllLabels(resolvedTolerance);
    // If we cannot use formula-based decomposition because of
    // non-graph-preserving regions but there are reward models, we need to
    // preserve those
    if (!graphPreserving &&
        std::any_of(formulas.begin(), formulas.end(), [](auto const& formula) { return formula->getReferencedRewardModels().size() > 0; })) {
        options.setKeepRewards(true);
    }
    options.setType(type);

    storm::storage::DeterministicModelBisimulationDecomposition<ModelType> bisimulationDecomposition(*model, options);
    bisimulationDecomposition.computeBisimulationDecomposition();
    return bisimulationDecomposition.getQuotient();
}

template<typename ModelType>
std::shared_ptr<ModelType> performNondeterministicSparseBisimulationMinimization(std::shared_ptr<ModelType> model,
                                                                                 std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                                 storm::storage::BisimulationType type, bool graphPreserving = true,
                                                                                 std::optional<double> const& tolerance = std::nullopt) {
    using OptionsType = typename storm::storage::NondeterministicModelBisimulationDecomposition<ModelType>::Options;
    // Falls back to the general precision setting when the caller does not deliberately choose a tolerance;
    // may be reworked to require an explicit choice throughout the API in the future.
    typename ModelType::ValueType const resolvedTolerance = storm::NumberTraits<typename ModelType::ValueType>::IsExact
                                                                ? storm::utility::zero<typename ModelType::ValueType>()
                                                                : storm::utility::convertNumber<typename ModelType::ValueType>(tolerance.value_or(
                                                                      storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision()));
    OptionsType options =
        (!formulas.empty() && graphPreserving) ? OptionsType(*model, formulas, resolvedTolerance) : OptionsType::preservingAllLabels(resolvedTolerance);
    // If we cannot use formula-based decomposition because of
    // non-graph-preserving regions but there are reward models, we need to
    // preserve those
    if (!graphPreserving &&
        std::any_of(formulas.begin(), formulas.end(), [](auto const& formula) { return formula->getReferencedRewardModels().size() > 0; })) {
        options.setKeepRewards(true);
    }
    options.setType(type);

    storm::storage::NondeterministicModelBisimulationDecomposition<ModelType> bisimulationDecomposition(*model, options);
    bisimulationDecomposition.computeBisimulationDecomposition();
    return bisimulationDecomposition.getQuotient();
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> performBisimulationMinimization(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
    storm::storage::BisimulationType type = storm::storage::BisimulationType::Strong, bool graphPreserving = true,
    std::optional<double> const& tolerance = std::nullopt) {
    STORM_LOG_THROW(
        model->isOfType(storm::models::ModelType::Dtmc) || model->isOfType(storm::models::ModelType::Ctmc) || model->isOfType(storm::models::ModelType::Mdp),
        storm::exceptions::NotSupportedException, "Bisimulation minimization is currently only available for DTMCs, CTMCs and MDPs.");

    // Try to get rid of non state-rewards to easy bisimulation computation.
    model->reduceToStateBasedRewards();

    if (model->isOfType(storm::models::ModelType::Dtmc)) {
        return performDeterministicSparseBisimulationMinimization<storm::models::sparse::Dtmc<ValueType>>(
            model->template as<storm::models::sparse::Dtmc<ValueType>>(), formulas, type, graphPreserving, tolerance);
    } else if (model->isOfType(storm::models::ModelType::Ctmc)) {
        return performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
            model->template as<storm::models::sparse::Ctmc<ValueType>>(), formulas, type, graphPreserving, tolerance);
    } else {
        return performNondeterministicSparseBisimulationMinimization<storm::models::sparse::Mdp<ValueType>>(
            model->template as<storm::models::sparse::Mdp<ValueType>>(), formulas, type, graphPreserving, tolerance);
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
typename std::enable_if<DdType == storm::dd::DdType::Sylvan || std::is_same<ValueType, double>::value,
                        std::shared_ptr<storm::models::Model<ExportValueType>>>::type
performBisimulationMinimization(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
                                std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                storm::storage::BisimulationType const& bisimulationType = storm::storage::BisimulationType::Strong,
                                storm::dd::bisimulation::SignatureMode const& mode = storm::dd::bisimulation::SignatureMode::Eager,
                                storm::dd::bisimulation::QuotientFormat const& quotientFormat = storm::dd::bisimulation::QuotientFormat::Dd,
                                storm::dd::bisimulation::BisimulationOptions const& bisimulationOptions = storm::dd::bisimulation::BisimulationOptions()) {
    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc) || model->isOfType(storm::models::ModelType::Ctmc) ||
                        model->isOfType(storm::models::ModelType::Mdp) || model->isOfType(storm::models::ModelType::MarkovAutomaton),
                    storm::exceptions::NotSupportedException, "Symbolic bisimulation minimization is currently only available for DTMCs, CTMCs, MDPs and MAs.");
    STORM_LOG_THROW(bisimulationType == storm::storage::BisimulationType::Strong, storm::exceptions::NotSupportedException,
                    "Currently only strong bisimulation is supported.");

    std::shared_ptr<storm::models::Model<ExportValueType>> result;
    model->getManager().execute([&]() {
        // Try to get rid of non state-rewards to easy bisimulation computation.
        model->reduceToStateBasedRewards();

        storm::dd::BisimulationDecomposition<DdType, ValueType, ExportValueType> decomposition(*model, formulas, bisimulationType, bisimulationOptions);
        decomposition.compute(mode);
        result = decomposition.getQuotient(quotientFormat);
    });
    return result;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
typename std::enable_if<DdType != storm::dd::DdType::Sylvan && !std::is_same<ValueType, double>::value,
                        std::shared_ptr<storm::models::Model<ExportValueType>>>::type
performBisimulationMinimization(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const&,
                                std::vector<std::shared_ptr<storm::logic::Formula const>> const&,
                                storm::storage::BisimulationType const& = storm::storage::BisimulationType::Strong,
                                storm::dd::bisimulation::SignatureMode const& = storm::dd::bisimulation::SignatureMode::Eager,
                                storm::dd::bisimulation::QuotientFormat const& = storm::dd::bisimulation::QuotientFormat::Dd,
                                storm::dd::bisimulation::BisimulationOptions const& = storm::dd::bisimulation::BisimulationOptions()) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                    "Symbolic bisimulation minimization is not supported for this combination of DD library and value type.");
    return nullptr;
}

}  // namespace api
}  // namespace storm
