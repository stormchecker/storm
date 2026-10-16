#include "storm-pars/api/synthesis.h"

#include <algorithm>
#include <set>
#include <utility>

#include "storm-pars/analysis/RegionConstraintChecker.h"
#include "storm-pars/api/region.h"
#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"
#include "storm-pars/modelchecker/instantiation/SparseDtmcInstantiationModelChecker.h"
#include "storm-pars/scp/SparseScpInstantiationSearcher.h"
#include "storm/api/verification.h"
#include "storm/exceptions/AbortException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/constants.h"

namespace storm::api {
namespace {

using Model = storm::models::sparse::Model<storm::RationalFunction>;
using Dtmc = storm::models::sparse::Dtmc<storm::RationalFunction>;
using Mdp = storm::models::sparse::Mdp<storm::RationalFunction>;
using Region = storm::storage::ParameterRegion<storm::RationalFunction>;
using Coefficient = Region::CoefficientType;
using Engine = storm::modelchecker::RegionCheckEngine;
using Status = storm::modelchecker::RegionResult;
template<typename ConstantType>
using Result = storm::pars::FeasibilitySynthesisResult<ConstantType>;
using GraphStatus = storm::pars::analysis::GraphPreservationStatus;

void validateInput(std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
                   std::optional<Engine> const& regionEngine = std::nullopt) {
    STORM_LOG_THROW(model && formula, storm::exceptions::InvalidArgumentException, "A model and formula are required.");
    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc) || model->isOfType(storm::models::ModelType::Mdp), storm::exceptions::NotSupportedException,
                    "Parameter synthesis supports sparse DTMCs and MDPs only.");
    STORM_LOG_THROW(model->getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "Parameter synthesis requires exactly one initial state.");
    STORM_LOG_THROW((formula->isProbabilityOperatorFormula() || formula->isRewardOperatorFormula()) && formula->asOperatorFormula().hasBound(),
                    storm::exceptions::NotSupportedException, "Parameter synthesis requires a bounded probability or reward operator formula.");
    if (regionEngine && *regionEngine != Engine::RobustParameterLifting) {
        STORM_LOG_THROW(storm::utility::parameterlifting::validateParameterLiftingSound(*model, *formula, *regionEngine),
                        storm::exceptions::NotSupportedException, "Could not establish that parameter lifting is sound for this model and formula.");
    }
    if (regionEngine == Engine::RobustParameterLifting) {
        STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException, "Robust PLA supports DTMCs only.");
        auto const& subformula = formula->asOperatorFormula().getSubformula();
        STORM_LOG_THROW(!subformula.isBoundedUntilFormula() && !subformula.isCumulativeRewardFormula(), storm::exceptions::NotSupportedException,
                        "Robust PLA does not support step-bounded probability or cumulative reward formulas.");
        if (formula->isRewardOperatorFormula()) {
            auto const& rewardFormula = formula->asRewardOperatorFormula();
            auto const& rewardModel =
                rewardFormula.hasRewardModelName() ? model->getRewardModel(rewardFormula.getRewardModelName()) : model->getUniqueRewardModel();
            for (auto const& reward : rewardModel.getTotalRewardVector(model->getTransitionMatrix())) {
                STORM_LOG_THROW(storm::utility::isConstant(reward), storm::exceptions::NotSupportedException,
                                "Robust PLA does not support nonconstant total rewards.");
            }
        }
    }
}

std::pair<Region, GraphStatus> resolveRegion(Model const& model, std::optional<Region> const& region, std::optional<Coefficient> const& regionBound,
                                             bool requireGraphPreservation) {
    STORM_LOG_THROW(region.has_value() != regionBound.has_value(), storm::exceptions::InvalidArgumentException,
                    "Provide exactly one of region or region bound.");
    auto const parameters = getModelParameters<storm::RationalFunction>(model);
    Region resolved;
    if (region) {
        resolved = *region;
    } else {
        Coefficient const one = storm::utility::one<Coefficient>();
        STORM_LOG_THROW(*regionBound >= storm::utility::zero<Coefficient>() && *regionBound <= one / 2, storm::exceptions::InvalidArgumentException,
                        "The region bound must be between 0 and 1/2, inclusive.");
        Region::Valuation lower, upper;
        for (auto const& parameter : parameters) {
            lower.emplace(parameter, *regionBound);
            upper.emplace(parameter, one - *regionBound);
        }
        resolved = Region(std::move(lower), std::move(upper));
    }
    for (auto const& variable : parameters) {
        STORM_LOG_THROW(resolved.getVariables().count(variable), storm::exceptions::InvalidArgumentException,
                        "The region does not cover model parameter '" << variable.name() << "'.");
    }
    if (!requireGraphPreservation) {
        return {std::move(resolved), GraphStatus::None};
    }
    storm::analysis::ConstraintCollector collector(model);
    storm::pars::analysis::RegionConstraintChecker checker(collector);
    STORM_LOG_THROW(checker.isGraphPreserving(resolved), storm::exceptions::NotSupportedException,
                    "This engine requires a well-defined, graph-preserving region. Provide an explicit graph-preserving region or use robust PLA for a DTMC. "
                    "The region's endpoints are not removed automatically.");
    return {std::move(resolved), GraphStatus::GraphPreserving};
}

