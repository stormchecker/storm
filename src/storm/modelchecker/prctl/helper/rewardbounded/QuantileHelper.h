#pragma once

#include <boost/optional.hpp>
#include <map>

#include "storm/logic/ProbabilityOperatorFormula.h"
#include "storm/logic/QuantileFormula.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/CostLimitClosure.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/Stopwatch.h"

namespace storm {
class Environment;

namespace modelchecker {
namespace helper {
namespace rewardbounded {

template<typename ModelType>
class QuantileHelper {
    typedef typename ModelType::ValueType ValueType;

   public:
    QuantileHelper(ModelType const& model, storm::logic::QuantileFormula const& quantileFormula);

    std::vector<std::vector<ValueType>> computeQuantile(Environment const& env);

   private:
    std::pair<CostLimitClosure, std::vector<ValueType>> computeQuantile(Environment& env, storm::storage::BitVector const& consideredDimensions,
                                                                        bool complementaryQuery);
    bool computeQuantile(Environment& env, storm::storage::BitVector const& consideredDimensions,
                         storm::logic::ProbabilityOperatorFormula const& boundedUntilOperator, storm::storage::BitVector const& lowerBoundedDimensions,
                         CostLimitClosure& satCostLimits, CostLimitClosure& unsatCostLimits, MultiDimensionalRewardUnfolding<ValueType, true>& rewardUnfolding);

    /*!
     * Gets the number of dimensions of the underlying boudned until formula
     */
    uint64_t getDimension() const;

    /*!
     * Gets the dimensions that are open, i.e., for which the bound value is not fixed
     * @return
     */
    storm::storage::BitVector getOpenDimensions() const;

    storm::expressions::Variable const& getVariableForDimension(uint64_t const& dim) const;

    ModelType const& model;
    storm::logic::QuantileFormula const& quantileFormula;
    std::map<storm::storage::BitVector, std::pair<CostLimitClosure, std::vector<ValueType>>> cachedSubQueryResults;

    /// Statistics
    mutable uint64_t numCheckedEpochs;
    mutable uint64_t numPrecisionRefinements;
    mutable storm::utility::Stopwatch swEpochAnalysis;
    mutable storm::utility::Stopwatch swExploration;
};
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
