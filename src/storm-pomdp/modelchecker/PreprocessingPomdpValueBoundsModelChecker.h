#pragma once

#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/storage/BeliefExplorationBounds.h"
#include "storm/api/verification.h"

namespace storm {
class Environment;
namespace modelchecker {
template<typename FormulaType, typename ValueType>
class CheckTask;
class CheckResult;
}  // namespace modelchecker
namespace logic {
class Formula;
}
namespace pomdp::modelchecker {
template<typename PomdpType>
class PreprocessingPomdpValueBoundsModelChecker {
   public:
    using PomdpValueType = PomdpType::ValueType;
    typedef pomdp::storage::PreprocessingPomdpValueBounds<PomdpValueType> ValueBounds;
    typedef pomdp::storage::ExtremePOMDPValueBound<PomdpValueType> ExtremeValueBound;

    explicit PreprocessingPomdpValueBoundsModelChecker(PomdpType const& pomdp);

    ValueBounds getValueBounds(storm::logic::Formula const& formula);

    ValueBounds getValueBounds(storm::Environment const& env, storm::logic::Formula const& formula);

    ValueBounds getValueBounds(storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info);

    ExtremeValueBound getExtremeValueBound(storm::logic::Formula const& formula);

    ExtremeValueBound getExtremeValueBound(storm::Environment const& env, storm::logic::Formula const& formula);

    ExtremeValueBound getExtremeValueBound(storm::Environment const& env, storm::logic::Formula const& formula,
                                           storm::pomdp::analysis::FormulaInformation const& info);

   private:
    PomdpType const& pomdp;

    std::vector<PomdpValueType> getChoiceValues(std::vector<PomdpValueType> const& stateValues, std::vector<PomdpValueType>* actionBasedRewards);

    std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>> computeValuesForGuessedScheduler(
        storm::Environment const& env, std::vector<PomdpValueType> const& stateValues, std::vector<PomdpValueType>* actionBasedRewards,
        storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info,
        std::shared_ptr<storm::models::sparse::Mdp<PomdpValueType>> underlyingMdp, PomdpValueType const& scoreThreshold, bool relativeScore);

    std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>> computeValuesForRandomFMPolicy(
        storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info, uint64_t memoryBound);

    [[maybe_unused]] std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>> computeValuesForRandomMemorylessPolicy(
        storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info,
        std::shared_ptr<storm::models::sparse::Mdp<PomdpValueType>> underlyingMdp);
};
}  // namespace pomdp::modelchecker
}  // namespace storm