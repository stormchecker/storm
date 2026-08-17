#include "storm-pars/modelchecker/region/AnnotatedRegion.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::modelchecker {

template<typename ParametricType>
AnnotatedRegion<ParametricType>::AnnotatedRegion(Region const& region) : region(region) {
    // Intentionally left empty
}

template<typename ParametricType>
void AnnotatedRegion<ParametricType>::propagateAnnotationsToSubregions(bool allowDeleteAnnotationsOfThis) {
    for (auto& r : subRegions) {
        if (result == storm::modelchecker::RegionResult::AllSat || result == storm::modelchecker::RegionResult::AllViolated) {
            r.result = result;
        } else if ((result == storm::modelchecker::RegionResult::CenterSat || result == storm::modelchecker::RegionResult::CenterViolated) &&
                   r.result == storm::modelchecker::RegionResult::Unknown && r.region.contains(region.getCenterPoint())) {
            r.result = result == storm::modelchecker::RegionResult::CenterSat ? storm::modelchecker::RegionResult::ExistsSat
                                                                              : storm::modelchecker::RegionResult::ExistsViolated;
        }
        if (r.refinementDepth == 0) {
            r.refinementDepth = refinementDepth + 1;
        }
        r.monotonicityAnnotation =
            monotonicityAnnotation;  // Potentially shared for all subregions! Creating actual copies is handled via the monotonicity backend
        r.knownLowerValueBound &= knownLowerValueBound;
        r.knownUpperValueBound &= knownUpperValueBound;
    }
    if (allowDeleteAnnotationsOfThis) {
        // Delete annotations that are memory intensive
        monotonicityAnnotation = {};
    }
}

template<typename ParametricType>
void AnnotatedRegion<ParametricType>::splitAndPropagate(typename Region::Valuation const& splittingPoint, std::set<VariableType> const& consideredVariables,
                                                        std::set<VariableType> const& discreteVariables, bool allowDeleteAnnotationsOfThis) {
    std::vector<storm::storage::ParameterRegion<ParametricType>> subRegionsWithoutAnnotations;
    region.split(splittingPoint, subRegionsWithoutAnnotations, consideredVariables, discreteVariables);
    subRegions.reserve(subRegionsWithoutAnnotations.size());
    for (auto& newRegion : subRegionsWithoutAnnotations) {
        subRegions.emplace_back(newRegion);
    }
    propagateAnnotationsToSubregions(allowDeleteAnnotationsOfThis);
}

template<typename ParametricType>
void AnnotatedRegion<ParametricType>::splitLeafNodeAtCenter(std::set<VariableType> const& splittingVariables, std::set<VariableType> const& discreteVariables,
                                                            bool allowDeleteAnnotationsOfThis) {
    STORM_LOG_ASSERT(subRegions.empty(), "Region assumed to be a leaf.");
    splitAndPropagate(region.getCenterPoint(), splittingVariables, discreteVariables, allowDeleteAnnotationsOfThis);
}

template<typename ParametricType>
void AnnotatedRegion<ParametricType>::postOrderTraverseSubRegions(std::function<void(AnnotatedRegion<ParametricType>&)> const& visitor) {
    for (auto& child : subRegions) {
        child.postOrderTraverseSubRegions(visitor);
    }
    visitor(*this);
}

template<typename ParametricType>
void AnnotatedRegion<ParametricType>::preOrderTraverseSubRegions(std::function<void(AnnotatedRegion<ParametricType>&)> const& visitor) {
    visitor(*this);
    for (auto& child : subRegions) {
        child.preOrderTraverseSubRegions(visitor);
    }
}

template<typename ParametricType>
uint64_t AnnotatedRegion<ParametricType>::getMaxDepthOfSubRegions() const {
    uint64_t max{0u};
    for (auto const& child : subRegions) {
        max = std::max(max, child.getMaxDepthOfSubRegions() + 1);
    }
    return max;
}

template<typename ParametricType>
bool AnnotatedRegion<ParametricType>::updateValueBound(CoefficientType const& newValue, storm::OptimizationDirection dir) {
    if (minimize(dir)) {
        return knownLowerValueBound &= newValue;
    } else {
        return knownUpperValueBound &= newValue;
    }
}

template struct AnnotatedRegion<storm::RationalFunction>;

}  // namespace storm::modelchecker