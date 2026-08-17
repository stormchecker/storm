#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "storm/models/sparse/ModelForward.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace logic {
class Formula;
class ProbabilityOperatorFormula;
class RewardOperatorFormula;
}  // namespace logic
namespace transformer {

/*
 * Merges the given target and sink states into single states with a selfloop.
 * Supports Dtmc, Ctmc, Mdp, and MarkovAutomaton models.
 */
template<typename ValueType>
class GoalStateMerger {
   public:
    struct ReturnType {
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model;  // The output model
        std::optional<uint64_t> targetState;                             // The target state of the output model (if reachable)
        std::optional<uint64_t> sinkState;                               // The sink state of the output model (if reachable)
        std::vector<uint64_t> oldToNewStateIndexMapping;  // maps a state from the input model to the corresponding state of the output model. Invalid
                                                          // index if the state does not exist
        storm::storage::BitVector keptChoices;            // The choices of the input model that are still present in the output model
    };

    /*!
     * @throws storm::exceptions::NotSupportedException if the given model is not supported by this transformer.
     */
    GoalStateMerger(storm::models::sparse::Model<ValueType> const& model);

    /*! Computes a submodel of the specified model that only considers the states given by maybeStates as well as
     *  * one target state to which all transitions to a state selected by targetStates are redirected and
     *  * one sink state to which all transitions to a state selected by sinkStates are redirected.
     *
     * If a choiceFilter is given, choices on maybestates that are not selected by the filter will be removed.
     *
     *  Notes:
     *  * the target (or sink) state is not created, if it is not reachable
     *  * by default, the target (or sink) state will get a label iff it is reachable and at least one of the given targetStates (sinkStates) have
     *    that label. If a representative target (sink) state is given, this default behaviour is overridden: the target (sink) state will then get exactly the
     *    labels of the representative state instead. This assumes that the provided label names are already known labels of the input model.
     *  * Only the selected reward models will be kept. The target and sink states will not get any reward.
     *  * Choices that lead from a maybeState to a ~(maybeState | target | sink) state will be removed. An exception is thrown if this leads to deadlocks.
     *  * It is assumed that maybeStates, targetStates, and sinkStates are pairwise disjoint. Otherwise, an exception is thrown.
     *  * The order of the maybeStates will not be affected (i.e. s_1 < s_2 in the input model implies s'_1 < s'_2 in the output model).
     */
    ReturnType mergeTargetAndSinkStates(storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates,
                                        storm::storage::BitVector const& sinkStates,
                                        std::vector<std::string> const& selectedRewardModels = std::vector<std::string>(),
                                        std::optional<storm::storage::BitVector> const& choiceFilter = std::nullopt,
                                        std::optional<uint64_t> representativeTargetState = std::nullopt,
                                        std::optional<uint64_t> representativeSinkState = std::nullopt) const;

    /*!
     * Identifies the maybeStates, targetStates, sinkStates (and, if applicable, the reward model and choice filter) that correspond to the
     * given formula and invokes mergeTargetAndSinkStates accordingly.
     *
     * @param dropUnreachableFromInit if true, maybeStates that are not reachable from the initial states (without hopping over a target/sink state) are
     * dropped. If false, this reachability analysis is skipped, i.e., the result is as if every state of the input model was an initial state.
     *
     * @return the result of the corresponding call to mergeTargetAndSinkStates, or std::nullopt if the given formula is not supported
     * (e.g. because it has non-propositional subformulas or is not one of the supported formula types).
     */
    std::optional<ReturnType> mergeForFormula(storm::logic::Formula const& formula, bool const dropUnreachableFromInit) const;

   private:
    std::optional<ReturnType> mergeForUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula, bool const dropUnreachableFromInit) const;
    std::optional<ReturnType> mergeForBoundedUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula,
                                                                bool const dropUnreachableFromInit) const;
    std::optional<ReturnType> mergeForReachabilityRewards(storm::logic::RewardOperatorFormula const& formula, bool const dropUnreachableFromInit) const;
    std::optional<ReturnType> mergeForCumulativeRewards(storm::logic::RewardOperatorFormula const& formula) const;

    storm::models::sparse::Model<ValueType> const& originalModel;

    /*!
     * Initializes the data required to build the result model
     *
     * @return The initialized result and the number of transitions of the result model
     */
    std::pair<ReturnType, uint64_t> initialize(storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates,
                                               storm::storage::BitVector const& sinkStates,
                                               std::optional<storm::storage::BitVector> const& choiceFilter = std::nullopt) const;

    /*!
     * Builds the transition matrix of the resulting model
     */
    storm::storage::SparseMatrix<ValueType> buildTransitionMatrix(storm::storage::BitVector const& maybeStates, ReturnType const& resultData,
                                                                  uint64_t transitionCount) const;
    storm::models::sparse::StateLabeling buildStateLabeling(storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates,
                                                            storm::storage::BitVector const& sinkStates, ReturnType const& resultData,
                                                            std::optional<uint64_t> const representativeTargetState = std::nullopt,
                                                            std::optional<uint64_t> const representativeSinkState = std::nullopt) const;
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> buildRewardModels(
        storm::storage::BitVector const& maybeStates, ReturnType const& resultData, std::vector<std::string> const& selectedRewardModels) const;
};
}  // namespace transformer
}  // namespace storm
