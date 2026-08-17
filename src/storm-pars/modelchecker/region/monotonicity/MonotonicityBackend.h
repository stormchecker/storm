#pragma once

#include <set>
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityKind.h"
#include "storm-pars/utility/parametric.h"

namespace storm {
class Environment;
}

namespace storm::modelchecker {

template<typename ParametricType>
struct AnnotatedRegion;

template<typename ParametricType>
class MonotonicityBackend {
   public:
    using CoefficientType = storm::utility::parametric::CoefficientType_t<ParametricType>;
    using VariableType = storm::utility::parametric::VariableType_t<ParametricType>;
    using Valuation = storm::utility::parametric::Valuation<ParametricType>;
    using MonotonicityKind = storm::analysis::MonotonicityKind;

    MonotonicityBackend() = default;
    virtual ~MonotonicityBackend() = default;

    /*!
     * Sets parameters that are assumed to be monotone throughout the analysis.
     * Previously specified parameters are overwritten.
     * @param parameter the parameter that is assumed to be monotone
     * @param kind the kind of monotonicity. Must be either increasing, decreasing or constant.
     */
    void setMonotoneParameter(VariableType const& parameter, MonotonicityKind const& kind);

    /*!
     * Returns true, if a region model checker needs to implement specific methods to properly use this backend.
     * Returns false, if it is safe and reasonable to use this backend with any given region model checker.
     *
     * @note this returns false in the base class, but might return true in derived classes.
     */
    virtual bool requiresInteractionWithRegionModelChecker() const;

    /*!
     * Returns whether additional model simplifications are recommended when using this backend.
     * @note this returns true in the base class, but might return false in derived classes.
     */
    virtual bool recommendModelSimplifications() const;

    /*!
     * Initializes the monotonicity information for the given region.
     * Overwrites all present monotonicity annotations in the given region.
     */
    virtual void initializeMonotonicity(storm::Environment const& env, AnnotatedRegion<ParametricType>& region);

    /*!
     * Updates the monotonicity information for the given region.
     * Assumes that some monotonicity information is already present (potentially inherited from a parent region) and potentially sharpens the results for the
     * given region.
     */
    virtual void updateMonotonicity(storm::Environment const& env, AnnotatedRegion<ParametricType>& region);

    /*!
     * Updates the monotonicity information for the given region right before splitting it.
     */
    virtual void updateMonotonicityBeforeSplitting(storm::Environment const& env, AnnotatedRegion<ParametricType>& region);

    /*!
     * Returns an optimistic approximation of the monotonicity of the parameters in this region.
     * This means that the returned monotonicity does not necessarily hold, but there is "sufficient hope" that it does.
     */
    virtual std::map<VariableType, MonotonicityKind> getOptimisticMonotonicityApproximation(AnnotatedRegion<ParametricType> const& region);

   protected:
    std::map<VariableType, MonotonicityKind> globallyKnownMonotonicityInformation;
};

}  // namespace storm::modelchecker