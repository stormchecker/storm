#pragma once

#include <vector>

#include "storm/modelchecker/multiobjective/pcaa/StandardPcaaWeightVectorChecker.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

/*!
 * Helper Class that takes preprocessed Pcaa data and a weight vector and ...
 * - computes the maximal expected reward w.r.t. the weighted sum of the rewards of the individual objectives
 * - extracts the scheduler that induces this maximum
 * - computes for each objective the value induced by this scheduler
 */
template<class SparseMdpModelType>
class StandardMdpPcaaWeightVectorChecker : public StandardPcaaWeightVectorChecker<SparseMdpModelType> {
   public:
    typedef typename SparseMdpModelType::ValueType ValueType;
    typedef typename SparseMdpModelType::RewardModelType RewardModelType;

    /*
     * Creates a weight vextor checker.
     *
     * @param model The (preprocessed) model
     * @param objectives The (preprocessed) objectives
     * @param possibleECActions Overapproximation of the actions that are part of an EC
     * @param possibleBottomStates The states for which it is posible to not collect further reward with prob. 1
     *
     */

    StandardMdpPcaaWeightVectorChecker(preprocessing::SparseMultiObjectivePreprocessorResult<SparseMdpModelType> const& preprocessorResult);

    virtual ~StandardMdpPcaaWeightVectorChecker() = default;

   protected:
    virtual void initializeModelTypeSpecificData(SparseMdpModelType const& model) override;
    virtual storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> createNondetInfiniteHorizonHelper(
        storm::storage::SparseMatrix<ValueType> const& transitions) const override;
    virtual storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType> createDetInfiniteHorizonHelper(
        storm::storage::SparseMatrix<ValueType> const& transitions) const override;

    virtual ValueType getWeightedPrecisionUnboundedPhase() const override;
    virtual ValueType getWeightedPrecisionBoundedPhase() const override;

   private:
    /*!
     * Computes the maximizing scheduler for the weighted sum of the objectives, including also step bounded objectives.
     * Moreover, the values of the individual objectives are computed w.r.t. this scheduler.
     *
     * @param weightVector the weight vector of the current check
     * @param weightedRewardVector the weighted rewards considering the unbounded objectives. Will be invalidated after calling this.
     */
    virtual void boundedPhase(Environment const& env, std::vector<ValueType> const& weightVector, std::vector<ValueType>& weightedRewardVector) override;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