std::shared_ptr<storm::logic::Formula> normalizedFormula(Model const& model, storm::logic::Formula const& formula) {
    auto result = formula.clone();
    auto& op = result->asOperatorFormula();
    // Removing the bound must not lose the default universal scheduler quantification of a threshold property.
    if (model.isOfType(storm::models::ModelType::Mdp) && !op.hasOptimalityType()) {
        op.setOptimalityType(op.getBound().isLowerBound() ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize);
    }
    return result;
}

std::shared_ptr<storm::logic::Formula> quantitativeFormula(Model const& model, storm::logic::Formula const& formula) {
    auto result = normalizedFormula(model, formula);
    result->asOperatorFormula().removeBound();
    return result;
}

std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> createFeasibilityTask(Model const& model, storm::logic::Formula const& formula,
                                                                                   Region const& region) {
    auto task = std::make_shared<storm::pars::FeasibilitySynthesisTask>(quantitativeFormula(model, formula));
    task->setBound(formula.asOperatorFormula().getBound());
    task->setRegion(region);
    return task;
}

bool verifyImpl(storm::Environment const& env, std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
                std::optional<Region> const& region, std::optional<Coefficient> const& regionBound, Engine engine) {
    validateInput(model, formula, engine);
    auto const [consideredRegion, graphStatus] = resolveRegion(*model, region, regionBound, engine != Engine::RobustParameterLifting);
    auto normalized = normalizedFormula(*model, *formula);
    std::set<Region::VariableType> discreteVariables;
    RefinementOptions<storm::RationalFunction> settings(model, *normalized, engine, storm::modelchecker::RegionSplittingStrategy(), MonotonicitySetting(),
                                                        discreteVariables, true, graphStatus);
    return verifyRegion(env, settings, consideredRegion);
}

