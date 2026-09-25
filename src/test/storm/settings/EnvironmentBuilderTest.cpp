#include "test/storm_gtest.h"

#include <limits>

#include "storm/environment/Environment.h"
#include "storm/environment/dd/CuddDdManagerEnvironment.h"
#include "storm/environment/dd/DdEnvironment.h"
#include "storm/environment/dd/SylvanDdManagerEnvironment.h"
#include "storm/environment/exploration/ExplorationEnvironment.h"
#include "storm/environment/modelchecker/ConditionalModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/EliminationSolverEnvironment.h"
#include "storm/environment/solver/GameSolverEnvironment.h"
#include "storm/environment/solver/GlpkSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/GurobiSolverEnvironment.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxLpSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/MultiplierEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/OviSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/environment/solver/TimeBoundedSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/settings/EnvironmentBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

namespace {

storm::Environment buildFromSettings() {
    return storm::settings::EnvironmentBuilder::buildEnvironment();
}

}  // namespace

TEST(EnvironmentBuilderTest, TopLevelEnvironmentMatchesDefault) {
    storm::Environment const defaultEnv;
    storm::Environment const builtEnv = buildFromSettings();

    EXPECT_DOUBLE_EQ(defaultEnv.modelTolerance(), builtEnv.modelTolerance());
}

