#pragma once

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/OptionalRef.h"

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"
#include "storm-pars/transformer/ParameterLifter.h"

namespace storm::transformer {
template<typename ParametricType, typename ConstantType>
class ParameterLifter;
}  // namespace storm::transformer

namespace storm::storage {
class BitVector;
template<typename T>
class SparseMatrix;
}  // namespace storm::storage

namespace storm::modelchecker {

template<typename ParametricType, typename ConstantType, bool Robust>
class SparseDtmcParameterLiftingModelChecker;

template<typename ParametricType, typename ConstantType>
class OrderBasedMonotonicityBackend : public MonotonicityBackend<ParametricType> {
   public:
    using CoefficientType = typename MonotonicityBackend<ParametricType>::CoefficientType;
    using VariableType = typename MonotonicityBackend<ParametricType>::VariableType;
    using Valuation = typename MonotonicityBackend<ParametricType>::Valuation;
    using MonotonicityKind = typename MonotonicityBackend<ParametricType>::MonotonicityKind;

    template<typename ModelType, typename ConstantType2, bool Robust>
    friend class SparseDtmcParameterLiftingModelChecker;

    OrderBasedMonotonicityBackend(bool useOnlyGlobal = false, bool useBounds = false);
    virtual ~OrderBasedMonotonicityBackend() = default;

    /*!
     * Returns true, since a region model checker needs to implement specific methods to properly use this backend.
     */
    virtual bool requiresInteractionWithRegionModelChecker() const override;

    /*!
     * Returns whether additional model simplifications are recommended when using this backend.
     * @note this returns false since the underlying monotonicity checker already does simplifications
     */
    virtual bool recommendModelSimplifications() const override;

    /*!
     * Initializes the monotonicity information for the given region.
     * Overwrites all present monotonicity annotations in the given region.
     */
    virtual void initializeMonotonicity(storm::Environment const& env, AnnotatedRegion<ParametricType>& region) override;

    /*!
     * Updates the monotonicity information for the given region.
     * Assumes that some monotonicity information is already present (potentially inherited from a parent region) and potentially sharpens the results for the
     * given region.
     */
    virtual void updateMonotonicity(storm::Environment const& env, AnnotatedRegion<ParametricType>& region) override;

    /*!
     * Updates the monotonicity information for the given region right before splitting it.
     */
    virtual void updateMonotonicityBeforeSplitting(storm::Environment const& env, AnnotatedRegion<ParametricType>& region) override;

    /*!
     * Returns an optimistic approximation of the monotonicity of the parameters in this region.
     * This means that the returned monotonicity does not necessarily hold, but there is "sufficient hope" that it does.
     */
    virtual std::map<VariableType, MonotonicityKind> getOptimisticMonotonicityApproximation(AnnotatedRegion<ParametricType> const& region) override;

   private:
    // Interaction with SparseDtmcParameterLiftingModelChecker:
    void registerParameterLifterReference(storm::transformer::ParameterLifter<ParametricType, ConstantType> const& parameterLifter);
    void registerPLABoundFunction(
        std::function<std::vector<ConstantType>(storm::Environment const&, AnnotatedRegion<ParametricType>&, storm::OptimizationDirection)> fun);
    void initializeMonotonicityChecker(storm::storage::SparseMatrix<ParametricType> const& parametricTransitionMatrix);
    void initializeOrderExtender(storm::storage::BitVector const& topStates, storm::storage::BitVector const& bottomStates,
                                 storm::storage::SparseMatrix<ParametricType> const& parametricTransitionMatrix);
    storm::storage::BitVector getChoicesToFixForPLASolver(AnnotatedRegion<ParametricType> const& region, storm::OptimizationDirection dir,
                                                          std::vector<uint64_t>& schedulerChoices);

   private:
    bool const useOnlyGlobal;
    bool const useBounds;

    std::optional<storm::analysis::OrderExtender<ParametricType, ConstantType>> orderExtender;
    std::optional<storm::analysis::MonotonicityChecker<ParametricType>> monotonicityChecker;
    std::set<VariableType> possibleMonotoneParameters;
    storm::OptionalRef<storm::transformer::ParameterLifter<ParametricType, ConstantType> const> parameterLifterRef;
    std::function<std::vector<ConstantType>(storm::Environment const&, AnnotatedRegion<ParametricType>&, storm::OptimizationDirection)> plaBoundFunction;
};

}  // namespace storm::modelchecker