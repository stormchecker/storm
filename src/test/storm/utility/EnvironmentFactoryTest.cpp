#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/api/verification.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/AllSolverEnvironments.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/languages/jani/Property.h"
#include "storm/languages/prism/Program.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Model.h"
#include "storm/settings/EnvironmentBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/constants.h"
#include "storm/utility/environment.h"

namespace {

template<typename ValueType>
storm::utility::ExtendedValueType<ValueType> checkInitialState(storm::Environment const& env, std::string const& file, std::string const& formulaString) {
    auto program = storm::api::parseProgram(file);
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    auto model = storm::api::buildSparseModel<ValueType>(program, formulas);
    auto task = storm::api::createTask<ValueType>(formulas.front(), true);
    auto result = storm::api::verifyWithSparseEngine<ValueType>(env, model, task);
    return result->template asExplicitQuantitativeCheckResult<ValueType>()[*model->getInitialStates().begin()];
}

}  // namespace

class EnvironmentFactoryModelCheckingTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_SMT_SOLVER
        GTEST_SKIP() << "No SMT solver available.";
#endif
    }
};

TEST(EnvironmentFactoryTest, DefaultEnvironmentIsNotSound) {
    storm::Environment const env = storm::utility::environment::EnvironmentFactory().create();
    EXPECT_FALSE(env.solver().isForceSoundness());
}

TEST(EnvironmentFactoryTest, SoundEnvironmentSelectsSoundMethods) {
    storm::Environment const env = storm::utility::environment::getSoundEnvironment();
    storm::SolverEnvironment const& solver = env.solver();

    EXPECT_TRUE(solver.isForceSoundness());
    EXPECT_FALSE(solver.isForceExact());

    // Defaults which are already sound remain untouched
    EXPECT_EQ(storm::solver::EquationSolverType::Topological, solver.getLinearEquationSolverType());
    EXPECT_EQ(storm::solver::MinMaxMethod::Topological, solver.minMax().getMethod());

    EXPECT_EQ(storm::solver::EquationSolverType::Native, solver.topological().getUnderlyingEquationSolverType());
    EXPECT_EQ(storm::solver::MinMaxMethod::OptimisticValueIteration, solver.topological().getUnderlyingMinMaxMethod());
    EXPECT_EQ(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration, solver.native().getMethod());
    EXPECT_EQ(storm::solver::EigenLinearEquationSolverMethod::SparseLU, solver.eigen().getMethod());
    EXPECT_EQ(storm::solver::GameMethod::PolicyIteration, solver.game().getMethod());
    EXPECT_EQ(storm::solver::LraMethod::ValueIteration, solver.lra().getDetLraMethod());
    EXPECT_EQ(storm::solver::LraMethod::ValueIteration, solver.lra().getNondetLraMethod());
    EXPECT_EQ(storm::SteadyStateDistributionAlgorithm::ExpectedVisitingTimes, env.modelchecker().getSteadyStateDistributionAlgorithm());

    // The choices count as defaults, such that e.g. exact mode can still override them
    EXPECT_TRUE(solver.isLinearEquationSolverTypeSetFromDefaultValue());
    EXPECT_TRUE(solver.topological().isUnderlyingEquationSolverTypeSetFromDefault());
    EXPECT_TRUE(solver.topological().isUnderlyingMinMaxMethodSetFromDefault());
    EXPECT_TRUE(solver.native().isMethodSetFromDefault());
    EXPECT_TRUE(solver.eigen().isMethodSetFromDefault());
    EXPECT_TRUE(solver.minMax().isMethodSetFromDefault());
    EXPECT_TRUE(solver.game().isMethodSetFromDefault());
    EXPECT_TRUE(solver.lra().isDetLraMethodSetFromDefault());
    EXPECT_TRUE(solver.lra().isNondetLraMethodSetFromDefault());
}

TEST(EnvironmentFactoryTest, ClonedFactoryCreatesSoundEnvironment) {
    std::unique_ptr<storm::utility::environment::EnvironmentFactory> factory = storm::utility::environment::SoundEnvironmentFactory().clone();
    EXPECT_TRUE(factory->create().solver().isForceSoundness());
}

TEST_F(EnvironmentFactoryModelCheckingTest, MatchesSoundCommandLineOption) {
    storm::settings::mutableManager().setFromExplodedString({"--sound"});
    storm::Environment const cliEnv = storm::settings::EnvironmentBuilder::buildEnvironment();
    storm::settings::mutableManager().getModule(storm::settings::modules::GeneralSettings::moduleName).restoreDefaults();

    storm::Environment const env = storm::utility::environment::getSoundEnvironment();
    EXPECT_EQ(cliEnv.solver().isForceSoundness(), env.solver().isForceSoundness());

    double const precision = 1e-6;
    std::string const dtmcFile = STORM_TEST_RESOURCES_DIR "/dtmc/die.pm";
    std::string const dtmcFormula = "P=? [F \"one\"]";
    EXPECT_NEAR(1.0 / 6.0, checkInitialState<double>(env, dtmcFile, dtmcFormula), precision);
    EXPECT_NEAR(checkInitialState<double>(cliEnv, dtmcFile, dtmcFormula), checkInitialState<double>(env, dtmcFile, dtmcFormula), precision);

    std::string const mdpFile = STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm";
    std::string const mdpFormula = "Pmin=? [F \"two\"]";
    EXPECT_NEAR(1.0 / 36.0, checkInitialState<double>(env, mdpFile, mdpFormula), precision);
    EXPECT_NEAR(checkInitialState<double>(cliEnv, mdpFile, mdpFormula), checkInitialState<double>(env, mdpFile, mdpFormula), precision);
}

