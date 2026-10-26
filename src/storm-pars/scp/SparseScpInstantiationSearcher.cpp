#include "storm-pars/scp/SparseScpInstantiationSearcher.h"

#include <algorithm>

#include "storm-pars/modelchecker/instantiation/SparseDtmcInstantiationModelChecker.h"
#include "storm-pars/modelchecker/instantiation/SparseMdpInstantiationModelChecker.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm::pars::scp {

template<typename SparseModelType, typename ConstantType>
SparseScpInstantiationSearcher<SparseModelType, ConstantType>::SparseScpInstantiationSearcher(SparseModelType const& model, ScpOptions options)
    : model(model), options(options) {}

template<typename SparseModelType, typename ConstantType>
ScpResult<ConstantType> SparseScpInstantiationSearcher<SparseModelType, ConstantType>::run(
    storm::Environment const& env, std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task) {
    // Also validated here because ScpOptions can be constructed by library callers.
    STORM_LOG_THROW(options.trustRegionInitial > 1.0, storm::exceptions::NotSupportedException, "ScpOptions::trustRegionInitial must be > 1.");
    STORM_LOG_THROW(options.trustRegionFactor > 1.0, storm::exceptions::NotSupportedException, "ScpOptions::trustRegionFactor must be > 1.");
    STORM_LOG_THROW(options.trustRegionMax > 1.0, storm::exceptions::NotSupportedException, "ScpOptions::trustRegionMax must be > 1.");
    STORM_LOG_THROW(options.trustRegionMinExcess >= 0.0, storm::exceptions::NotSupportedException, "ScpOptions::trustRegionMinExcess must be >= 0.");
    STORM_LOG_THROW(options.maxIterations >= 1, storm::exceptions::NotSupportedException, "ScpOptions::maxIterations must be >= 1.");

    STORM_LOG_THROW(task->getFormula().isProbabilityOperatorFormula() || task->getFormula().isRewardOperatorFormula(), storm::exceptions::NotSupportedException,
                    "SCP currently only supports probability- and reward operator formulas.");
    bool const isReward = task->getFormula().isRewardOperatorFormula();
    STORM_LOG_THROW(task->getFormula().asOperatorFormula().getSubformula().isEventuallyFormula(), storm::exceptions::NotSupportedException,
                    "SCP currently only supports unbounded reachability (F) properties.");
    STORM_LOG_THROW(task->isBoundSet(), storm::exceptions::NotSupportedException, "SCP requires an explicitly given bound.");

    storm::logic::Bound const& bound = task->getBound();
    ThresholdDirection direction = bound.isLowerBound() ? ThresholdDirection::AtLeast : ThresholdDirection::AtMost;
    double threshold = bound.threshold.evaluateAsDouble();

    // Robust reading: AtLeast is checked against Pmin/Rmin, AtMost against Pmax/Rmax.
    storm::solver::OptimizationDirection schedulerDirection =
        (direction == ThresholdDirection::AtLeast) ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;

    // Target states, via a plain propositional check of the (label-based) subformula of "F ...".
    auto const& targetSubformula = task->getFormula().asOperatorFormula().getSubformula().asEventuallyFormula().getSubformula();
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(model);
    storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType> targetCheckTask(targetSubformula);
    storm::storage::BitVector targetStates =
        propositionalChecker.check(targetCheckTask)->template asExplicitQualitativeCheckResult<ParametricType>().getTruthValuesVector();
    storm::storage::BitVector allStates(model.getNumberOfStates(), true);

    // Probability property: zeroStates/oneStates are Prob0/Prob1. Reward property: zeroStates are the
    // target states, oneStates stays empty, and infiniteStates have infinite expected reward. All sets
    // are computed for schedulerDirection.
    storm::storage::BitVector zeroStates, oneStates, infiniteStates;
    storm::storage::BitVector excludedRows(model.getTransitionMatrix().getRowCount(), false);
    if constexpr (std::is_same_v<SparseModelType, storm::models::sparse::Mdp<ParametricType>>) {
        if (isReward) {
            zeroStates = targetStates;
            oneStates = storm::storage::BitVector(model.getNumberOfStates(), false);
            infiniteStates = storm::solver::minimize(schedulerDirection)
                                 ? storm::utility::graph::performProb1E(model.getTransitionMatrix(), model.getTransitionMatrix().getRowGroupIndices(),
                                                                        model.getBackwardTransitions(), allStates, targetStates)
                                 : storm::utility::graph::performProb1A(model.getTransitionMatrix(), model.getTransitionMatrix().getRowGroupIndices(),
                                                                        model.getBackwardTransitions(), allStates, targetStates);
            infiniteStates.complement();
            // A state with finite value can still have actions leading into an infinite-value state;
            // those rows are excluded individually.
            storm::storage::BitVector maybeStates = ~(zeroStates | infiniteStates);
            excludedRows = ~model.getTransitionMatrix().getRowFilter(maybeStates, ~infiniteStates);
        } else {
            auto probs = storm::solver::maximize(schedulerDirection) ? storm::utility::graph::performProb01Max(model, allStates, targetStates)
                                                                     : storm::utility::graph::performProb01Min(model, allStates, targetStates);
            zeroStates = std::move(probs.first);
            oneStates = std::move(probs.second);
            infiniteStates = storm::storage::BitVector(model.getNumberOfStates(), false);
        }
    } else {
        static_assert(std::is_same_v<SparseModelType, storm::models::sparse::Dtmc<ParametricType>>,
                      "SparseScpInstantiationSearcher only supports Mdp and Dtmc models.");
        if (isReward) {
            zeroStates = targetStates;
            oneStates = storm::storage::BitVector(model.getNumberOfStates(), false);
            infiniteStates = storm::utility::graph::performProb1(model.getBackwardTransitions(), allStates, targetStates);
            infiniteStates.complement();
            // A DTMC state with an infinite successor is itself infinite, so no row exclusion is needed.
        } else {
            auto probs = storm::utility::graph::performProb01(model, allStates, targetStates);
            zeroStates = std::move(probs.first);
            oneStates = std::move(probs.second);
            infiniteStates = storm::storage::BitVector(model.getNumberOfStates(), false);
        }
    }

    storm::models::sparse::StandardRewardModel<ParametricType> const* rewardModel = nullptr;
    if (isReward) {
        auto const& rewardFormula = task->getFormula().asRewardOperatorFormula();
        if (rewardFormula.hasRewardModelName()) {
            STORM_LOG_THROW(model.hasRewardModel(rewardFormula.getRewardModelName()), storm::exceptions::NotSupportedException,
                            "The model has no reward model named '" << rewardFormula.getRewardModelName() << "'.");
            rewardModel = &model.getRewardModel(rewardFormula.getRewardModelName());
        } else {
            STORM_LOG_THROW(model.hasUniqueRewardModel(), storm::exceptions::NotSupportedException,
                            "The reward formula does not name a reward model, and the model does not have a unique reward model either.");
            rewardModel = &model.getUniqueRewardModel();
        }
    }

    AffinePMdpDecomposition decomposition(model, rewardModel);

    // Use a real region in both cases: the caller's, or [0,1]^n.
    storm::storage::ParameterRegion<ParametricType> region;
    if (task->isRegionSet()) {
        region = task->getRegion();
        for (auto const& var : decomposition.getParameters()) {
            STORM_LOG_THROW(region.getVariables().count(var) > 0, storm::exceptions::NotSupportedException,
                            "The given region does not cover parameter '" << var << "', which occurs in the model.");
        }
    } else {
        Valuation lower, upper;
        for (auto const& var : decomposition.getParameters()) {
            lower[var] = storm::utility::zero<CoefficientType>();
            upper[var] = storm::utility::one<CoefficientType>();
        }
        region = storm::storage::ParameterRegion<ParametricType>(std::move(lower), std::move(upper));
    }

    std::map<VariableType, std::pair<double, double>> parameterBounds;
    for (auto const& var : decomposition.getParameters()) {
        parameterBounds[var] = {storm::utility::convertNumber<double>(region.getLowerBoundary(var)),
                                storm::utility::convertNumber<double>(region.getUpperBoundary(var))};
    }
    Valuation initialValuation = region.getCenterPoint();

    STORM_LOG_THROW(model.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "SCP requires a model with a unique initial state.");
    uint64_t initialState = model.getInitialStates().getNextSetIndex(0);

    if (zeroStates.get(initialState) || oneStates.get(initialState) || infiniteStates.get(initialState)) {
        // The initial state's value is fixed by the graph structure; ScpLpEncoder requires a state
        // with an unknown value.
        storm::utility::ExtendedValueType<ConstantType> constantValue = oneStates.get(initialState)        ? storm::utility::one<ConstantType>()
                                                                        : infiniteStates.get(initialState) ? storm::utility::positiveInfinity<ConstantType>()
                                                                                                           : storm::utility::zero<ConstantType>();
        return ScpResult<ConstantType>{{constantValue, initialValuation}, bound.isSatisfied(constantValue) ? ScpStatus::Feasible : ScpStatus::NotFound, 0};
    }

    std::vector<uint64_t> rowGroupIndices(model.getTransitionMatrix().getRowGroupIndices().begin(), model.getTransitionMatrix().getRowGroupIndices().end());
    PropertyKind propertyKind = isReward ? PropertyKind::Reward : PropertyKind::Probability;
    ScpLpEncoder encoder(decomposition, rowGroupIndices, zeroStates, oneStates, infiniteStates, excludedRows, initialState, parameterBounds, threshold,
                         direction, propertyKind, options);

    storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType> checkTask(task->getFormula(), /*onlyInitialStatesRelevant=*/false);

    if constexpr (std::is_same_v<SparseModelType, storm::models::sparse::Mdp<ParametricType>>) {
        if (checkTask.isOptimizationDirectionSet()) {
            // An explicit Pmax/Pmin/Rmax/Rmin must agree with the direction implied by the threshold.
            STORM_LOG_THROW(checkTask.getOptimizationDirection() == schedulerDirection, storm::exceptions::NotSupportedException,
                            "SCP requires the formula's optimization direction (if explicitly given, e.g. Pmax=?/Pmin=?) to match the "
                            "robust reading implied by the threshold direction (AtLeast -> Pmin, AtMost -> Pmax); mismatched combinations "
                            "like 'Pmax >= lambda' are not supported.");
        } else {
            checkTask.setOptimizationDirection(schedulerDirection);
        }
        storm::modelchecker::SparseMdpInstantiationModelChecker<SparseModelType, ConstantType> instantiationChecker(env, model, /*produceScheduler=*/false);
        instantiationChecker.setInstantiationsAreGraphPreserving(true);
        instantiationChecker.specifyFormula(checkTask);
        return runLoop(env, instantiationChecker, encoder, bound, direction, initialState, initialValuation);
    } else {
        storm::modelchecker::SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType> instantiationChecker(env, model);
        instantiationChecker.setInstantiationsAreGraphPreserving(true);
        instantiationChecker.specifyFormula(checkTask);
        return runLoop(env, instantiationChecker, encoder, bound, direction, initialState, initialValuation);
    }
}

