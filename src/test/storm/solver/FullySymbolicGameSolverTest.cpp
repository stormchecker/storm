#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/Environment.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/solver/SymbolicGameSolver.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/solver.h"

class Cudd {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::CUDD;
};

class Sylvan {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
};

template<typename TestType>
class FullySymbolicGameSolverTest : public ::testing::Test {
   public:
    void SetUp() override {
        TestType::checkLibraryAvailable();
    }

    static const storm::dd::DdType DdType = TestType::DdType;
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(FullySymbolicGameSolverTest, TestingTypes, );

TYPED_TEST(FullySymbolicGameSolverTest, Solve) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    // Create some variables.
    auto manager(storm::dd::DdManager<DdType>::createWithDefaultEnvironment());
    std::pair<storm::expressions::Variable, storm::expressions::Variable> state = manager->addMetaVariable("x", 1, 4);
    std::pair<storm::expressions::Variable, storm::expressions::Variable> pl1 = manager->addMetaVariable("a", 0, 1);
    std::pair<storm::expressions::Variable, storm::expressions::Variable> pl2 = manager->addMetaVariable("b", 0, 1);

    storm::dd::Bdd<DdType> allRows = manager->getBddZero();
    std::set<storm::expressions::Variable> rowMetaVariables({state.first});
    std::set<storm::expressions::Variable> columnMetaVariables({state.second});
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs = {state};
    std::set<storm::expressions::Variable> player1Variables({pl1.first});
    std::set<storm::expressions::Variable> player2Variables({pl2.first});

    // Construct simple game.
    storm::dd::Add<DdType, double> matrix =
        manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 2).template toAdd<double>() *
        manager->getEncoding(pl1.first, 0).template toAdd<double>() * manager->getEncoding(pl2.first, 0).template toAdd<double>() * manager->getConstant(0.6);
    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 1).template toAdd<double>() *
              manager->getEncoding(pl1.first, 0).template toAdd<double>() * manager->getEncoding(pl2.first, 0).template toAdd<double>() *
              manager->getConstant(0.4);

    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 2).template toAdd<double>() *
              manager->getEncoding(pl1.first, 0).template toAdd<double>() * manager->getEncoding(pl2.first, 1).template toAdd<double>() *
              manager->getConstant(0.2);
    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 3).template toAdd<double>() *
              manager->getEncoding(pl1.first, 0).template toAdd<double>() * manager->getEncoding(pl2.first, 1).template toAdd<double>() *
              manager->getConstant(0.8);

    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 3).template toAdd<double>() *
              manager->getEncoding(pl1.first, 1).template toAdd<double>() * manager->getEncoding(pl2.first, 0).template toAdd<double>() *
              manager->getConstant(0.5);
    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 4).template toAdd<double>() *
              manager->getEncoding(pl1.first, 1).template toAdd<double>() * manager->getEncoding(pl2.first, 0).template toAdd<double>() *
              manager->getConstant(0.5);

    matrix += manager->getEncoding(state.first, 1).template toAdd<double>() * manager->getEncoding(state.second, 1).template toAdd<double>() *
              manager->getEncoding(pl1.first, 1).template toAdd<double>() * manager->getEncoding(pl2.first, 1).template toAdd<double>() *
              manager->template getConstant<double>(1);

    std::unique_ptr<storm::solver::SymbolicGameSolverFactory<DdType, double>> solverFactory(new storm::solver::SymbolicGameSolverFactory<DdType, double>());

    storm::dd::Bdd<DdType> tmp = matrix.toBdd().existsAbstract({state.second});
    storm::dd::Bdd<DdType> illegalPlayer2Mask = !tmp && manager->getRange(state.first);
    storm::dd::Bdd<DdType> illegalPlayer1Mask = tmp.existsAbstract({pl2.first});
    illegalPlayer2Mask &= illegalPlayer1Mask;
    illegalPlayer1Mask &= !illegalPlayer1Mask && manager->getRange(state.first);

    std::unique_ptr<storm::solver::SymbolicGameSolver<DdType>> solver =
        solverFactory->create(matrix, allRows, illegalPlayer1Mask, illegalPlayer2Mask, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs,
                              player1Variables, player2Variables);

    // Create solution and target state vector.
    storm::dd::Add<DdType, double> x = manager->template getAddZero<double>();
    storm::dd::Add<DdType, double> b =
        manager->getEncoding(state.first, 2).template toAdd<double>() + manager->getEncoding(state.first, 4).template toAdd<double>();

    // Now solve the game with different strategies for the players.
    storm::dd::Add<DdType> result = solver->solveGame(env, storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, x, b);
    result *= manager->getEncoding(state.first, 1).template toAdd<double>();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0, result.getValue(), storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    x = manager->template getAddZero<double>();
    result = solver->solveGame(env, storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize, x, b);
    result *= manager->getEncoding(state.first, 1).template toAdd<double>();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.5, result.getValue(), storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    x = manager->template getAddZero<double>();
    result = solver->solveGame(env, storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize, x, b);
    result *= manager->getEncoding(state.first, 1).template toAdd<double>();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.2, result.getValue(), storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    x = manager->template getAddZero<double>();
    result = solver->solveGame(env, storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, x, b);
    result *= manager->getEncoding(state.first, 1).template toAdd<double>();
    result = result.sumAbstract({state.first});
    EXPECT_NEAR(0.99999892625817599, result.getValue(), storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());
}