TEST(EnvironmentFactoryTest, DefaultEnvironmentIsNotExact) {
    storm::Environment const env = storm::utility::environment::EnvironmentFactory().create();
    EXPECT_FALSE(env.solver().isForceExact());
}

TEST(EnvironmentFactoryTest, ExactEnvironmentSelectsExactMethods) {
    storm::Environment const env = storm::utility::environment::getExactEnvironment();
    storm::SolverEnvironment const& solver = env.solver();

    EXPECT_TRUE(solver.isForceExact());
    EXPECT_FALSE(solver.isForceSoundness());

    // Defaults which already support exact computations remain untouched
    EXPECT_EQ(storm::solver::EquationSolverType::Topological, solver.getLinearEquationSolverType());
    EXPECT_EQ(storm::solver::MinMaxMethod::Topological, solver.minMax().getMethod());

    EXPECT_EQ(storm::solver::EquationSolverType::Eigen, solver.topological().getUnderlyingEquationSolverType());
    EXPECT_EQ(storm::solver::MinMaxMethod::PolicyIteration, solver.topological().getUnderlyingMinMaxMethod());
    EXPECT_EQ(storm::solver::NativeLinearEquationSolverMethod::RationalSearch, solver.native().getMethod());
    EXPECT_EQ(storm::solver::EigenLinearEquationSolverMethod::SparseLU, solver.eigen().getMethod());
    EXPECT_EQ(storm::solver::GameMethod::PolicyIteration, solver.game().getMethod());
    EXPECT_EQ(storm::solver::LraMethod::GainBiasEquations, solver.lra().getDetLraMethod());
    EXPECT_EQ(storm::solver::LraMethod::LinearProgramming, solver.lra().getNondetLraMethod());
    EXPECT_EQ(storm::solver::LpSolverType::Z3, solver.getLpSolverType());

    EXPECT_TRUE(solver.isLinearEquationSolverTypeSetFromDefaultValue());
    EXPECT_TRUE(solver.topological().isUnderlyingEquationSolverTypeSetFromDefault());
    EXPECT_TRUE(solver.topological().isUnderlyingMinMaxMethodSetFromDefault());
    EXPECT_TRUE(solver.native().isMethodSetFromDefault());
    EXPECT_TRUE(solver.eigen().isMethodSetFromDefault());
    EXPECT_TRUE(solver.minMax().isMethodSetFromDefault());
    EXPECT_TRUE(solver.game().isMethodSetFromDefault());
    EXPECT_TRUE(solver.lra().isDetLraMethodSetFromDefault());
    EXPECT_TRUE(solver.lra().isNondetLraMethodSetFromDefault());
    EXPECT_TRUE(solver.isLpSolverTypeSetFromDefaultValue());
}

TEST(EnvironmentFactoryTest, ClonedFactoryCreatesExactEnvironment) {
    std::unique_ptr<storm::utility::environment::EnvironmentFactory> factory = storm::utility::environment::ExactEnvironmentFactory().clone();
    EXPECT_TRUE(factory->create().solver().isForceExact());
}

TEST_F(EnvironmentFactoryModelCheckingTest, MatchesExactCommandLineOption) {
    storm::settings::mutableManager().setFromExplodedString({"--exact"});
    storm::Environment const cliEnv = storm::settings::EnvironmentBuilder::buildEnvironment();
    storm::settings::mutableManager().getModule(storm::settings::modules::GeneralSettings::moduleName).restoreDefaults();

    storm::Environment const env = storm::utility::environment::getExactEnvironment();
    EXPECT_EQ(cliEnv.solver().isForceExact(), env.solver().isForceExact());

    std::string const dtmcFile = STORM_TEST_RESOURCES_DIR "/dtmc/die.pm";
    std::string const dtmcFormula = "P=? [F \"one\"]";
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("1/6")), checkInitialState<storm::RationalNumber>(env, dtmcFile, dtmcFormula));
    EXPECT_EQ(checkInitialState<storm::RationalNumber>(cliEnv, dtmcFile, dtmcFormula), checkInitialState<storm::RationalNumber>(env, dtmcFile, dtmcFormula));

    std::string const mdpFile = STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm";
    std::string const mdpFormula = "Pmin=? [F \"two\"]";
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("1/36")), checkInitialState<storm::RationalNumber>(env, mdpFile, mdpFormula));
    EXPECT_EQ(checkInitialState<storm::RationalNumber>(cliEnv, mdpFile, mdpFormula), checkInitialState<storm::RationalNumber>(env, mdpFile, mdpFormula));
}
