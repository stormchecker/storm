#include "storm/storage/dd/bisimulation/PartialQuotientExtractor.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
PartialQuotientExtractor<DdType, ValueType, ExportValueType>::PartialQuotientExtractor(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                       storm::dd::bisimulation::QuotientFormat const& quotientFormat)
    : model(model), quotientFormat(quotientFormat) {
    if (this->quotientFormat != storm::dd::bisimulation::QuotientFormat::Dd) {
        STORM_LOG_ERROR("Only DD-based partial quotient extraction is currently supported. Switching to DD-based extraction.");
        this->quotientFormat = storm::dd::bisimulation::QuotientFormat::Dd;
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::Model<ExportValueType>> PartialQuotientExtractor<DdType, ValueType, ExportValueType>::extract(
    Partition<DdType, ValueType> const& partition, PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto start = std::chrono::high_resolution_clock::now();
    std::shared_ptr<storm::models::Model<ExportValueType>> result;

    STORM_LOG_THROW(this->quotientFormat == storm::dd::bisimulation::QuotientFormat::Dd, storm::exceptions::NotSupportedException,
                    "Only DD-based partial quotient extraction is currently supported.");
    result = extractDdQuotient(partition, preservationInformation);
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Quotient extraction completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    STORM_LOG_THROW(result, storm::exceptions::NotSupportedException, "Quotient could not be extracted.");

    return result;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> PartialQuotientExtractor<DdType, ValueType, ExportValueType>::extractDdQuotient(
    Partition<DdType, ValueType> const& partition, PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto modelType = model.getType();
    if (modelType == storm::models::ModelType::Dtmc || modelType == storm::models::ModelType::Mdp) {
        // Sanity checks.
        STORM_LOG_ASSERT(partition.getNumberOfStates() == model.getNumberOfStates(), "Mismatching partition size.");
        STORM_LOG_ASSERT(partition.getStates().renameVariables(model.getColumnVariables(), model.getRowVariables()) == model.getReachableStates(),
                         "Mismatching partition.");

        std::set<storm::expressions::Variable> blockVariableSet = {partition.getBlockVariable()};
        std::set<storm::expressions::Variable> blockPrimeVariableSet = {partition.getPrimedBlockVariable()};
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> blockMetaVariablePairs = {
            std::make_pair(partition.getBlockVariable(), partition.getPrimedBlockVariable())};

        storm::dd::Bdd<DdType> partitionAsBdd = partition.storedAsBdd() ? partition.asBdd() : partition.asAdd().notZero();

        auto start = std::chrono::high_resolution_clock::now();
        partitionAsBdd = partitionAsBdd.renameVariables(model.getColumnVariables(), model.getRowVariables());
        storm::dd::Bdd<DdType> reachableStates = partitionAsBdd.existsAbstract(model.getRowVariables());
        storm::dd::Bdd<DdType> initialStates = (model.getInitialStates() && partitionAsBdd).existsAbstract(model.getRowVariables());

        std::map<std::string, storm::dd::Bdd<DdType>> preservedLabelBdds;
        for (auto const& label : preservationInformation.getLabels()) {
            preservedLabelBdds.emplace(label, (model.getStates(label) && partitionAsBdd).existsAbstract(model.getRowVariables()));
        }
        for (auto const& expression : preservationInformation.getExpressions()) {
            std::stringstream stream;
            stream << expression;
            std::string expressionAsString = stream.str();

            auto it = preservedLabelBdds.find(expressionAsString);
            if (it != preservedLabelBdds.end()) {
                STORM_LOG_WARN("Duplicate label '" << expressionAsString << "', dropping second label definition.");
            } else {
                preservedLabelBdds.emplace(stream.str(), (model.getStates(expression) && partitionAsBdd).existsAbstract(model.getRowVariables()));
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        STORM_LOG_TRACE("Quotient labels extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        start = std::chrono::high_resolution_clock::now();
        std::set<storm::expressions::Variable> blockAndRowVariables;
        std::set_union(blockVariableSet.begin(), blockVariableSet.end(), model.getRowVariables().begin(), model.getRowVariables().end(),
                       std::inserter(blockAndRowVariables, blockAndRowVariables.end()));
        std::set<storm::expressions::Variable> blockPrimeAndColumnVariables;
        std::set_union(blockPrimeVariableSet.begin(), blockPrimeVariableSet.end(), model.getColumnVariables().begin(), model.getColumnVariables().end(),
                       std::inserter(blockPrimeAndColumnVariables, blockPrimeAndColumnVariables.end()));
        storm::dd::Add<DdType, ValueType> partitionAsAdd = partitionAsBdd.template toAdd<ValueType>();
        storm::dd::Add<DdType, ValueType> quotientTransitionMatrix = model.getTransitionMatrix().multiplyMatrix(
            partitionAsAdd.renameVariables(blockAndRowVariables, blockPrimeAndColumnVariables), model.getColumnVariables());

        quotientTransitionMatrix = quotientTransitionMatrix * partitionAsAdd;
        end = std::chrono::high_resolution_clock::now();

        // Check quotient matrix for sanity.
        if (std::is_same<ValueType, storm::RationalNumber>::value) {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>()).isZero(), "Illegal entries in quotient matrix.");
        } else {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>() + storm::utility::convertNumber<ValueType>(1e-6)).isZero(),
                             "Illegal entries in quotient matrix.");
        }

        STORM_LOG_TRACE("Quotient transition matrix extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        storm::dd::Bdd<DdType> quotientTransitionMatrixBdd = quotientTransitionMatrix.notZero();
        std::set<storm::expressions::Variable> nonSourceVariables;
        std::set_union(blockPrimeVariableSet.begin(), blockPrimeVariableSet.end(), model.getRowVariables().begin(), model.getRowVariables().end(),
                       std::inserter(nonSourceVariables, nonSourceVariables.begin()));
        storm::dd::Bdd<DdType> deadlockStates = !quotientTransitionMatrixBdd.existsAbstract(nonSourceVariables) && reachableStates;

        start = std::chrono::high_resolution_clock::now();
        std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<DdType, ValueType>> quotientRewardModels;
        for (auto const& rewardModelName : preservationInformation.getRewardModelNames()) {
            auto const& rewardModel = model.getRewardModel(rewardModelName);

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateRewards;
            if (rewardModel.hasStateRewards()) {
                quotientStateRewards = rewardModel.getStateRewardVector() * partitionAsAdd;
            }

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateActionRewards;
            if (rewardModel.hasStateActionRewards()) {
                quotientStateActionRewards = rewardModel.getStateActionRewardVector() * partitionAsAdd;
            }

            quotientRewardModels.emplace(rewardModelName, storm::models::symbolic::StandardRewardModel<DdType, ValueType>(
                                                              quotientStateRewards, quotientStateActionRewards, boost::none));
        }
        end = std::chrono::high_resolution_clock::now();
        STORM_LOG_TRACE("Reward models extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> result;
        if (modelType == storm::models::ModelType::Dtmc) {
            result = std::make_shared<storm::models::symbolic::Mdp<DdType, ValueType>>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, blockVariableSet,
                blockPrimeVariableSet, blockMetaVariablePairs, model.getRowVariables(), preservedLabelBdds, quotientRewardModels);
        } else if (modelType == storm::models::ModelType::Mdp) {
            std::set<storm::expressions::Variable> allNondeterminismVariables;
            std::set_union(model.getRowVariables().begin(), model.getRowVariables().end(), model.getNondeterminismVariables().begin(),
                           model.getNondeterminismVariables().end(), std::inserter(allNondeterminismVariables, allNondeterminismVariables.begin()));

            result = std::make_shared<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, blockVariableSet,
                blockPrimeVariableSet, blockMetaVariablePairs, model.getRowVariables(), model.getNondeterminismVariables(), allNondeterminismVariables,
                preservedLabelBdds, quotientRewardModels);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported quotient type.");
        }

        return result->template toValueType<ExportValueType>();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Cannot extract partial quotient for this model type.");
    }
}

template class PartialQuotientExtractor<storm::dd::DdType::CUDD, double>;
template class PartialQuotientExtractor<storm::dd::DdType::Sylvan, double>;

template class PartialQuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class PartialQuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalNumber, double>;
template class PartialQuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