TEST(EnvironmentBuilderTest, SolverEnvironmentMatchesDefault) {
    storm::Environment const defaultEnv;
    storm::Environment const builtEnv = buildFromSettings();

    storm::SolverEnvironment const& defaultSolver = defaultEnv.solver();
    storm::SolverEnvironment const& builtSolver = builtEnv.solver();

    EXPECT_EQ(defaultSolver.isForceSoundness(), builtSolver.isForceSoundness());
    EXPECT_EQ(defaultSolver.isForceExact(), builtSolver.isForceExact());
    EXPECT_EQ(defaultSolver.isDebugSet(), builtSolver.isDebugSet());
    EXPECT_EQ(defaultSolver.isVerboseSet(), builtSolver.isVerboseSet());
    EXPECT_EQ(defaultSolver.getShowProgressDelay(), builtSolver.getShowProgressDelay());
    EXPECT_EQ(defaultSolver.getLinearEquationSolverType(), builtSolver.getLinearEquationSolverType());
    EXPECT_EQ(defaultSolver.isLinearEquationSolverTypeSetFromDefaultValue(), builtSolver.isLinearEquationSolverTypeSetFromDefaultValue());
    EXPECT_EQ(defaultSolver.getLpSolverType(), builtSolver.getLpSolverType());
    EXPECT_EQ(defaultSolver.isLpSolverTypeSetFromDefaultValue(), builtSolver.isLpSolverTypeSetFromDefaultValue());

    EXPECT_EQ(defaultSolver.eigen().getMethod(), builtSolver.eigen().getMethod());
    EXPECT_EQ(defaultSolver.eigen().isMethodSetFromDefault(), builtSolver.eigen().isMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.eigen().getPreconditioner(), builtSolver.eigen().getPreconditioner());
    EXPECT_EQ(defaultSolver.eigen().getRestartThreshold(), builtSolver.eigen().getRestartThreshold());
    EXPECT_EQ(defaultSolver.eigen().getMaximalNumberOfIterations(), builtSolver.eigen().getMaximalNumberOfIterations());
    EXPECT_EQ(defaultSolver.eigen().getPrecision(), builtSolver.eigen().getPrecision());

    EXPECT_EQ(defaultSolver.elimination().getOrder(), builtSolver.elimination().getOrder());
    EXPECT_EQ(defaultSolver.elimination().getMethod(), builtSolver.elimination().getMethod());
    EXPECT_EQ(defaultSolver.elimination().getMaximalSccSize(), builtSolver.elimination().getMaximalSccSize());
    EXPECT_EQ(defaultSolver.elimination().isEliminateEntryStatesLastSet(), builtSolver.elimination().isEliminateEntryStatesLastSet());

    EXPECT_EQ(defaultSolver.game().getMethod(), builtSolver.game().getMethod());
    EXPECT_EQ(defaultSolver.game().isMethodSetFromDefault(), builtSolver.game().isMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.game().getMaximalNumberOfIterations(), builtSolver.game().getMaximalNumberOfIterations());
    EXPECT_EQ(defaultSolver.game().getPrecision(), builtSolver.game().getPrecision());
    EXPECT_EQ(defaultSolver.game().getRelativeTerminationCriterion(), builtSolver.game().getRelativeTerminationCriterion());

    EXPECT_DOUBLE_EQ(defaultSolver.glpk().getIntegerTolerance(), builtSolver.glpk().getIntegerTolerance());
    EXPECT_EQ(defaultSolver.glpk().isMILPPresolverEnabled(), builtSolver.glpk().isMILPPresolverEnabled());
    EXPECT_EQ(defaultSolver.glpk().isOutputSet(), builtSolver.glpk().isOutputSet());

    EXPECT_EQ(defaultSolver.gmmxx().getMethod(), builtSolver.gmmxx().getMethod());
    EXPECT_EQ(defaultSolver.gmmxx().getPreconditioner(), builtSolver.gmmxx().getPreconditioner());
    EXPECT_EQ(defaultSolver.gmmxx().getRestartThreshold(), builtSolver.gmmxx().getRestartThreshold());
    EXPECT_EQ(defaultSolver.gmmxx().getMaximalNumberOfIterations(), builtSolver.gmmxx().getMaximalNumberOfIterations());
    EXPECT_EQ(defaultSolver.gmmxx().getPrecision(), builtSolver.gmmxx().getPrecision());

    EXPECT_EQ(defaultSolver.gurobi().getMethod(), builtSolver.gurobi().getMethod());
    EXPECT_EQ(defaultSolver.gurobi().getNumberOfThreads(), builtSolver.gurobi().getNumberOfThreads());
    EXPECT_EQ(defaultSolver.gurobi().getMIPFocus(), builtSolver.gurobi().getMIPFocus());
    EXPECT_EQ(defaultSolver.gurobi().getNumberOfConcurrentMipThreads(), builtSolver.gurobi().getNumberOfConcurrentMipThreads());
    EXPECT_DOUBLE_EQ(defaultSolver.gurobi().getIntegerTolerance(), builtSolver.gurobi().getIntegerTolerance());
    EXPECT_EQ(defaultSolver.gurobi().isOutputSet(), builtSolver.gurobi().isOutputSet());

    EXPECT_EQ(defaultSolver.lra().getDetLraMethod(), builtSolver.lra().getDetLraMethod());
    EXPECT_EQ(defaultSolver.lra().isDetLraMethodSetFromDefault(), builtSolver.lra().isDetLraMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.lra().getNondetLraMethod(), builtSolver.lra().getNondetLraMethod());
    EXPECT_EQ(defaultSolver.lra().isNondetLraMethodSetFromDefault(), builtSolver.lra().isNondetLraMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.lra().getPrecision(), builtSolver.lra().getPrecision());
    EXPECT_EQ(defaultSolver.lra().getRelativeTerminationCriterion(), builtSolver.lra().getRelativeTerminationCriterion());
    EXPECT_EQ(defaultSolver.lra().isMaximalIterationCountSet(), builtSolver.lra().isMaximalIterationCountSet());
    EXPECT_EQ(defaultSolver.lra().getAperiodicFactor(), builtSolver.lra().getAperiodicFactor());

    EXPECT_EQ(defaultSolver.minMax().getMethod(), builtSolver.minMax().getMethod());
    EXPECT_EQ(defaultSolver.minMax().isMethodSetFromDefault(), builtSolver.minMax().isMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.minMax().getMaximalNumberOfIterations(), builtSolver.minMax().getMaximalNumberOfIterations());
    EXPECT_EQ(defaultSolver.minMax().getPrecision(), builtSolver.minMax().getPrecision());
    EXPECT_EQ(defaultSolver.minMax().getRelativeTerminationCriterion(), builtSolver.minMax().getRelativeTerminationCriterion());
    EXPECT_EQ(defaultSolver.minMax().getMultiplicationStyle(), builtSolver.minMax().getMultiplicationStyle());
    EXPECT_EQ(defaultSolver.minMax().isForceRequireUnique(), builtSolver.minMax().isForceRequireUnique());

    EXPECT_EQ(defaultSolver.minMax().lp().getUseEqualityForSingleActions(), builtSolver.minMax().lp().getUseEqualityForSingleActions());
    EXPECT_EQ(defaultSolver.minMax().lp().getOptimizeOnlyForInitialState(), builtSolver.minMax().lp().getOptimizeOnlyForInitialState());
    EXPECT_EQ(defaultSolver.minMax().lp().getUseNonTrivialBounds(), builtSolver.minMax().lp().getUseNonTrivialBounds());

    EXPECT_EQ(defaultSolver.multiplier().getType(), builtSolver.multiplier().getType());
    EXPECT_EQ(defaultSolver.multiplier().isTypeSetFromDefault(), builtSolver.multiplier().isTypeSetFromDefault());

    EXPECT_EQ(defaultSolver.native().getMethod(), builtSolver.native().getMethod());
    EXPECT_EQ(defaultSolver.native().isMethodSetFromDefault(), builtSolver.native().isMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.native().getMaximalNumberOfIterations(), builtSolver.native().getMaximalNumberOfIterations());
    EXPECT_EQ(defaultSolver.native().getPrecision(), builtSolver.native().getPrecision());
    EXPECT_EQ(defaultSolver.native().getRelativeTerminationCriterion(), builtSolver.native().getRelativeTerminationCriterion());
    EXPECT_EQ(defaultSolver.native().getPowerMethodMultiplicationStyle(), builtSolver.native().getPowerMethodMultiplicationStyle());
    EXPECT_EQ(defaultSolver.native().getSorOmega(), builtSolver.native().getSorOmega());
    EXPECT_EQ(defaultSolver.native().isSymmetricUpdatesSet(), builtSolver.native().isSymmetricUpdatesSet());

    EXPECT_EQ(defaultSolver.ovi().getUpperBoundGuessingFactor(), builtSolver.ovi().getUpperBoundGuessingFactor());

    EXPECT_EQ(defaultSolver.timeBounded().getMaMethod(), builtSolver.timeBounded().getMaMethod());
    EXPECT_EQ(defaultSolver.timeBounded().isMaMethodSetFromDefault(), builtSolver.timeBounded().isMaMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.timeBounded().getPrecision(), builtSolver.timeBounded().getPrecision());
    EXPECT_EQ(defaultSolver.timeBounded().getRelativeTerminationCriterion(), builtSolver.timeBounded().getRelativeTerminationCriterion());
    EXPECT_EQ(defaultSolver.timeBounded().getUnifPlusKappa(), builtSolver.timeBounded().getUnifPlusKappa());

    EXPECT_EQ(defaultSolver.topological().getUnderlyingEquationSolverType(), builtSolver.topological().getUnderlyingEquationSolverType());
    EXPECT_EQ(defaultSolver.topological().isUnderlyingEquationSolverTypeSetFromDefault(),
              builtSolver.topological().isUnderlyingEquationSolverTypeSetFromDefault());
    EXPECT_EQ(defaultSolver.topological().getUnderlyingMinMaxMethod(), builtSolver.topological().getUnderlyingMinMaxMethod());
    EXPECT_EQ(defaultSolver.topological().isUnderlyingMinMaxMethodSetFromDefault(), builtSolver.topological().isUnderlyingMinMaxMethodSetFromDefault());
    EXPECT_EQ(defaultSolver.topological().isExtendRelevantValues(), builtSolver.topological().isExtendRelevantValues());
}

