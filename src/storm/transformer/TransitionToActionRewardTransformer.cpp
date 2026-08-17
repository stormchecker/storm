#include "storm/transformer/TransitionToActionRewardTransformer.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::transformer {

namespace detail {
template<typename ValueType>
using MultiRewardVector = std::vector<ValueType>;

template<typename ValueType>
class RewardTransitionIterator {
   public:
    RewardTransitionIterator(storm::storage::SparseMatrix<ValueType> const& m) : transitionMatrix(m) {}

    void addRewardModel(storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel) {
        if (rewardModel.hasTransitionRewards()) {
            transitionRewards.emplace_back(rewardModel.getTransitionRewardMatrix());
            STORM_LOG_ASSERT(transitionRewards.back()->isSubmatrixOf(transitionMatrix), "Invalid reward matrix.");
        } else {
            transitionRewards.emplace_back();
        }
    }

    template<typename CallBackType>
    void forEachRowEntry(uint64_t rowIndex, bool skip0RewardEntries, CallBackType&& callBack) {
        // Set-up iterators
        std::vector<typename storm::storage::SparseMatrix<ValueType>::const_iterator> rewardIterators;
        std::vector<typename storm::storage::SparseMatrix<ValueType>::const_iterator> rewardIteratorsEnd;
        for (auto const& rewardMatrix : transitionRewards) {
            if (rewardMatrix) {
                rewardIterators.push_back(rewardMatrix->begin(rowIndex));
                rewardIteratorsEnd.push_back(rewardMatrix->end(rowIndex));
            } else {
                rewardIterators.emplace_back();
                rewardIteratorsEnd.emplace_back();
            }
        }

        std::vector<ValueType> rewards(transitionRewards.size());
        for (auto const& entry : transitionMatrix.getRow(rowIndex)) {
            // Fill in rewards for this entry
            bool skipEntry = skip0RewardEntries;
            for (uint64_t i = 0; i < transitionRewards.size(); ++i) {
                if (rewardIterators[i] != rewardIteratorsEnd[i] && rewardIterators[i]->getColumn() == entry.getColumn()) {
                    rewards[i] = rewardIterators[i]->getValue();
                    ++rewardIterators[i];
                    skipEntry = skipEntry && storm::utility::isZero(rewards[i]);
                } else {
                    rewards[i] = storm::utility::zero<ValueType>();
                }
            }
            if (!skipEntry) {
                callBack(entry.getColumn(), entry.getValue(), rewards);
            }
        }
    }

   private:
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    std::vector<storm::OptionalRef<storm::storage::SparseMatrix<ValueType> const>> transitionRewards;
};

}  // namespace detail

