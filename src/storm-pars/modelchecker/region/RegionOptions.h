#pragma once

#include <memory>
#include <optional>
#include <set>

#include "storm-pars/modelchecker/region/RegionCheckEngine.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/storage/ParameterRegion.h"

#include "storm/modelchecker/CheckTask.h"
#include "storm/models/sparse/ModelForward.h"

namespace storm {
namespace pars {
namespace modelchecker {

struct MonotonicityOptions {
    bool useMonotonicity;
    bool useOnlyGlobalMonotonicity;
    bool useBoundsFromPLA;

    explicit MonotonicityOptions(bool useMonotonicity = false, bool useOnlyGlobalMonotonicity = false, bool useBoundsFromPLA = false) {
        this->useMonotonicity = useMonotonicity;
        this->useOnlyGlobalMonotonicity = useOnlyGlobalMonotonicity;
        this->useBoundsFromPLA = useBoundsFromPLA;
    }
};

template<typename ValueType>
struct RegionRefinementOptions {
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task;
    storm::modelchecker::RegionCheckEngine engine;
    storm::modelchecker::RegionSplittingStrategy regionSplittingStrategy;

    MonotonicityOptions monotonicitySetting;
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& discreteVariables;
    bool allowModelSimplification;
    bool graphPreserving;
    bool preconditionsValidated;
    std::optional<std::pair<std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>,
                            std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>>>
        monotoneParameters;

    /**
     * @brief Constructs the region refinement options.
     *
     * @param model A shared pointer to the sparse model.
     * @param task The check task to be performed.
     * @param engine The region check engine to be used.
     * @param regionSplittingStrategy The strategy for splitting regions.
     * @param monotonicitySetting The options for monotonicity (default is a default-constructed MonotonicityOptions).
     * @param discreteVariables A set of discrete variables (default is an empty set).
     * @param allowModelSimplification A flag indicating whether model simplification is allowed (default is true).
     * @param graphPreserving A flag indicating whether the graph should be preserved (default is true).
     * @param preconditionsValidated A flag indicating whether preconditions have been validated (default is false).
     * @param monotoneParameters An optional pair of sets of monotone parameters (default is std::nullopt).
     */
    RegionRefinementOptions(std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                            storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task, storm::modelchecker::RegionCheckEngine engine,
                            storm::modelchecker::RegionSplittingStrategy regionSplittingStrategy,
                            MonotonicityOptions monotonicitySetting = MonotonicityOptions(),
                            std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& discreteVariables = {},
                            bool allowModelSimplification = true, bool graphPreserving = true, bool preconditionsValidated = false,
                            std::optional<std::pair<std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>,
                                                    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>>>
                                monotoneParameters = std::nullopt)
        : model(std::move(model)),
          task(std::move(task)),
          engine(engine),
          regionSplittingStrategy(std::move(regionSplittingStrategy)),
          monotonicitySetting(std::move(monotonicitySetting)),
          discreteVariables(discreteVariables),
          allowModelSimplification(allowModelSimplification),
          graphPreserving(graphPreserving),
          preconditionsValidated(preconditionsValidated),
          monotoneParameters(std::move(monotoneParameters)) {}
};

}  // namespace modelchecker
}  // namespace pars
}  // namespace storm