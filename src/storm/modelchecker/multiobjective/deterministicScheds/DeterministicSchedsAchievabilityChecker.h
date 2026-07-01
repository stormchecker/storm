#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace storm {
class Environment;

namespace modelchecker {

class CheckResult;

namespace multiobjective {
template<typename SparseModelType, typename GeometryValueType>
class DeterministicSchedsLpChecker;

template<typename SparseModelType>
class DeterministicSchedsObjectiveHelper;

namespace preprocessing {
template<typename SparseModelType>
struct SparseMultiObjectivePreprocessorResult;
}

template<class SparseModelType, typename GeometryValueType>
class DeterministicSchedsAchievabilityChecker {
   public:
    typedef typename SparseModelType::ValueType ModelValueType;

    DeterministicSchedsAchievabilityChecker(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>& preprocessorResult);

    virtual std::unique_ptr<CheckResult> check(Environment const& env);

   private:
    std::shared_ptr<DeterministicSchedsLpChecker<SparseModelType, GeometryValueType>> lpChecker;
    std::vector<DeterministicSchedsObjectiveHelper<SparseModelType>> objectiveHelper;

    std::shared_ptr<SparseModelType> model;
    uint64_t const originalModelInitialState;
    std::optional<uint64_t> optimizingObjectiveIndex;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm