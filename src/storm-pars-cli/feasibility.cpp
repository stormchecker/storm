#include "storm-pars-cli/feasibility.h"
#include <optional>

#include "storm-cli-utilities/print.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/api/verification.h"

#include "storm/settings/SettingsManager.h"
#include "storm/utility/constants.h"

#include "storm-pars/api/region.h"
#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"
#include "storm-pars/derivative/GradientDescentMethod.h"
#include "storm-pars/settings/modules/DerivativeSettings.h"
#include "storm-pars/settings/modules/RegionVerificationSettings.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"

namespace storm::pars {

template<typename VT1>
void printFeasibilityResult(bool success,
                            std::pair<VT1, typename storm::storage::ParameterRegion<storm::RationalFunction>::Valuation> const& valueValuationPair,
                            storm::utility::Stopwatch const& watch) {
    std::stringstream valuationStr;
    bool first = true;
    for (auto const& v : valueValuationPair.second) {
        if (first) {
            first = false;
        } else {
            valuationStr << ", ";
        }
        valuationStr << v.first << "=" << v.second;
    }
    if (success) {
        STORM_PRINT_AND_LOG("Result at initial state: " << valueValuationPair.first << " ( approx. "
                                                        << storm::utility::convertNumber<double>(valueValuationPair.first) << ") at [" << valuationStr.str()
                                                        << "].\n");
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
void performFeasibility(std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
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
        runFeasibilityWithGD(model, task, omittedParameters, monotonicitySettings);
    } else if (feasibilitySettings.getFeasibilityMethod() == storm::pars::FeasibilityMethod::PLA) {
        runFeasibilityWithPLA(model, task, omittedParameters, monotonicitySettings);
    } else {
        STORM_LOG_ASSERT(feasibilitySettings.getFeasibilityMethod() == storm::pars::FeasibilityMethod::SCP, "Remaining method must be SCP.");
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "SCP is not yet implemented.");
    }
}

template<typename ValueType>
void runFeasibilityWithGD(std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
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
        // Check if the region includes bounds at 0 or 1
        bool hasZeroBound = false;
        for (auto const& var : region->getVariables()) {
            auto lowerBound = region->getLowerBoundary(var);
            auto upperBound = region->getUpperBoundary(var);
            if (storm::utility::isZero(lowerBound) || storm::utility::isOne(upperBound)) {
                hasZeroBound = true;
                break;
            }
        }
        if (hasZeroBound) {
            STORM_LOG_WARN(
                "The region includes bounds at 0 or 1, which is not supported by Gradient Descent. Continuing anyway, but results may be incorrect.");
        }
    }

    STORM_PRINT("Finding an extremum using Gradient Descent\n");
    storm::utility::Stopwatch derivativeWatch(true);
    storm::derivative::GradientDescentInstantiationSearcher<storm::RationalFunction, double> gdsearch(
        *dtmc, *method, derSettings.getLearningRate(), derSettings.getAverageDecay(), derSettings.getSquaredAverageDecay(), derSettings.getMiniBatchSize(),
        derSettings.getTerminationEpsilon(), startPoint, *constraintMethod, region, derSettings.isPrintJsonSet());

    gdsearch.setup(Environment(), task);
    auto instantiationAndValue = gdsearch.gradientDescent();
    // TODO check what happens if no feasible solution is found
    if (!derSettings.areInconsequentialParametersOmitted() && omittedParameters) {
        for (RationalFunctionVariable const& param : *omittedParameters) {
            if (startPoint) {
                instantiationAndValue.first[param] = startPoint->at(param);
            } else {
                instantiationAndValue.first[param] = utility::convertNumber<RationalFunction::CoeffType>(0.5);
            }
        }
    }
    derivativeWatch.stop();

    // TODO refactor this such that the order is globally fixed.
    std::pair<double, typename storm::storage::ParameterRegion<ValueType>::Valuation> valueValuationPair;
    valueValuationPair.first = instantiationAndValue.second;
    valueValuationPair.second = instantiationAndValue.first;

    if (derSettings.isPrintJsonSet()) {
        gdsearch.printRunAsJson();
    }

    if (task->isBoundSet()) {
        printFeasibilityResult(task->getBound().isSatisfied(valueValuationPair.first), valueValuationPair, derivativeWatch);
    } else {
        printFeasibilityResult(true, valueValuationPair, derivativeWatch);
    }
}

template<typename ValueType>
void runFeasibilityWithPLA(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                           std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                           boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings) {
    STORM_LOG_THROW(task->isRegionSet(), storm::exceptions::NotSupportedException, "PLA requires an explicitly given region.");
    storm::solver::OptimizationDirection direction = task->getOptimizationDirection();

    // TODO handle omittedParameterss
    auto regionVerificationSettings = storm::settings::getModule<storm::settings::modules::RegionVerificationSettings>();
    auto engine = regionVerificationSettings.getRegionCheckEngine();

    auto regionSplittingStrategy = storm::modelchecker::RegionSplittingStrategy();

    regionSplittingStrategy.heuristic = regionVerificationSettings.getRegionSplittingHeuristic();
    regionSplittingStrategy.estimateKind = regionVerificationSettings.getRegionSplittingEstimateMethod();
    if (regionVerificationSettings.isSplittingThresholdSet()) {
        regionSplittingStrategy.maxSplitDimensions = regionVerificationSettings.getSplittingThreshold();
    }

    if (task->isBoundSet()) {
        storm::utility::Stopwatch watch(true);
        auto const& settings = storm::api::RefinementOptions<ValueType>{model, storm::api::createTask<ValueType>(task->getFormula().asSharedPointer(), true),
                                                                        engine, regionSplittingStrategy};
        auto valueValuation = storm::api::computeExtremalValue<ValueType>(settings, task->getRegion(), direction, storm::utility::zero<ValueType>(),
                                                                          !task->isMaxGapRelative(), task->getBound().getInvertedBound());
        watch.stop();

        printFeasibilityResult(task->getBound().isSatisfied(valueValuation.first), valueValuation, watch);
    } else {
        STORM_LOG_THROW(task->getMaximalAllowedGap() != std::nullopt, storm::exceptions::NotSupportedException,
                        "Without a bound, PLA requires an explicit target in form of a guarantee.");

        ValueType precision = storm::utility::convertNumber<ValueType>(task->getMaximalAllowedGap().value());
        storm::utility::Stopwatch watch(true);
        auto const& settings = storm::api::RefinementOptions<ValueType>{model, storm::api::createTask<ValueType>(task->getFormula().asSharedPointer(), true),
                                                                        engine, regionSplittingStrategy};
        auto valueValuation = storm::api::computeExtremalValue<ValueType>(settings, task->getRegion(), direction, storm::utility::zero<ValueType>(),
                                                                          !task->isMaxGapRelative(), std::nullopt);
        watch.stop();

        printFeasibilityResult(true, valueValuation, watch);
    }
}

template void performFeasibility(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model,
                                 std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                 boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings);

template void runFeasibilityWithGD(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model,
                                   std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                   boost::optional<std::set<RationalFunctionVariable>> omittedParameters, storm::api::MonotonicitySetting monotonicitySettings);

template void runFeasibilityWithPLA(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model,
                                    std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                                    boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                                    storm::api::MonotonicitySetting monotonicitySettings);

}  // namespace storm::pars