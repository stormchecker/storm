#include "storm-pars-cli/feasibility.h"
#include <optional>

#include "storm-cli-utilities/print.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/api/verification.h"

#include "storm/settings/SettingsManager.h"
#include "storm/utility/constants.h"

#include "storm-pars-cli/graphPreservation.h"
#include "storm-pars/api/region.h"
#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"
#include "storm-pars/derivative/GradientDescentMethod.h"
#include "storm-pars/scp/SparseScpInstantiationSearcher.h"
#include "storm-pars/settings/modules/DerivativeSettings.h"
#include "storm-pars/settings/modules/RegionVerificationSettings.h"
#include "storm-pars/settings/modules/ScpSettings.h"
#include "storm-pars/utility/FeasibilityResult.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

namespace storm::pars {

template<typename ConstantType>
void printFeasibilityResult(bool success, storm::pars::FeasibilityResult<ConstantType> const& result, storm::utility::Stopwatch const& watch) {
    std::stringstream valuationStr;
    bool first = true;
    for (auto const& v : result.valuation) {
        if (first) {
            first = false;
        } else {
            valuationStr << ", ";
        }
        valuationStr << v.first << "=" << v.second;
    }
    if (success) {
        STORM_PRINT_AND_LOG("Result at initial state: " << result.value << " ( approx. " << storm::utility::convertNumber<double>(result.value) << ") at ["
                                                        << valuationStr.str() << "].\n");
    } else {
        STORM_PRINT_AND_LOG("No satisfying result found.\n");
    }
    STORM_PRINT_AND_LOG("Time for model checking: " << watch << ".\n");
}

std::shared_ptr<FeasibilitySynthesisTask const> createFeasibilitySynthesisTaskFromSettings(
    std::shared_ptr<storm::logic::Formula const> const& formula, std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> const& regions) {
    STORM_LOG_THROW(formula->isRewardOperatorFormula() || formula->isProbabilityOperatorFormula(), storm::exceptions::NotSupportedException,
                    "We only support reward- and probability operator formulas.");
    STORM_LOG_THROW(regions.size() <= 1, storm::exceptions::NotSupportedException, "Storm only supports one or zero regions.");

    std::shared_ptr<storm::logic::Formula const> formulaNoBound;
    if (formula->asOperatorFormula().hasBound()) {
        std::shared_ptr<storm::logic::Formula> formulaWithoutBounds = formula->clone();
        formulaWithoutBounds->asOperatorFormula().removeBound();
        formulaNoBound = formulaWithoutBounds->asSharedPointer();
    } else {
        formulaNoBound = formula;
    }

    FeasibilitySynthesisTask t(formulaNoBound);
    auto const& feasibilitySettings = storm::settings::getModule<storm::settings::modules::FeasibilitySettings>();

    if (formula->asOperatorFormula().hasBound()) {
        t.setBound(formula->asOperatorFormula().getBound());

        STORM_LOG_THROW(!feasibilitySettings.isParameterDirectionSet(), storm::exceptions::NotSupportedException,
                        "With a bound, the direction for the parameters is inferred from the bound.");
        STORM_LOG_THROW(!feasibilitySettings.hasOptimalValueGuaranteeBeenSet(), storm::exceptions::NotSupportedException,
                        "When a bound is given, the guarantee is that this bound will be satisfied by a solution.");
    } else {
        if (feasibilitySettings.hasOptimalValueGuaranteeBeenSet()) {
            t.setMaximalAllowedGap(storm::utility::convertNumber<storm::RationalNumber>(feasibilitySettings.getOptimalValueGuarantee()));
            t.setMaximalAllowedGapIsRelative(!feasibilitySettings.isAbsolutePrecisionSet());
        }
        STORM_LOG_THROW(feasibilitySettings.isParameterDirectionSet(), storm::exceptions::NotSupportedException,
                        "Without a bound, the direction for the parameters must be explicitly given.");
        t.setOptimizationDirection(feasibilitySettings.getParameterDirection());
    }
    if (regions.size() == 1) {
        t.setRegion(regions.front());
    }
    return std::make_shared<FeasibilitySynthesisTask const>(std::move(t));
}

template<typename ValueType>
void performFeasibility(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                        std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                        boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings) {
    auto const& feasibilitySettings = storm::settings::getModule<storm::settings::modules::FeasibilitySettings>();

    STORM_PRINT_AND_LOG("Find feasible solution for  " << task->getFormula());
    if (task->isRegionSet()) {
        STORM_PRINT_AND_LOG(" within region " << task->getRegion());
    }
    if (monotonicitySettings.useMonotonicity) {
        STORM_PRINT_AND_LOG(" and using monotonicity ...");
    }
    STORM_PRINT_AND_LOG("\n");

    if (feasibilitySettings.getFeasibilityMethod() == storm::pars::FeasibilityMethod::GD) {
        runFeasibilityWithGD(env, model, task, omittedParameters, monotonicitySettings);
    } else if (feasibilitySettings.getFeasibilityMethod() == storm::pars::FeasibilityMethod::PLA) {
        runFeasibilityWithPLA(env, model, task, omittedParameters, monotonicitySettings);
    } else {
        STORM_LOG_ASSERT(feasibilitySettings.getFeasibilityMethod() == storm::pars::FeasibilityMethod::SCP, "Remaining method must be SCP.");
        runFeasibilityWithSCP(env, model, task, omittedParameters, monotonicitySettings);
    }
}

template<typename ValueType>
void runFeasibilityWithGD(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                          std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                          boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings) {
    auto derSettings = storm::settings::getModule<storm::settings::modules::DerivativeSettings>();

    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException,
                    "Gradient descent is currently only supported for DTMCs.");
    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> dtmc = model->template as<storm::models::sparse::Dtmc<ValueType>>();
    STORM_LOG_THROW(task->getFormula().isProbabilityOperatorFormula() || task->getFormula().isRewardOperatorFormula(), storm::exceptions::NotSupportedException,
                    "Input formula needs to be either a probability operator formula or a reward operator formula.");
    STORM_LOG_THROW(task->isBoundSet(), storm::exceptions::NotImplementedException, "GD (right now) requires an explicitly given bound.");
    STORM_LOG_THROW(task->getMaximalAllowedGap() == std::nullopt, storm::exceptions::NotSupportedException,
                    "GD cannot provide guarantees on the optimality of the solution..");

