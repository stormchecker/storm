#include "storm/settings/EnvironmentBuilder.h"

#include "storm/environment/dd/AllDdEnvironments.h"
#include "storm/environment/exploration/ExplorationEnvironment.h"
#include "storm/environment/modelchecker/AllModelCheckerEnvironments.h"
#include "storm/environment/solver/AllSolverEnvironments.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ConditionalSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/CuddSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/settings/modules/ExplorationSettings.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/GlpkSettings.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/settings/modules/GurobiSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/LongRunAverageSolverSettings.h"
#include "storm/settings/modules/MinMaxEquationSolverSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/settings/modules/MultiplierSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/settings/modules/OviSolverSettings.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/settings/modules/TimeBoundedSolverSettings.h"
#include "storm/settings/modules/TopologicalEquationSolverSettings.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace {

void applyPrecision(auto& environment, double precision) {
    environment.setPrecision(storm::utility::convertNumber<storm::RationalNumber>(precision));
}

void applyPrecision(auto& environment, double precision, bool setFromDefault) {
    environment.setPrecision(storm::utility::convertNumber<storm::RationalNumber>(precision), setFromDefault);
}

void applyIfSet(bool isSet, auto&& apply) {
    if (isSet) {
        apply();
    }
}

}  // namespace

namespace storm {
namespace settings {

storm::Environment EnvironmentBuilder::buildEnvironment() {
    storm::Environment env;

    auto const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    env.setModelTolerance(generalSettings.getPrecision());

    setSolverEnvironment(env.solver());
    setModelcheckerEnvironment(env.modelchecker());
    setDdEnvironment(env.dd());
    setExplorationEnvironment(env.exploration());

    return env;
}

void EnvironmentBuilder::setSolverEnvironment(storm::SolverEnvironment& solver) {
    auto const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    auto const& coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
    auto const& debugSettings = storm::settings::getModule<storm::settings::modules::DebugSettings>();

    solver.setForceSoundness(generalSettings.isSoundSet());
    solver.setForceExact(generalSettings.isExactSet() || generalSettings.isExactFinitePrecisionSet());
    solver.setDebug(debugSettings.isDebugSet());
    solver.setVerbose(generalSettings.isVerboseSet());
    solver.setShowProgressDelay(generalSettings.getShowProgressDelay());
    solver.setLinearEquationSolverType(coreSettings.getEquationSolver(), coreSettings.isEquationSolverSetFromDefaultValue());
    solver.setLpSolverType(coreSettings.getLpSolver(), coreSettings.isLpSolverSetFromDefaultValue());

    auto const& eigenSettings = storm::settings::getModule<storm::settings::modules::EigenEquationSolverSettings>();
    auto& eigen = solver.eigen();
    eigen.setMethod(eigenSettings.getLinearEquationSystemMethod(), eigenSettings.isLinearEquationSystemMethodSetFromDefault());
    eigen.setPreconditioner(eigenSettings.getPreconditioningMethod());
    eigen.setRestartThreshold(eigenSettings.getRestartIterationCount());
    applyIfSet(eigenSettings.isMaximalIterationCountSet(), [&]() { eigen.setMaximalNumberOfIterations(eigenSettings.getMaximalIterationCount()); });
    applyPrecision(eigen, eigenSettings.getPrecision());

    auto const& eliminationSettings = storm::settings::getModule<storm::settings::modules::EliminationSettings>();
    auto& elimination = solver.elimination();
    elimination.setOrder(eliminationSettings.getEliminationOrder());
    elimination.setMethod(eliminationSettings.getEliminationMethod());
    elimination.setMaximalSccSize(eliminationSettings.getMaximalSccSize());
    elimination.setEliminateEntryStatesLast(eliminationSettings.isEliminateEntryStatesLastSet());

    auto const& gameSettings = storm::settings::getModule<storm::settings::modules::GameSolverSettings>();
    auto& game = solver.game();
    game.setMethod(gameSettings.getGameSolvingMethod(), gameSettings.isGameSolvingMethodSetFromDefaultValue());
    applyIfSet(gameSettings.isMaximalIterationCountSet(), [&]() { game.setMaximalNumberOfIterations(gameSettings.getMaximalIterationCount()); });
    applyPrecision(game, gameSettings.getPrecision());
    game.setRelativeTerminationCriterion(gameSettings.getConvergenceCriterion() ==
                                         storm::settings::modules::GameSolverSettings::ConvergenceCriterion::Relative);

    auto const& glpkSettings = storm::settings::getModule<storm::settings::modules::GlpkSettings>();
    auto& glpk = solver.glpk();
    glpk.setIntegerTolerance(glpkSettings.getIntegerTolerance());
    glpk.setMILPPresolverEnabled(glpkSettings.isMILPPresolverEnabled());
    glpk.setOutput(glpkSettings.isOutputSet());

    auto const& gmmxxSettings = storm::settings::getModule<storm::settings::modules::GmmxxEquationSolverSettings>();
    auto& gmmxx = solver.gmmxx();
    gmmxx.setMethod(gmmxxSettings.getLinearEquationSystemMethod());
    gmmxx.setPreconditioner(gmmxxSettings.getPreconditioningMethod());
    gmmxx.setRestartThreshold(gmmxxSettings.getRestartIterationCount());
    applyIfSet(gmmxxSettings.isMaximalIterationCountSet(), [&]() { gmmxx.setMaximalNumberOfIterations(gmmxxSettings.getMaximalIterationCount()); });
    applyPrecision(gmmxx, gmmxxSettings.getPrecision());

    auto const& gurobiSettings = storm::settings::getModule<storm::settings::modules::GurobiSettings>();
    auto& gurobi = solver.gurobi();
    gurobi.setMethod(gurobiSettings.getMethod());
    gurobi.setNumberOfThreads(gurobiSettings.getNumberOfThreads());
    gurobi.setMIPFocus(gurobiSettings.getMIPFocus());
    gurobi.setNumberOfConcurrentMipThreads(gurobiSettings.getNumberOfConcurrentMipThreads());
    gurobi.setIntegerTolerance(gurobiSettings.getIntegerTolerance());
    gurobi.setOutput(gurobiSettings.isOutputSet());

    auto const& lraSettings = storm::settings::getModule<storm::settings::modules::LongRunAverageSolverSettings>();
    auto& lra = solver.lra();
    lra.setDetLraMethod(lraSettings.getDetLraMethod(), lraSettings.isDetLraMethodSetFromDefaultValue());
    lra.setNondetLraMethod(lraSettings.getNondetLraMethod(), lraSettings.isNondetLraMethodSetFromDefaultValue());
    applyPrecision(lra, lraSettings.getPrecision());
    lra.setRelativeTerminationCriterion(lraSettings.isRelativePrecision());
    applyIfSet(lraSettings.isMaximalIterationCountSet(), [&]() { lra.setMaximalIterationCount(lraSettings.getMaximalIterationCount()); });
    lra.setAperiodicFactor(storm::utility::convertNumber<storm::RationalNumber>(lraSettings.getAperiodicFactor()));

    auto const& minMaxSettings = storm::settings::getModule<storm::settings::modules::MinMaxEquationSolverSettings>();
    auto& minMax = solver.minMax();
    minMax.setMethod(minMaxSettings.getMinMaxEquationSolvingMethod(), minMaxSettings.isMinMaxEquationSolvingMethodSetFromDefaultValue());
    applyIfSet(minMaxSettings.isMaximalIterationCountSet(), [&]() { minMax.setMaximalNumberOfIterations(minMaxSettings.getMaximalIterationCount()); });
    applyPrecision(minMax, minMaxSettings.getPrecision());
    minMax.setRelativeTerminationCriterion(minMaxSettings.getConvergenceCriterion() ==
                                           storm::settings::modules::MinMaxEquationSolverSettings::ConvergenceCriterion::Relative);
    minMax.setMultiplicationStyle(minMaxSettings.getValueIterationMultiplicationStyle());
    minMax.setForceRequireUnique(minMaxSettings.isForceUniqueSolutionRequirementSet());

    auto& minMaxLp = minMax.lp();
    minMaxLp.setUseNonTrivialBounds(minMaxSettings.getLpUseNonTrivialBounds());
    minMaxLp.setOptimizeOnlyForInitialState(minMaxSettings.getLpUseOnlyInitialStateAsObjective());
    minMaxLp.setUseEqualityForSingleActions(minMaxSettings.getLpUseEqualityForTrivialActions());

    auto const& multiplierSettings = storm::settings::getModule<storm::settings::modules::MultiplierSettings>();
    solver.multiplier().setType(multiplierSettings.getMultiplierType(), multiplierSettings.isMultiplierTypeSetFromDefaultValue());

    auto const& nativeSettings = storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>();
    auto& native = solver.native();
    native.setMethod(nativeSettings.getLinearEquationSystemMethod(), nativeSettings.isLinearEquationSystemTechniqueSetFromDefaultValue());
    applyIfSet(nativeSettings.isMaximalIterationCountSet(), [&]() { native.setMaximalNumberOfIterations(nativeSettings.getMaximalIterationCount()); });
    applyPrecision(native, nativeSettings.getPrecision());
    native.setRelativeTerminationCriterion(nativeSettings.getConvergenceCriterion() ==
                                           storm::settings::modules::NativeEquationSolverSettings::ConvergenceCriterion::Relative);
    native.setPowerMethodMultiplicationStyle(nativeSettings.getPowerMethodMultiplicationStyle());
    native.setSorOmega(storm::utility::convertNumber<storm::RationalNumber>(nativeSettings.getOmega()));
    native.setSymmetricUpdates(nativeSettings.isForceIntervalIterationSymmetricUpdatesSet());

    auto const& oviSettings = storm::settings::getModule<storm::settings::modules::OviSolverSettings>();
    applyIfSet(oviSettings.hasUpperBoundGuessingFactorBeenSet(), [&]() {
        solver.ovi().setUpperBoundGuessingFactor(storm::utility::convertNumber<storm::RationalNumber>(oviSettings.getUpperBoundGuessingFactor()));
    });

    auto const& tbSettings = storm::settings::getModule<storm::settings::modules::TimeBoundedSolverSettings>();
    auto& timeBounded = solver.timeBounded();
    timeBounded.setMaMethod(tbSettings.getMaMethod(), tbSettings.isMaMethodSetFromDefaultValue());
    applyPrecision(timeBounded, tbSettings.getPrecision());
    timeBounded.setRelativeTerminationCriterion(tbSettings.isRelativePrecision());
    timeBounded.setUnifPlusKappa(storm::utility::convertNumber<storm::RationalNumber>(tbSettings.getUnifPlusKappa()));

    auto const& topologicalSettings = storm::settings::getModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    auto& topological = solver.topological();
    topological.setUnderlyingEquationSolverType(topologicalSettings.getUnderlyingEquationSolverType(),
                                                topologicalSettings.isUnderlyingEquationSolverTypeSetFromDefaultValue());
    topological.setUnderlyingMinMaxMethod(topologicalSettings.getUnderlyingMinMaxMethod(), topologicalSettings.isUnderlyingMinMaxMethodSetFromDefaultValue());
    topological.setExtendRelevantValues(topologicalSettings.isExtendRelevantValues());
}

void EnvironmentBuilder::setModelcheckerEnvironment(storm::ModelCheckerEnvironment& modelchecker) {
    auto const& mcSettings = storm::settings::getModule<storm::settings::modules::ModelCheckerSettings>();
    applyIfSet(mcSettings.isLtl2daToolSet(), [&]() { modelchecker.setLtl2daTool(mcSettings.getLtl2daTool()); });

    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    modelchecker.setSteadyStateDistributionAlgorithm(ioSettings.getSteadyStateDistributionAlgorithm());
    modelchecker.setFilterRewZero(mcSettings.isFilterRewZeroSet());
    modelchecker.setExportCdf(ioSettings.isExportCdfSet());
    applyIfSet(ioSettings.isExportCdfSet(), [&]() { modelchecker.setExportCdfDirectory(ioSettings.getExportCdfDirectory()); });

    auto const& conditionalSettings = storm::settings::getModule<storm::settings::modules::ConditionalSettings>();
    auto& conditional = modelchecker.conditional();
    conditional.setAlgorithm(conditionalSettings.getConditionalAlgorithmSetting());
    applyPrecision(conditional, conditionalSettings.getConditionalPrecision(), conditionalSettings.isConditionalPrecisionSetFromDefaultValue());
    conditional.setRelativePrecision(!conditionalSettings.isConditionalPrecisionAbsolute());

    auto const& multiobjectiveSettings = storm::settings::getModule<storm::settings::modules::MultiObjectiveSettings>();
    auto& multiobjective = modelchecker.multi();
    multiobjective.setMethod(multiobjectiveSettings.getMultiObjectiveMethod());
    applyIfSet(multiobjectiveSettings.isExportPlotSet(), [&]() {
        std::string const exportPlotDirectory = multiobjectiveSettings.getExportPlotDirectory();
        multiobjective.setPlotPathUnderApproximation(exportPlotDirectory + "underapproximation.csv");
        multiobjective.setPlotPathOverApproximation(exportPlotDirectory + "overapproximation.csv");
        multiobjective.setPlotPathParetoPoints(exportPlotDirectory + "paretopoints.csv");
    });
    applyPrecision(multiobjective, multiobjectiveSettings.getPrecision());
    if (multiobjectiveSettings.getPrecisionAbsolute()) {
        multiobjective.setPrecisionType(MultiObjectiveModelCheckerEnvironment::PrecisionType::Absolute);
    } else if (multiobjectiveSettings.getPrecisionRelativeToDiff()) {
        multiobjective.setPrecisionType(MultiObjectiveModelCheckerEnvironment::PrecisionType::RelativeToDiff);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unhandled precision type.");
    }
    if (multiobjectiveSettings.isAutoEncodingSet()) {
        multiobjective.setEncodingType(MultiObjectiveModelCheckerEnvironment::EncodingType::Auto);
    } else if (multiobjectiveSettings.isClassicEncodingSet()) {
        multiobjective.setEncodingType(MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
    } else if (multiobjectiveSettings.isFlowEncodingSet()) {
        multiobjective.setEncodingType(MultiObjectiveModelCheckerEnvironment::EncodingType::Flow);
    }
    multiobjective.setUseBsccOrderEncoding(multiobjectiveSettings.isBsccDetectionViaOrderConstraintsSet());
    multiobjective.setUseIndicatorConstraints(multiobjectiveSettings.isIndicatorConstraintsSet());
    multiobjective.setUseRedundantBsccConstraints(multiobjectiveSettings.isRedundantBsccConstraintsSet());
    applyIfSet(multiobjectiveSettings.isWeightedSumApproximationTradeoffSet(), [&]() {
        multiobjective.setApproximationTradeoff(
            storm::utility::convertNumber<storm::RationalNumber>(multiobjectiveSettings.getWeightedSumApproximationTradeoff()));
    });
    applyIfSet(multiobjectiveSettings.isMaxStepsSet(), [&]() { multiobjective.setMaxSteps(multiobjectiveSettings.getMaxSteps()); });
    applyIfSet(multiobjectiveSettings.hasSchedulerRestriction(),
               [&]() { multiobjective.setSchedulerRestriction(multiobjectiveSettings.getSchedulerRestriction()); });
    multiobjective.setPrintResults(multiobjectiveSettings.isPrintResultsSet());
}

void EnvironmentBuilder::setDdEnvironment(storm::DdEnvironment& dd) {
    if (storm::settings::hasModule<storm::settings::modules::CuddSettings>()) {
        auto const& cuddSettings = storm::settings::getModule<storm::settings::modules::CuddSettings>();
        auto& cudd = dd.cudd();
        cudd.setConstantPrecision(cuddSettings.getConstantPrecision());
        cudd.setMaximalMemory(cuddSettings.getMaximalMemory());
        cudd.setReorderingEnabled(cuddSettings.isReorderingEnabled());
        cudd.setReorderingTechnique(cuddSettings.getReorderingTechnique());
    }
    if (storm::settings::hasModule<storm::settings::modules::SylvanSettings>()) {
        auto const& sylvanSettings = storm::settings::getModule<storm::settings::modules::SylvanSettings>();
        auto& sylvan = dd.sylvan();
        sylvan.setMaximalMemory(sylvanSettings.getMaximalMemory());
        applyIfSet(sylvanSettings.isNumberOfThreadsSet(), [&]() { sylvan.setNumberOfThreads(sylvanSettings.getNumberOfThreads()); });
    }
}

void EnvironmentBuilder::setExplorationEnvironment(storm::ExplorationEnvironment& exploration) {
    auto const& explorationSettings = storm::settings::getModule<storm::settings::modules::ExplorationSettings>();
    exploration.setPrecomputationType(explorationSettings.getPrecomputationType());
    exploration.setStepsUntilPrecomputation(static_cast<uint64_t>(explorationSettings.getNumberOfExplorationStepsUntilPrecomputation()));
    applyIfSet(explorationSettings.isNumberOfSampledPathsUntilPrecomputationSet(),
               [&]() { exploration.setSampledPathsUntilPrecomputation(explorationSettings.getNumberOfSampledPathsUntilPrecomputation()); });
    exploration.setNextStateHeuristic(explorationSettings.getNextStateHeuristic());
    exploration.setPrecision(explorationSettings.getPrecision());
}

}  // namespace settings
}  // namespace storm