Result<double> checkFeasibilityImpl(storm::Environment const& env, std::shared_ptr<Model> const& model,
                                    std::shared_ptr<storm::logic::Formula const> const& formula, std::optional<Region> const& region,
                                    std::optional<Coefficient> const& regionBound, storm::pars::FeasibilityMethod method, Engine regionEngine) {
    try {
        switch (method) {
            case storm::pars::FeasibilityMethod::PLA: {
                validateInput(model, formula, regionEngine);
                auto const [consideredRegion, graphStatus] = resolveRegion(*model, region, regionBound, regionEngine != Engine::RobustParameterLifting);
                auto normalized = normalizedFormula(*model, *formula);
                std::set<Region::VariableType> discreteVariables;
                RefinementOptions<storm::RationalFunction> settings(model, *normalized, regionEngine, storm::modelchecker::RegionSplittingStrategy(),
                                                                    MonotonicitySetting(), discreteVariables, true, graphStatus);
                auto checker = initializeRegionRefinementChecker(env, settings);
                auto const& bound = normalized->asOperatorFormula().getBound();
                auto acceptGlobalBound = [&](auto const&, auto const& globalBound) { return !bound.isSatisfied(globalBound); };
                auto rejectInstance = [&](auto const& value) { return bound.isSatisfied(value); };
                auto direction = bound.isLowerBound() ? storm::solver::OptimizationDirection::Maximize : storm::solver::OptimizationDirection::Minimize;
                auto result = checker->computeExtremalValueHelper(env, consideredRegion, direction, acceptGlobalBound, rejectInstance);
                if (bound.isSatisfied(result.first)) {
                    return {Status::ExistsSat,
                            storm::pars::FeasibilityResult<double>{storm::utility::convertNumber<double>(result.first), std::move(result.second)}};
                }
                return {Status::AllViolated, std::nullopt};
            }
            case storm::pars::FeasibilityMethod::GD: {
                validateInput(model, formula);
                STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException,
                                "Gradient descent supports DTMCs only.");
                auto const [consideredRegion, graphStatus] = resolveRegion(*model, region, regionBound, true);
                auto const& gdRegion = consideredRegion;
                if (std::all_of(consideredRegion.getVariables().begin(), consideredRegion.getVariables().end(),
                                [&gdRegion](auto const& variable) { return storm::utility::isZero(gdRegion.getDifference(variable)); })) {
                    auto valuation = consideredRegion.getCenterPoint();
                    storm::modelchecker::SparseDtmcInstantiationModelChecker<Dtmc, double> checker(env, *model->as<Dtmc>());
                    checker.specifyFormula(createTask<storm::RationalFunction>(quantitativeFormula(*model, *formula), true));
                    auto value = checker.check(env, valuation)->asExplicitQuantitativeCheckResult<double>()[model->getInitialStates().getNextSetIndex(0)];
                    if (!formula->asOperatorFormula().getBound().isSatisfied(value)) {
                        return {Status::AllViolated, std::nullopt};
                    }
                    return {Status::ExistsSat, storm::pars::FeasibilityResult<double>{value, std::move(valuation)}};
                }
                auto task = createFeasibilityTask(*model, *formula, consideredRegion);
                storm::derivative::GradientDescentInstantiationSearcher<storm::RationalFunction, double> searcher(
                    env, *model->as<Dtmc>(), storm::derivative::GradientDescentMethod::ADAM, 0.1, 0.9, 0.999, 32, 1e-6, consideredRegion.getCenterPoint(),
                    storm::derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT, consideredRegion);
                searcher.setup(env, task);
                auto result = searcher.gradientDescent();
                if (!formula->asOperatorFormula().getBound().isSatisfied(result.value)) {
                    return {};
                }
                return {Status::ExistsSat, std::move(result)};
            }
            case storm::pars::FeasibilityMethod::SCP: {
                validateInput(model, formula);
                auto const& op = formula->asOperatorFormula();
                if (model->isOfType(storm::models::ModelType::Mdp) && op.hasOptimalityType()) {
                    auto schedulerDirection =
                        op.getBound().isLowerBound() ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
                    STORM_LOG_THROW(op.getOptimalityType() == schedulerDirection, storm::exceptions::NotSupportedException,
                                    "SCP requires universal scheduler quantification on MDPs: lower bounds use min and upper bounds use max.");
                }
                auto const [consideredRegion, graphStatus] = resolveRegion(*model, region, regionBound, true);
                auto task = createFeasibilityTask(*model, *formula, consideredRegion);
                storm::pars::scp::ScpResult<double> result;
                if (model->isOfType(storm::models::ModelType::Dtmc)) {
                    result = storm::pars::scp::SparseScpInstantiationSearcher<Dtmc, double>(*model->as<Dtmc>()).run(env, task);
                } else {
                    STORM_LOG_ASSERT(model->isOfType(storm::models::ModelType::Mdp), "Expected an MDP.");
                    result = storm::pars::scp::SparseScpInstantiationSearcher<Mdp, double>(*model->as<Mdp>()).run(env, task);
                }
                if (result.status != storm::pars::scp::ScpStatus::Feasible) {
                    return {};
                }
                return {Status::ExistsSat, storm::pars::FeasibilityResult<double>{std::move(result.value), std::move(result.valuation)}};
            }
        }
        STORM_LOG_THROW_UNCONDITIONALLY(storm::exceptions::InvalidArgumentException, "Unknown feasibility method.");
    } catch (storm::exceptions::AbortException const&) {
        return {};
    }
}

}  // namespace

bool verify(storm::Environment const& env, std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
            Region const& region, Engine engine) {
    return verifyImpl(env, model, formula, region, std::nullopt, engine);
}

bool verify(storm::Environment const& env, std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
            Coefficient const& regionBound, Engine engine) {
    return verifyImpl(env, model, formula, std::nullopt, regionBound, engine);
}

Result<double> checkFeasibility(storm::Environment const& env, std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
                                Region const& region, storm::pars::FeasibilityMethod method, Engine regionEngine) {
    return checkFeasibilityImpl(env, model, formula, region, std::nullopt, method, regionEngine);
}

Result<double> checkFeasibility(storm::Environment const& env, std::shared_ptr<Model> const& model, std::shared_ptr<storm::logic::Formula const> const& formula,
                                Coefficient const& regionBound, storm::pars::FeasibilityMethod method, Engine regionEngine) {
    return checkFeasibilityImpl(env, model, formula, std::nullopt, regionBound, method, regionEngine);
}

}  // namespace storm::api