    if (omittedParameters && !omittedParameters->empty()) {
        // TODO get rid of std::cout here
        std::cout << "Parameters ";
        for (auto const& entry : *omittedParameters) {
            std::cout << entry << " ";
        }
        std::cout << "are inconsequential.";
        if (derSettings.areInconsequentialParametersOmitted()) {
            std::cout << " They will be omitted in the found instantiation.\n";
        } else {
            std::cout << " They will be set to 0.5 in the found instantiation. To omit them, set the flag --omit-inconsequential-params.\n";
        }
    }

    boost::optional<derivative::GradientDescentConstraintMethod> constraintMethod = derSettings.getConstraintMethod();
    if (!constraintMethod) {
        STORM_LOG_ERROR("Unknown Gradient Descent Constraint method: " << derSettings.getConstraintMethodAsString());
        return;
    }

    boost::optional<derivative::GradientDescentMethod> method = derSettings.getGradientDescentMethod();
    if (!method) {
        STORM_LOG_ERROR("Unknown Gradient Descent method: " << derSettings.getGradientDescentMethodAsString());
        return;
    }

    std::optional<std::map<typename utility::parametric::VariableType<ValueType>::type, typename utility::parametric::CoefficientType<ValueType>::type>>
        startPoint;

    std::optional<storage::ParameterRegion<storm::RationalFunction>> region;
    if (task->isRegionSet()) {
        region = task->getRegion();
        // Gradient descent requires a graph-preserving region.
        establishGraphPreservation<ValueType>(model, {*region}, true);
    }

    STORM_PRINT("Finding an extremum using Gradient Descent\n");
    storm::utility::Stopwatch derivativeWatch(true);
    storm::derivative::GradientDescentInstantiationSearcher<storm::RationalFunction, double> gdsearch(
        env, *dtmc, *method, derSettings.getLearningRate(), derSettings.getAverageDecay(), derSettings.getSquaredAverageDecay(), derSettings.getMiniBatchSize(),
        derSettings.getTerminationEpsilon(), startPoint, *constraintMethod, region, derSettings.isPrintJsonSet());

    gdsearch.setup(env, task);
    auto result = gdsearch.gradientDescent();
    // TODO check what happens if no feasible solution is found
    if (!derSettings.areInconsequentialParametersOmitted() && omittedParameters) {
        for (RationalFunctionVariable const& param : *omittedParameters) {
            if (startPoint) {
                result.valuation[param] = startPoint->at(param);
            } else {
                result.valuation[param] = utility::convertNumber<RationalFunction::CoeffType>(0.5);
            }
        }
    }
    derivativeWatch.stop();

    if (derSettings.isPrintJsonSet()) {
        gdsearch.printRunAsJson();
    }

    if (task->isBoundSet()) {
        printFeasibilityResult(task->getBound().isSatisfied(result.value), result, derivativeWatch);
    } else {
        printFeasibilityResult(true, result, derivativeWatch);
    }
}

