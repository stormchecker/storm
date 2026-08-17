#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/solver/GameSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/settings/SettingsManager.h"
#include "storm/solver/StandardGameSolver.h"
#include "storm/storage/SparseMatrix.h"

namespace {

class DoubleViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().game().setMethod(storm::solver::GameMethod::ValueIteration);
        env.solver().game().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class DoublePiEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().game().setMethod(storm::solver::GameMethod::PolicyIteration);
        env.solver().game().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
        env.solver().setLinearEquationSolverPrecision(env.solver().game().getPrecision());
        return env;
    }
};

class RationalPiEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().game().setMethod(storm::solver::GameMethod::PolicyIteration);
        return env;
    }
};

template<typename TestType>
class GameSolverTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    GameSolverTest() : _environment(TestType::createEnvironment()) {}
    storm::Environment const& env() const {
        return _environment;
    }
    ValueType precision() const {
        return TestType::isExact ? parseNumber("0") : parseNumber("1e-6");
    }
    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DoubleViEnvironment, DoublePiEnvironment, RationalPiEnvironment> TestingTypes;

TYPED_TEST_SUITE(GameSolverTest, TestingTypes, );

TYPED_TEST(GameSolverTest, SolveEquations) {
    typedef typename TestFixture::ValueType ValueType;
    // Construct simple game. Start with player 2 matrix.
    storm::storage::SparseMatrixBuilder<ValueType> player2MatrixBuilder(0, 0, 0, false, true);
    player2MatrixBuilder.newRowGroup(0);
    player2MatrixBuilder.addNextValue(0, 0, this->parseNumber("0.4"));
    player2MatrixBuilder.addNextValue(0, 1, this->parseNumber("0.6"));
    player2MatrixBuilder.addNextValue(1, 1, this->parseNumber("0.2"));
    player2MatrixBuilder.addNextValue(1, 2, this->parseNumber("0.8"));
    player2MatrixBuilder.newRowGroup(2);
    player2MatrixBuilder.addNextValue(2, 2, this->parseNumber("0.5"));
    player2MatrixBuilder.addNextValue(2, 3, this->parseNumber("0.5"));
    player2MatrixBuilder.newRowGroup(4);
    player2MatrixBuilder.newRowGroup(5);
    player2MatrixBuilder.newRowGroup(6);
    storm::storage::SparseMatrix<ValueType> player2Matrix = player2MatrixBuilder.build();

    // Now build player 1 matrix.
    storm::storage::SparseMatrixBuilder<storm::storage::sparse::state_type> player1MatrixBuilder(0, 0, 0, false, true);
    player1MatrixBuilder.newRowGroup(0);
    player1MatrixBuilder.addNextValue(0, 0, 1);
    player1MatrixBuilder.addNextValue(1, 1, 1);
    player1MatrixBuilder.newRowGroup(2);
    player1MatrixBuilder.addNextValue(2, 2, 1);
    player1MatrixBuilder.newRowGroup(3);
    player1MatrixBuilder.addNextValue(3, 3, 1);
    player1MatrixBuilder.newRowGroup(4);
    player1MatrixBuilder.addNextValue(4, 4, 1);
    storm::storage::SparseMatrix<storm::storage::sparse::state_type> player1Matrix = player1MatrixBuilder.build();

    storm::solver::GameSolverFactory<ValueType> factory;
    auto solver = factory.create(this->env(), player1Matrix, player2Matrix);

    // Create solution and target state vector.
    std::vector<ValueType> result(4);
    std::vector<ValueType> b(7);
    b[4] = this->parseNumber("1");
    b[6] = this->parseNumber("1");

    // Now solve the game with different strategies for the players.
    solver->solveGame(this->env(), storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, result, b);
    EXPECT_NEAR(this->parseNumber("0"), result[0], this->precision());

    result = std::vector<ValueType>(4);
    solver->setBounds(this->parseNumber("0"), this->parseNumber("1"));

    solver->solveGame(this->env(), storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize, result, b);
    EXPECT_NEAR(this->parseNumber("0.5"), result[0], this->precision());

    result = std::vector<ValueType>(4);

    solver->solveGame(this->env(), storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize, result, b);
    EXPECT_NEAR(this->parseNumber("0.2"), result[0], this->precision());

    result = std::vector<ValueType>(4);

    solver->solveGame(this->env(), storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, result, b);
    EXPECT_NEAR(this->parseNumber("1"), result[0], this->precision());
}

}  // namespace