TEST(EnvironmentBuilderTest, ModelCheckerEnvironmentMatchesDefault) {
    storm::Environment const defaultEnv;
    storm::Environment const builtEnv = buildFromSettings();

    storm::ModelCheckerEnvironment const& defaultMc = defaultEnv.modelchecker();
    storm::ModelCheckerEnvironment const& builtMc = builtEnv.modelchecker();

    EXPECT_EQ(defaultMc.getSteadyStateDistributionAlgorithm(), builtMc.getSteadyStateDistributionAlgorithm());
    EXPECT_EQ(defaultMc.isLtl2daToolSet(), builtMc.isLtl2daToolSet());
    EXPECT_EQ(defaultMc.isFilterRewZeroSet(), builtMc.isFilterRewZeroSet());
    EXPECT_EQ(defaultMc.isExportCdfSet(), builtMc.isExportCdfSet());
    EXPECT_EQ(defaultMc.getExportCdfDirectory(), builtMc.getExportCdfDirectory());

    EXPECT_EQ(defaultMc.conditional().getAlgorithm(), builtMc.conditional().getAlgorithm());
    EXPECT_EQ(defaultMc.conditional().getPrecision(), builtMc.conditional().getPrecision());
    EXPECT_EQ(defaultMc.conditional().isRelativePrecision(), builtMc.conditional().isRelativePrecision());
    EXPECT_EQ(defaultMc.conditional().isPrecisionSetFromDefault(), builtMc.conditional().isPrecisionSetFromDefault());

    EXPECT_EQ(defaultMc.multi().getMethod(), builtMc.multi().getMethod());
    EXPECT_EQ(defaultMc.multi().isExportPlotSet(), builtMc.multi().isExportPlotSet());
    EXPECT_EQ(defaultMc.multi().getPrecision(), builtMc.multi().getPrecision());
    EXPECT_EQ(defaultMc.multi().getPrecisionType(), builtMc.multi().getPrecisionType());
    EXPECT_EQ(defaultMc.multi().getEncodingType(), builtMc.multi().getEncodingType());
    EXPECT_EQ(defaultMc.multi().getUseIndicatorConstraints(), builtMc.multi().getUseIndicatorConstraints());
    EXPECT_EQ(defaultMc.multi().getUseBsccOrderEncoding(), builtMc.multi().getUseBsccOrderEncoding());
    EXPECT_EQ(defaultMc.multi().getUseRedundantBsccConstraints(), builtMc.multi().getUseRedundantBsccConstraints());
    EXPECT_EQ(defaultMc.multi().isApproximationTradeoffSet(), builtMc.multi().isApproximationTradeoffSet());
    EXPECT_EQ(defaultMc.multi().isMaxStepsSet(), builtMc.multi().isMaxStepsSet());
    EXPECT_EQ(defaultMc.multi().isSchedulerRestrictionSet(), builtMc.multi().isSchedulerRestrictionSet());
    EXPECT_EQ(defaultMc.multi().isPrintResultsSet(), builtMc.multi().isPrintResultsSet());
}

