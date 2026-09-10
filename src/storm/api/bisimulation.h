#pragma once

#include <optional>

#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/bisimulation/BisimulationDecomposition.h"
#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/transformer/bisimulation/Bisimulation.h"
#include "storm/utility/macros.h"

namespace storm {
namespace api {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> performBisimulationMinimization(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = {},
    storm::bisimulation::Options const& options = {}) {
    return storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, options).quotient;
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