template<typename ValueType>
void runFeasibilityWithPLA(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                           std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                           boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings) {
    STORM_LOG_THROW(task->isRegionSet(), storm::exceptions::NotSupportedException, "PLA requires an explicitly given region.");
    storm::solver::OptimizationDirection direction = task->getOptimizationDirection();

    // TODO handle omittedParameterss
    auto regionVerificationSettings = storm::settings::getModule<storm::settings::modules::RegionVerificationSettings>();
    auto engine = regionVerificationSettings.getRegionCheckEngine();

    auto const graphPreservationStatus =
        establishGraphPreservation<ValueType>(model, {task->getRegion()}, engine != storm::modelchecker::RegionCheckEngine::RobustParameterLifting);

    auto regionSplittingStrategy = storm::modelchecker::RegionSplittingStrategy();

    regionSplittingStrategy.heuristic = regionVerificationSettings.getRegionSplittingHeuristic();
    regionSplittingStrategy.estimateKind = regionVerificationSettings.getRegionSplittingEstimateMethod();
    if (regionVerificationSettings.isSplittingThresholdSet()) {
        regionSplittingStrategy.maxSplitDimensions = regionVerificationSettings.getSplittingThreshold();
    }

    if (task->isBoundSet()) {
        storm::utility::Stopwatch watch(true);
        auto const& settings = storm::api::RefinementOptions<ValueType>{model,
                                                                        storm::api::createTask<ValueType>(task->getFormula().asSharedPointer(), true),
                                                                        engine,
                                                                        regionSplittingStrategy,
                                                                        storm::api::MonotonicitySetting(),
                                                                        {},
                                                                        true,
                                                                        graphPreservationStatus};
        auto valueValuation = storm::api::computeExtremalValue<ValueType>(env, settings, task->getRegion(), direction, storm::utility::zero<ValueType>(),
                                                                          !task->isMaxGapRelative(), task->getBound().getInvertedBound());
        watch.stop();

        printFeasibilityResult(task->getBound().isSatisfied(valueValuation.value), valueValuation, watch);
    } else {
        STORM_LOG_THROW(task->getMaximalAllowedGap() != std::nullopt, storm::exceptions::NotSupportedException,
                        "Without a bound, PLA requires an explicit target in form of a guarantee.");

        ValueType precision = storm::utility::convertNumber<ValueType>(task->getMaximalAllowedGap().value());
        storm::utility::Stopwatch watch(true);
        auto const& settings = storm::api::RefinementOptions<ValueType>{model,
                                                                        storm::api::createTask<ValueType>(task->getFormula().asSharedPointer(), true),
                                                                        engine,
                                                                        regionSplittingStrategy,
                                                                        storm::api::MonotonicitySetting(),
                                                                        {},
                                                                        true,
                                                                        graphPreservationStatus};
        auto valueValuation = storm::api::computeExtremalValue<ValueType>(env, settings, task->getRegion(), direction, storm::utility::zero<ValueType>(),
                                                                          !task->isMaxGapRelative(), std::nullopt);
        watch.stop();

        printFeasibilityResult(true, valueValuation, watch);
    }
}

template<typename ValueType>
void runFeasibilityWithSCP(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                           std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                           boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings) {
    // Formula and bound checks are done by SparseScpInstantiationSearcher::run().
    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Mdp) || model->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException,
                    "SCP is only supported for MDPs and DTMCs.");
    // TODO handle omittedParameters.
    STORM_LOG_THROW(task->isRegionSet(), storm::exceptions::NotSupportedException, "SCP requires an explicitly given region.");
    // SCP requires a graph-preserving region.
    establishGraphPreservation<ValueType>(model, {task->getRegion()}, true);

    auto const& scpSettings = storm::settings::getModule<storm::settings::modules::ScpSettings>();
    storm::pars::scp::ScpOptions options;
    options.trustRegionInitial = scpSettings.getTrustRegionInitial();
    options.trustRegionFactor = scpSettings.getTrustRegionFactor();
    options.trustRegionMax = scpSettings.getTrustRegionMax();
    options.trustRegionMinExcess = scpSettings.getTrustRegionMinExcess();
    options.maxIterations = scpSettings.getMaxIterations();
    options.initialStateBias = scpSettings.getInitialStateBias();
    options.useMaxViolationObjective = !scpSettings.isSumViolationObjectiveSet();

    auto run = [&]<typename ConstantType>() {
        storm::utility::Stopwatch watch(true);
        storm::pars::scp::ScpResult<ConstantType> result;
        if (model->isOfType(storm::models::ModelType::Mdp)) {
            auto mdp = model->template as<storm::models::sparse::Mdp<ValueType>>();
            result = storm::pars::scp::SparseScpInstantiationSearcher<storm::models::sparse::Mdp<ValueType>, ConstantType>(*mdp, options).run(env, task);
        } else {
            auto dtmc = model->template as<storm::models::sparse::Dtmc<ValueType>>();
            result = storm::pars::scp::SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<ValueType>, ConstantType>(*dtmc, options).run(env, task);
        }
        watch.stop();
        printFeasibilityResult(result.status == storm::pars::scp::ScpStatus::Feasible, result, watch);
    };
    if (scpSettings.isExactSet()) {
        run.template operator()<storm::RationalNumber>();
    } else {
        run.template operator()<double>();
    }
}

template void performFeasibility(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model,
                                 std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                 boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings);

template void runFeasibilityWithGD(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model,
                                   std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                   boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings);

template void runFeasibilityWithPLA(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
                                    std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                    boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                                    storm::api::MonotonicitySetting monotonicitySettings);

template void runFeasibilityWithSCP(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model,
                                    std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                    boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                                    storm::api::MonotonicitySetting monotonicitySettings);

}  // namespace storm::pars
