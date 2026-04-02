#include "storm/transformer/AddUncertainty.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::transformer {

template<typename ValueType>
AddUncertainty<ValueType>::AddUncertainty(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& originalModel) : origModel(originalModel) {}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<typename AddUncertainty<ValueType>::IntervalType>> AddUncertainty<ValueType>::transform(
    ValueType additiveUncertainty, ValueType minimalTransitionProbability, std::optional<uint64_t> maxSuccessors) {
    // we first build the matrix and later copy the row grouping.
    auto newMatrixBuilder =
        storage::SparseMatrixBuilder<IntervalType>(origModel->getTransitionMatrix().getRowCount(), origModel->getTransitionMatrix().getColumnCount(),
                                                   origModel->getTransitionMatrix().getNonzeroEntryCount(), true, false);
    // Build transition matrix (without row grouping)
    for (uint64_t rowIndex = 0; rowIndex < origModel->getTransitionMatrix().getRowCount(); ++rowIndex) {
        if (!maxSuccessors.has_value() || origModel->getTransitionMatrix().getRowEntryCount(rowIndex) <= maxSuccessors.value()) {
            for (auto const& entry : origModel->getTransitionMatrix().getRow(rowIndex)) {
                newMatrixBuilder.addNextValue(rowIndex, entry.getColumn(), addUncertainty(entry.getValue(), additiveUncertainty, minimalTransitionProbability));
            }
        } else {
            for (auto const& entry : origModel->getTransitionMatrix().getRow(rowIndex)) {
                newMatrixBuilder.addNextValue(rowIndex, entry.getColumn(), storm::utility::convertNumber<IntervalType>(entry.getValue()));
            }
        }
    }
    storm::storage::sparse::ModelComponents<IntervalType> modelComponents(newMatrixBuilder.build(), origModel->getStateLabeling());
    if (!origModel->getTransitionMatrix().hasTrivialRowGrouping()) {
        modelComponents.transitionMatrix.setRowGroupIndices(origModel->getTransitionMatrix().getRowGroupIndices());
    }
    // Change value type of standard reward model.
    std::unordered_map<std::string, models::sparse::StandardRewardModel<IntervalType>> newRewardModels;
    for (auto const& rewModel : origModel->getRewardModels()) {
        auto const& oldRewModel = rewModel.second;
        std::optional<std::vector<IntervalType>> stateRewards;
        std::optional<std::vector<IntervalType>> stateActionRewards;
        if (oldRewModel.hasStateRewards()) {
            stateRewards = utility::vector::convertNumericVector<IntervalType>(oldRewModel.getStateRewardVector());
        }
        if (oldRewModel.hasStateActionRewards()) {
            stateActionRewards = utility::vector::convertNumericVector<IntervalType>(oldRewModel.getStateActionRewardVector());
        }
        STORM_LOG_THROW(!oldRewModel.hasTransitionRewards(), exceptions::NotImplementedException, "Transition rewards are not supported.");
        models::sparse::StandardRewardModel<IntervalType> newRewModel(std::move(stateRewards), std::move(stateActionRewards));
        newRewardModels.emplace(rewModel.first, std::move(newRewModel));
    }

    // remaining model components.
    modelComponents.rewardModels = std::move(newRewardModels);
    modelComponents.stateValuations = origModel->getOptionalStateValuations();
    modelComponents.choiceLabeling = origModel->getOptionalChoiceLabeling();
    modelComponents.choiceOrigins = origModel->getOptionalChoiceOrigins();

    switch (origModel->getType()) {
        case storm::models::ModelType::Dtmc:
            return std::make_shared<storm::models::sparse::Dtmc<IntervalType>>(std::move(modelComponents));
        case storm::models::ModelType::Mdp:
            return std::make_shared<storm::models::sparse::Mdp<IntervalType>>(std::move(modelComponents));
        default:
            STORM_LOG_THROW(false, exceptions::NotImplementedException, "Only DTMC and MDP model types are currently supported.");
    }
}

template<typename ValueType>
typename AddUncertainty<ValueType>::IntervalType AddUncertainty<ValueType>::addUncertainty(ValueType const& vt, ValueType additiveUncertainty,
                                                                                           ValueType minimalValue) {
    if (utility::isOne(vt)) {
        return IntervalType(storm::utility::one<ValueType>(), storm::utility::one<ValueType>());
    }
    STORM_LOG_THROW(vt >= minimalValue, storm::exceptions::InvalidArgumentException, "Transition probability is smaller than minimal value");
    ValueType const lowerBound = storm::utility::max<ValueType>(vt - additiveUncertainty, minimalValue);
    ValueType const upperBound = storm::utility::min<ValueType>(vt + additiveUncertainty, storm::utility::one<ValueType>() - minimalValue);
    STORM_LOG_ASSERT(storm::utility::isPositive(lowerBound), "Lower bound must be strictly above zero.");
    STORM_LOG_ASSERT(upperBound < storm::utility::one<ValueType>(), "Upper bound must be strictly below one.");
    return IntervalType(lowerBound, upperBound);
}

template class AddUncertainty<double>;
template class AddUncertainty<storm::RationalNumber>;
}  // namespace storm::transformer