#pragma once

#include <vector>

#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"

#include "storm/utility/Stopwatch.h"

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
class RewardBoundedMdpPcaaWeightVectorChecker : public PcaaWeightVectorChecker<SparseMdpModelType> {
   public:
    typedef typename SparseMdpModelType::ValueType ValueType;
    typedef typename SparseMdpModelType::RewardModelType RewardModelType;

    RewardBoundedMdpPcaaWeightVectorChecker(preprocessing::SparseMultiObjectivePreprocessorResult<SparseMdpModelType> const& preprocessorResult);

    virtual ~RewardBoundedMdpPcaaWeightVectorChecker();

    /*!
     * - computes the optimal expected reward w.r.t. the weighted sum of the rewards of the individual objectives
     * - extracts the scheduler that induces this optimum
     * - computes for each objective the value induced by this scheduler
     */
    virtual void check(Environment const& env, std::vector<ValueType> weightVector) override;

    virtual std::vector<ValueType> getAchievablePoint() const override;
    ValueType getOptimalWeightedSum() const override;

   private:
    struct EpochCheckingData {
        std::vector<ValueType> bMinMax;
        std::vector<ValueType> xMinMax;
        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> minMaxSolver;

        std::vector<uint64_t> schedulerChoices;

        std::vector<ValueType> bLinEq;
        std::vector<std::vector<ValueType>> xLinEq;
        std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> linEqSolver;

        std::vector<typename helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, false>::SolutionType> solutions;
    };

    void computeEpochSolution(Environment const& env, typename helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, false>::Epoch const& epoch,
                              std::vector<ValueType> const& weightVector, EpochCheckingData& cachedData);

    void updateCachedData(Environment const& env, typename helper::rewardbounded::EpochModel<ValueType, false> const& epochModel, EpochCheckingData& cachedData,
                          std::vector<ValueType> const& weightVector);

    storm::utility::Stopwatch swAll, swEpochModelBuild, swEpochModelAnalysis;
    uint64_t numCheckedEpochs, numChecks;

    helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, false> rewardUnfolding;

    std::optional<std::vector<ValueType>> achievablePoint;
    std::optional<ValueType> weightedSum;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
