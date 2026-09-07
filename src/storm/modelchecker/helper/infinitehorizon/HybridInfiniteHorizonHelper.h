#pragma once
#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"
#include "storm/modelchecker/results/HybridQuantitativeCheckResult.h"
#include "storm/models/symbolic/Model.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
class Environment;

namespace storage {
template<typename ValueType>
class SparseMatrix;
class BitVector;
}  // namespace storage

namespace modelchecker {
namespace helper {

template<typename ValueType, bool Nondeterministic>
class SparseInfiniteHorizonHelper;

/*!
 * Helper class for model checking queries that depend on the long run behavior of the (nondeterministic) system.
 */
template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
class HybridInfiniteHorizonHelper : public SingleValueModelCheckerHelper<ValueType, storm::models::GetModelRepresentation<DdType>::representation> {
   public:
    /*!
     * Initializes the helper for a discrete time model (MDP or DTMC)
     */
    HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix);

    /*!
     * Initializes the helper for a Markov Automaton
     */
    HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                storm::dd::Bdd<DdType> const& markovianStates, storm::dd::Add<DdType, ValueType> const& _exitRates);

    /*!
     * Initializes the helper for a CTMC
     * @note The transition matrix must be probabilistic
     */
    HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                storm::dd::Add<DdType, ValueType> const& _exitRates);

    /*!
     * Computes the long run average probabilities, i.e., the fraction of the time we are in a psiState
     * @return a value for each state
     */
    std::unique_ptr<HybridQuantitativeCheckResult<DdType, ValueType>> computeLongRunAverageProbabilities(Environment const& env,
                                                                                                         storm::dd::Bdd<DdType> const& psiStates);

    /*!
     * Computes the long run average rewards, i.e., the average reward collected per time unit
     * @return a value for each state
     */
    std::unique_ptr<HybridQuantitativeCheckResult<DdType, ValueType>> computeLongRunAverageRewards(
        Environment const& env, storm::models::symbolic::StandardRewardModel<DdType, ValueType> const& rewardModel);

   protected:
    /*!
     * @return true iff this is a computation on a continuous time model (i.e. MA)
     */
    bool isContinuousTime() const;

    /*!
     * @return a sparse infinite horizon helper with the provided explicit model information.
     * @param exitRates exit rates (ignored for discrete time models)
     * @param markovianStates Markovian states or (ignored for non-MA)
     */
    template<bool N = Nondeterministic, std::enable_if_t<N, int> = 0>
    std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> createSparseHelper(
        storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates,
        std::vector<ValueType> const& exitRates, storm::dd::Odd const& odd) const;
    template<bool N = Nondeterministic, std::enable_if_t<!N, int> = 0>
    std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> createSparseHelper(
        storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates,
        std::vector<ValueType> const& exitRates, storm::dd::Odd const& odd) const;

   private:
    storm::models::symbolic::Model<DdType, ValueType> const& _model;
    storm::dd::Add<DdType, ValueType> const& _transitionMatrix;
    storm::dd::Bdd<DdType> const* _markovianStates;
    storm::dd::Add<DdType, ValueType> const* _exitRates;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