template<typename ValueType>
TransitionToActionRewardTransformerReturnType<ValueType> transformTransitionToActionRewards(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> originalModel, std::vector<std::string> const& relevantRewardModelNames) {
    STORM_LOG_ASSERT(originalModel, "Model must not be null.");
    detail::RewardTransitionIterator<ValueType> rewardTransitionIterator(originalModel->getTransitionMatrix());
    bool hasTransitionRewards = false;
    for (auto const& rewardModelName : relevantRewardModelNames) {
        auto const& rewardModel = originalModel->getRewardModel(rewardModelName);
        if (rewardModel.hasTransitionRewards()) {
            hasTransitionRewards = true;
        }
        rewardTransitionIterator.addRewardModel(rewardModel);
    }
    if (!hasTransitionRewards) {
        return {originalModel->template as<storm::models::sparse::Model<ValueType>>(),
                {storm::utility::vector::buildVectorForRange<uint64_t>(0, originalModel->getNumberOfStates())}};
    }

    // Make a pass to find the different rewards with which a state is entered
    std::vector<std::set<detail::MultiRewardVector<ValueType>>> incomingRewards(originalModel->getNumberOfStates());
    auto const& transitions = originalModel->getTransitionMatrix();
    for (uint64_t row = 0; row < transitions.getRowCount(); ++row) {
        rewardTransitionIterator.forEachRowEntry(
            row, true,
            [&incomingRewards](uint64_t column, ValueType, detail::MultiRewardVector<ValueType> const& rewards) { incomingRewards[column].insert(rewards); });
    }

    // Create a mapping from original to new indices
    std::vector<uint64_t> originalToNewIndex;
    uint64_t numStates = 0;
    for (auto const& incRewardsSet : incomingRewards) {
        numStates += incRewardsSet.size();
        originalToNewIndex.push_back(numStates);
        ++numStates;
    }

    // Populate the new transition matrix and (action) rewards for intermediate states
    uint64_t const numIntermediateStates = numStates - originalModel->getNumberOfStates();
    bool const useGroups = !transitions.hasTrivialRowGrouping();
    storm::storage::SparseMatrixBuilder<ValueType> newTransitionsBuilder(transitions.getRowCount() + numIntermediateStates, numStates,
                                                                         transitions.getEntryCount() + numIntermediateStates, true, useGroups,
                                                                         useGroups ? numStates : 0ull);
    std::vector<std::vector<ValueType>> newActionRewards(relevantRewardModelNames.size(),
                                                         std::vector<ValueType>(transitions.getRowCount() + numIntermediateStates));
    uint64_t currNewRow = 0;
    for (uint64_t currOrigState = 0; currOrigState < originalModel->getNumberOfStates(); ++currOrigState) {
        uint64_t const currNewState = originalToNewIndex[currOrigState];
        // First add the transitions and rewards for the intermediate states
        for (auto const& incRewardsSet : incomingRewards[currOrigState]) {
            if (useGroups) {
                newTransitionsBuilder.newRowGroup(currNewRow);
            }
            newTransitionsBuilder.addNextValue(currNewRow, currNewState, storm::utility::one<ValueType>());
            auto newRewIt = newActionRewards.begin();
            for (auto const& rew : incRewardsSet) {
                (*newRewIt)[currNewRow] = rew;
                ++newRewIt;
            }
            ++currNewRow;
        }
        // Add the transitions and rewards for the original state
        if (useGroups) {
            newTransitionsBuilder.newRowGroup(currNewRow);
        }
        for (auto origRowIndex : transitions.getRowGroupIndices(currOrigState)) {
            rewardTransitionIterator.forEachRowEntry(
                origRowIndex, false,
                [&newTransitionsBuilder, &originalToNewIndex, &incomingRewards, &currNewRow](uint64_t column, ValueType prob,
                                                                                             detail::MultiRewardVector<ValueType> const& rewards) {
                    if (std::all_of(rewards.begin(), rewards.end(), [](ValueType const& r) { return storm::utility::isZero(r); })) {
                        // No transition reward collected so use originial state
                        newTransitionsBuilder.addNextValue(currNewRow, originalToNewIndex[column], prob);
                    } else {
                        // Redirect to intermediate state
                        auto incomingRewardsIt = incomingRewards[column].find(rewards);
                        STORM_LOG_ASSERT(incomingRewardsIt != incomingRewards[column].end(), "Invalid incoming rewards.");
                        uint64_t const intermediateStateIndex =
                            originalToNewIndex[column] - incomingRewards[column].size() + std::distance(incomingRewards[column].begin(), incomingRewardsIt);
                        newTransitionsBuilder.addNextValue(currNewRow, intermediateStateIndex, prob);
                    }
                });
            ++currNewRow;
        }
    }

    // create new state labels and init components
    storm::models::sparse::StateLabeling newLabeling(numStates);
    for (auto const& l : originalModel->getStateLabeling().getLabels()) {
        newLabeling.addLabel(l);
        for (auto origIndex : originalModel->getStateLabeling().getStates(l)) {
            newLabeling.addLabelToState(l, originalToNewIndex[origIndex]);
        }
    }
    storm::storage::sparse::ModelComponents<ValueType> components(newTransitionsBuilder.build(), std::move(newLabeling));

    // create new reward models
    uint64_t rewardIndex = 0;
    for (auto const& rewardModelName : relevantRewardModelNames) {
        auto& newActionRewardVector = newActionRewards[rewardIndex++];
        auto const& oldRewardModel = originalModel->getRewardModel(rewardModelName);
        for (uint64_t oldState = 0; oldState < originalModel->getNumberOfStates(); ++oldState) {
            uint64_t const oldStartRow = transitions.getRowGroupIndices()[oldState];
            uint64_t const newState = originalToNewIndex[oldState];
            uint64_t const newStartRow = useGroups ? components.transitionMatrix.getRowGroupIndices()[newState] : newState;
            uint64_t const numRowsInGroup = useGroups ? transitions.getRowGroupSize(oldState) : 1ull;
            for (uint64_t groupOffset = 0; groupOffset < numRowsInGroup; ++groupOffset) {
                auto& rewValue = newActionRewardVector[newStartRow + groupOffset];
                if (oldRewardModel.hasStateRewards()) {
                    rewValue += oldRewardModel.getStateReward(oldState);
                }
                if (oldRewardModel.hasStateActionRewards()) {
                    rewValue += oldRewardModel.getStateActionReward(oldStartRow + groupOffset);
                }
            }
        }
        storm::models::sparse::StandardRewardModel<ValueType> newRewardModel(std::nullopt, std::move(newActionRewardVector));
        components.rewardModels.emplace(rewardModelName, std::move(newRewardModel));
    }

    STORM_LOG_WARN_COND(!originalModel->hasChoiceLabeling(), "Choice labellings will be dropped as the transformation is currently not implemented.");
    STORM_LOG_WARN_COND(!originalModel->hasStateValuations(), "State valuations will be dropped as the transformation is currently not implemented.");
    STORM_LOG_WARN_COND(!originalModel->hasChoiceOrigins(), "Choice origins will be dropped as the transformation is currently not implemented.");

    // Model type specific components
    if (originalModel->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        auto const& ma = *originalModel->template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        components.markovianStates = storm::storage::BitVector(numStates);
        components.exitRates = std::vector<ValueType>(numStates, storm::utility::zero<ValueType>());
        for (uint64_t origState = 0; origState < originalModel->getNumberOfStates(); ++origState) {
            uint64_t const newState = originalToNewIndex[origState];
            if (ma.isMarkovianState(origState)) {
                components.markovianStates->set(newState, true);
                components.exitRates->at(newState) = ma.getExitRate(origState);
            }
        }
        components.rateTransitions = false;  // Note that originalModel->getTransitionMatrix() contains probabilities
    } else if (originalModel->isOfType(storm::models::ModelType::Ctmc)) {
        components.rateTransitions = true;
    } else {
        STORM_LOG_THROW(originalModel->isOfType(storm::models::ModelType::Dtmc) || originalModel->isOfType(storm::models::ModelType::Mdp),
                        storm::exceptions::UnexpectedException, "Unhandled model type.");
    }
    return {storm::utility::builder::buildModelFromComponents(originalModel->getType(), std::move(components)), std::move(originalToNewIndex)};
}

template struct TransitionToActionRewardTransformerReturnType<double>;
template struct TransitionToActionRewardTransformerReturnType<storm::RationalNumber>;
template struct TransitionToActionRewardTransformerReturnType<storm::RationalFunction>;

template TransitionToActionRewardTransformerReturnType<double> transformTransitionToActionRewards(
    std::shared_ptr<storm::models::sparse::Model<double>> originalModel, std::vector<std::string> const& relevantRewardModelNames);
template TransitionToActionRewardTransformerReturnType<storm::RationalNumber> transformTransitionToActionRewards(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> originalModel, std::vector<std::string> const& relevantRewardModelNames);
template TransitionToActionRewardTransformerReturnType<storm::RationalFunction> transformTransitionToActionRewards(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> originalModel, std::vector<std::string> const& relevantRewardModelNames);

}  // namespace storm::transformer
