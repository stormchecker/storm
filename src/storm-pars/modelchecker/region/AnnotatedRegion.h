#pragma once

#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityAnnotation.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/utility/Extremum.h"

namespace storm::modelchecker {
template<typename ParametricType>
struct AnnotatedRegion {
    using Region = storm::storage::ParameterRegion<ParametricType>;
    using VariableType = typename Region::VariableType;
    using CoefficientType = typename Region::CoefficientType;

    explicit AnnotatedRegion(Region const& region);

    void propagateAnnotationsToSubregions(bool allowDeleteAnnotationsOfThis);

    void splitAndPropagate(typename Region::Valuation const& splittingPoint, std::set<VariableType> const& consideredVariables,
                           std::set<VariableType> const& discreteVariables, bool allowDeleteAnnotationsOfThis);

    void splitLeafNodeAtCenter(std::set<VariableType> const& splittingVariables, std::set<VariableType> const& discreteVariables,
                               bool allowDeleteAnnotationsOfThis);

    void postOrderTraverseSubRegions(std::function<void(AnnotatedRegion<ParametricType>&)> const& visitor);

    void preOrderTraverseSubRegions(std::function<void(AnnotatedRegion<ParametricType>&)> const& visitor);

    uint64_t getMaxDepthOfSubRegions() const;

    std::vector<AnnotatedRegion<ParametricType>> subRegions;  /// The subregions of this region

    Region const region;  /// The region this is an annotation for

    uint64_t refinementDepth{0};  /// The depth of the refinement tree this region is in

    storm::modelchecker::RegionResult result{storm::modelchecker::RegionResult::Unknown};  /// The result of the analysis of this region
    bool resultKnownThroughMonotonicity{false};                                            /// Whether the result is known through monotonicity

    storm::modelchecker::MonotonicityAnnotation<ParametricType> monotonicityAnnotation;  /// what is known about this region in terms of monotonicity

    bool updateValueBound(CoefficientType const& newValue, storm::OptimizationDirection dir);

    storm::utility::Maximum<CoefficientType> knownLowerValueBound;  // Maximal known lower bound on the value of the region
    storm::utility::Minimum<CoefficientType> knownUpperValueBound;  // Minimal known upper bound on the value of the region
};
}  // namespace storm::modelchecker