template<typename SparseModelType, typename ConstantType>
template<typename InstantiationChecker>
ScpResult<ConstantType> SparseScpInstantiationSearcher<SparseModelType, ConstantType>::runLoop(storm::Environment const& env,
                                                                                               InstantiationChecker& instantiationChecker,
                                                                                               ScpLpEncoder const& encoder, storm::logic::Bound const& bound,
                                                                                               ThresholdDirection direction, uint64_t initialState,
                                                                                               Valuation const& initialValuation) const {
    // Returns the model-checked value vector at ConstantType precision. Only the LP below uses double.
    auto modelCheck = [&](Valuation const& valuation) -> std::vector<ConstantType> {
        auto result = instantiationChecker.check(env, valuation);
        return result->template asExplicitQuantitativeCheckResult<ConstantType>().getFiniteValueVector();
    };
    auto toDouble = [](Valuation const& v) -> std::map<VariableType, double> {
        std::map<VariableType, double> result;
        for (auto const& [var, val] : v) {
            result[var] = storm::utility::convertNumber<double>(val);
        }
        return result;
    };
    auto toDoubleVec = [](std::vector<ConstantType> const& v) -> std::vector<double> {
        std::vector<double> result(v.size());
        for (uint64_t i = 0; i < v.size(); ++i) {
            result[i] = storm::utility::convertNumber<double>(v[i]);
        }
        return result;
    };

    Valuation vHat = initialValuation;
    std::vector<ConstantType> pHat = modelCheck(vHat);
    ConstantType betaStar = pHat.at(initialState);

    ScpResult<ConstantType> best{{betaStar, vHat}, ScpStatus::NotFound, 0};
    if (bound.isSatisfied(betaStar)) {
        best.status = ScpStatus::Feasible;
        return best;
    }

    double delta = options.trustRegionInitial;

    uint64_t iteration = 0;
    for (; iteration < options.maxIterations; ++iteration) {
        // Stops on --timeout, Ctrl-C and other termination requests.
        if (storm::utility::resources::isTerminate()) {
            // best.iterations keeps the iteration at which `best` was established.
            best.status = ScpStatus::Timeout;
            return best;
        }
        if (delta - 1.0 <= options.trustRegionMinExcess) {
            best.status = ScpStatus::NotFound;
            return best;
        }

        EncodedLp lp = encoder.encode(env, toDouble(vHat), toDoubleVec(pHat), delta);
        lp.solver->optimize();
        if (!lp.solver->isOptimal()) {
            delta = (delta - 1.0) / options.trustRegionFactor + 1.0;
            continue;
        }

        Valuation candidate = initialValuation;
        for (auto const& [var, lpVar] : lp.parameterVariables) {
            candidate[var] = storm::utility::convertNumber<CoefficientType>(lp.solver->getContinuousValue(lpVar));
        }

        std::vector<ConstantType> valueVector = modelCheck(candidate);
        ConstantType beta = valueVector.at(initialState);

        if (bound.isSatisfied(beta)) {
            return ScpResult<ConstantType>{{beta, candidate}, ScpStatus::Feasible, iteration + 1};
        }

        bool improved = (direction == ThresholdDirection::AtLeast) ? (beta > betaStar) : (beta < betaStar);
        if (improved) {
            vHat = candidate;
            pHat = std::move(valueVector);
            betaStar = beta;
            best = ScpResult<ConstantType>{{betaStar, vHat}, ScpStatus::NotFound, iteration + 1};
            delta = std::min(options.trustRegionMax, (delta - 1.0) * options.trustRegionFactor + 1.0);
        } else {
            delta = (delta - 1.0) / options.trustRegionFactor + 1.0;
        }
    }

    best.status = ScpStatus::MaxIterationsReached;
    return best;
}

template class SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double>;
template class SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double>;
// ConstantType = storm::RationalNumber: the checks and the reported value are exact; only the LP is double.
template class SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber>;
template class SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber>;

}  // namespace storm::pars::scp
