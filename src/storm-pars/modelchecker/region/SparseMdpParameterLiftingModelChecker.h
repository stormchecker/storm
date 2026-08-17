#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "storm-pars/modelchecker/instantiation/SparseMdpInstantiationModelChecker.h"
#include "storm-pars/modelchecker/region/SparseParameterLiftingModelChecker.h"
#include "storm-pars/transformer/ParameterLifter.h"

#include "storm/solver/GameSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/StateType.h"

namespace storm::modelchecker {

template<typename SparseModelType, typename ConstantType>
class SparseMdpParameterLiftingModelChecker : public SparseParameterLiftingModelChecker<SparseModelType, ConstantType> {
   public:
    using ParametricType = typename SparseModelType::ValueType;
    using CoefficientType = typename RegionModelChecker<ParametricType>::CoefficientType;
    using VariableType = typename RegionModelChecker<ParametricType>::VariableType;
    using Valuation = typename RegionModelChecker<ParametricType>::Valuation;

    SparseMdpParameterLiftingModelChecker();
    SparseMdpParameterLiftingModelChecker(std::unique_ptr<storm::solver::GameSolverFactory<ConstantType>>&& solverFactory);
    virtual ~SparseMdpParameterLiftingModelChecker() = default;

    virtual bool canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel,
                           CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const override;
    virtual void specify(Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel,
                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
                         std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates = std::nullopt,
                         std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend = {}, bool allowModelSimplifications = true,
                         bool graphPreserving = true) override;

    std::optional<storm::storage::Scheduler<ConstantType>> getCurrentMinScheduler();
    std::optional<storm::storage::Scheduler<ConstantType>> getCurrentMaxScheduler();
    std::optional<storm::storage::Scheduler<ConstantType>> getCurrentPlayer1Scheduler();

    /*!
     * Returns whether this region model checker can work together with the given monotonicity backend.
     */
    virtual bool isMonotonicitySupported(MonotonicityBackend<ParametricType> const& backend,
                                         CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const override;

   protected:
    virtual void specifyBoundedUntilFormula(const CheckTask<storm::logic::BoundedUntilFormula, ConstantType>& checkTask) override;
    virtual void specifyUntilFormula(Environment const& env, CheckTask<storm::logic::UntilFormula, ConstantType> const& checkTask) override;
    virtual void specifyReachabilityRewardFormula(Environment const& env, CheckTask<storm::logic::EventuallyFormula, ConstantType> const& checkTask) override;
    virtual void specifyCumulativeRewardFormula(const CheckTask<storm::logic::CumulativeRewardFormula, ConstantType>& checkTask) override;

    virtual storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>& getInstantiationChecker(bool quantitative) override;

    virtual std::vector<ConstantType> computeQuantitativeValues(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                storm::solver::OptimizationDirection const& dirForParameters) override;

    virtual void reset() override;

   private:
    void computePlayer1Matrix(std::optional<storm::storage::BitVector> const& selectedRows = std::nullopt);

    storm::storage::BitVector maybeStates;
    std::vector<ConstantType> resultsForNonMaybeStates;
    std::optional<uint64_t> stepBound;

    std::unique_ptr<storm::modelchecker::SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>> instantiationChecker;

    std::unique_ptr<CheckTask<storm::logic::Formula, ParametricType>> currentCheckTaskNoBound;
    std::shared_ptr<storm::logic::Formula const> currentFormulaNoBound;

    storm::storage::SparseMatrix<storm::storage::sparse::state_type> player1Matrix;
    std::unique_ptr<storm::transformer::ParameterLifter<ParametricType, ConstantType>> parameterLifter;
    std::unique_ptr<storm::solver::GameSolverFactory<ConstantType>> solverFactory;

    // Results from the most recent solver call.
    std::optional<std::vector<uint64_t>> minSchedChoices, maxSchedChoices;
    std::optional<std::vector<uint64_t>> player1SchedChoices;
    std::vector<ConstantType> x;
    std::optional<ConstantType> lowerResultBound, upperResultBound;
    bool applyPreviousResultAsHint;
};
}  // namespace storm::modelchecker