TEST(EnvironmentBuilderTest, DdAndExplorationEnvironmentMatchDefault) {
    storm::Environment const defaultEnv;
    storm::Environment const builtEnv = buildFromSettings();

    EXPECT_DOUBLE_EQ(defaultEnv.dd().cudd().getConstantPrecision(), builtEnv.dd().cudd().getConstantPrecision());
    EXPECT_EQ(defaultEnv.dd().cudd().getMaximalMemory(), builtEnv.dd().cudd().getMaximalMemory());
    EXPECT_EQ(defaultEnv.dd().cudd().isReorderingEnabled(), builtEnv.dd().cudd().isReorderingEnabled());
    EXPECT_EQ(defaultEnv.dd().cudd().getReorderingTechnique(), builtEnv.dd().cudd().getReorderingTechnique());

    EXPECT_EQ(defaultEnv.dd().sylvan().getMaximalMemory(), builtEnv.dd().sylvan().getMaximalMemory());
    EXPECT_EQ(defaultEnv.dd().sylvan().getNumberOfThreads(), builtEnv.dd().sylvan().getNumberOfThreads());

    EXPECT_EQ(defaultEnv.exploration().getPrecomputationType(), builtEnv.exploration().getPrecomputationType());
    EXPECT_EQ(defaultEnv.exploration().getStepsUntilPrecomputation(), builtEnv.exploration().getStepsUntilPrecomputation());
    EXPECT_EQ(defaultEnv.exploration().getSampledPathsUntilPrecomputation(), builtEnv.exploration().getSampledPathsUntilPrecomputation());
    EXPECT_EQ(defaultEnv.exploration().getNextStateHeuristic(), builtEnv.exploration().getNextStateHeuristic());
    EXPECT_DOUBLE_EQ(defaultEnv.exploration().getPrecision(), builtEnv.exploration().getPrecision());
}

TEST(EnvironmentBuilderTest, BuildsEnvironmentFromSettings) {
    storm::settings::mutableManager().setFromExplodedString({"--sound"});

    storm::Environment const builtEnv = buildFromSettings();
    EXPECT_TRUE(builtEnv.solver().isForceSoundness());

    storm::Environment const defaultEnv;
    EXPECT_FALSE(defaultEnv.solver().isForceSoundness());

    // Restore the default values so that the mutation of the global settings manager does not influence subsequent tests.
    storm::settings::mutableManager().getModule(storm::settings::modules::GeneralSettings::moduleName).restoreDefaults();
}

TEST(EnvironmentBuilderTest, SoundFlagIsResetByRestoreDefaults) {
    storm::settings::mutableManager().setFromExplodedString({"--sound"});
    ASSERT_TRUE(storm::settings::getModule<storm::settings::modules::GeneralSettings>().isSoundSet());

    storm::settings::mutableManager().getModule(storm::settings::modules::GeneralSettings::moduleName).restoreDefaults();
    EXPECT_FALSE(storm::settings::getModule<storm::settings::modules::GeneralSettings>().isSoundSet());
    EXPECT_FALSE(buildFromSettings().solver().isForceSoundness());
}
