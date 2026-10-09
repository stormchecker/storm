#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/storage/SparseMatrix.h"

namespace {

class DoubleViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class DoubleViRegMultEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().minMax().setMultiplicationStyle(storm::solver::MultiplicationStyle::Regular);
        return env;
    }
};

class DoubleSoundViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().setForceSoundness(true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class DoubleIntervalIterationEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::IntervalIteration);
        env.solver().setForceSoundness(true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class DoubleGuessingViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::GuessingValueIteration);
        env.solver().setForceSoundness(true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class DoubleOptimisticViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
        env.solver().setForceSoundness(true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class DoubleTopologicalViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
        env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class DoublePIEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static const bool reportsBounds = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
        env.solver().setLinearEquationSolverPrecision(env.solver().minMax().getPrecision());
        return env;
    }
};
class RationalPIEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};
class RationalRationalSearchEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static const bool reportsBounds = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch);
        return env;
    }
};

template<typename TestType>
class MinMaxLinearEquationSolverTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    MinMaxLinearEquationSolverTest() : _environment(TestType::createEnvironment()) {}
    storm::Environment const& env() const {
        return _environment;
    }
    ValueType precision() const {
        return TestType::isExact ? parseNumber("0") : parseNumber("1e-6");
    }
    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }
    bool reportsBounds() const {
        return TestType::reportsBounds;
    }

    /*!
     * Expects the solver to report bounds on the solution that enclose the given value.
     */
    void expectSolutionBoundsEnclose(storm::solver::MinMaxLinearEquationSolver<ValueType> const& solver, uint64_t index, ValueType const& value) const {
        ASSERT_TRUE(solver.hasSolutionLowerBounds()) << "No lower bound on the solution was reported.";
        ASSERT_TRUE(solver.hasSolutionUpperBounds()) << "No upper bound on the solution was reported.";
        EXPECT_LE(solver.getSolutionLowerBounds()[index], value + precision()) << "The lower bound exceeds the solution.";
        EXPECT_LE(value, solver.getSolutionUpperBounds()[index] + precision()) << "The upper bound falls below the solution.";
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DoubleViEnvironment, DoubleViRegMultEnvironment, DoubleSoundViEnvironment, DoubleIntervalIterationEnvironment,
                         DoubleOptimisticViEnvironment, DoubleGuessingViEnvironment, DoubleTopologicalViEnvironment, DoublePIEnvironment, RationalPIEnvironment,
                         RationalRationalSearchEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(MinMaxLinearEquationSolverTest, TestingTypes, );

TYPED_TEST(MinMaxLinearEquationSolverTest, SolveEquations) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> builder(0, 0, 0, false, true);
    ASSERT_NO_THROW(builder.newRowGroup(0));
    ASSERT_NO_THROW(builder.addNextValue(0, 0, this->parseNumber("0.9")));

    storm::storage::SparseMatrix<ValueType> A;
    ASSERT_NO_THROW(A = builder.build(2));

    std::vector<ValueType> x(1);
    std::vector<ValueType> b = {this->parseNumber("0.099"), this->parseNumber("0.5")};

    auto factory = storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType>();
    auto solver = factory.create(this->env(), A);
    solver->setHasUniqueSolution(true);
    solver->setHasNoEndComponents(true);
    solver->setBounds(this->parseNumber("0"), this->parseNumber("2"));
    storm::solver::MinMaxLinearEquationSolverRequirements req = solver->getRequirements(this->env());
    req.clearBounds();
    ASSERT_FALSE(req.hasEnabledRequirement());
    ASSERT_NO_THROW(solver->solveEquations(this->env(), storm::OptimizationDirection::Minimize, x, b));
    EXPECT_NEAR(x[0], this->parseNumber("0.5"), this->precision());
    if (this->reportsBounds()) {
        this->expectSolutionBoundsEnclose(*solver, 0, this->parseNumber("0.5"));
    }

    ASSERT_NO_THROW(solver->solveEquations(this->env(), storm::OptimizationDirection::Maximize, x, b));
    EXPECT_NEAR(x[0], this->parseNumber("0.99"), this->precision());
    if (this->reportsBounds()) {
        this->expectSolutionBoundsEnclose(*solver, 0, this->parseNumber("0.99"));
    }
}

TEST(MinMaxLinearEquationSolverTest, TopologicalWithoutCyclesIsExact) {
    // Two row groups whose only cycles are self loops, so the topological solver settles both by substitution.
    storm::storage::SparseMatrixBuilder<double> builder(0, 0, 0, false, true);
    builder.newRowGroup(0);
    builder.addNextValue(0, 0, 0.5);
    builder.addNextValue(0, 1, 0.25);
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, 0.5);
    storm::storage::SparseMatrix<double> A = builder.build(2, 2, 2);

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
    env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::vector<double> x(2);
    std::vector<double> b = {0.0, 0.5};

    auto solver = storm::solver::GeneralMinMaxLinearEquationSolverFactory<double>().create(env, A);
    solver->setHasUniqueSolution(true);
    solver->setHasNoEndComponents(true);
    solver->setRequirementsChecked(true);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    EXPECT_TRUE(solver->hasExactSolutionBounds()) << "An acyclic topological solve should report the values as exact.";
    ASSERT_TRUE(solver->hasSolutionLowerBounds());
    ASSERT_TRUE(solver->hasSolutionUpperBounds());
    EXPECT_EQ(x, solver->getSolutionLowerBounds());
    EXPECT_EQ(x, solver->getSolutionUpperBounds());
}
// Four states forming the two non-trivial SCCs {0, 1} and {2, 3}. The solution is 5/18, 2/9, 1/3 and 1/6.
storm::storage::SparseMatrix<double> buildTwoSccMatrix() {
    storm::storage::SparseMatrixBuilder<double> builder(0, 0, 0, false, true);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.newRowGroup(1);
    builder.addNextValue(1, 0, 0.5);
    builder.addNextValue(1, 2, 0.25);
    builder.newRowGroup(2);
    builder.addNextValue(2, 3, 0.5);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, 0.5);
    return builder.build(4, 4, 4);
}

std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<double>> createSolver(storm::Environment const& env, storm::storage::SparseMatrix<double> const& A) {
    auto solver = storm::solver::GeneralMinMaxLinearEquationSolverFactory<double>().create(env, A);
    solver->setHasUniqueSolution(true);
    solver->setHasNoEndComponents(true);
    solver->setRequirementsChecked(true);
    return solver;
}

TEST(MinMaxLinearEquationSolverTest, ValueIterationFromBelowReportsLowerBound) {
    auto A = buildTwoSccMatrix();
    std::vector<double> b = {0.0, 0.0, 0.25, 0.0};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
    env.solver().setForceSoundness(true);

    std::vector<double> x(4);
    auto solver = createSolver(env, A);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    // Starting from zero, no sweep ever decreases a value, which places the iterate below the fixed point.
    ASSERT_TRUE(solver->hasSolutionLowerBounds());
    EXPECT_FALSE(solver->hasSolutionUpperBounds());
    EXPECT_LE(solver->getSolutionLowerBounds()[0], 5.0 / 18.0);
}

TEST(MinMaxLinearEquationSolverTest, ValueIterationWithoutSoundnessReportsNoBounds) {
    auto A = buildTwoSccMatrix();
    std::vector<double> b = {0.0, 0.0, 0.25, 0.0};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));

    std::vector<double> x(4);
    auto solver = createSolver(env, A);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    EXPECT_FALSE(solver->hasSolutionLowerBounds());
    EXPECT_FALSE(solver->hasSolutionUpperBounds());
}

TEST(MinMaxLinearEquationSolverTest, InitialSchedulerDoesNotYieldBounds) {
    storm::storage::SparseMatrixBuilder<double> builder(0, 0, 0, false, true);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(1, 1, 0.25);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, 0.5);
    storm::storage::SparseMatrix<double> A = builder.build(3, 2, 2);
    std::vector<double> b = {0.0, 0.0, 0.25};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
    env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
    env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);

    std::vector<double> x(2);
    auto solver = createSolver(env, A);
    solver->setInitialScheduler({1, 0});
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));
    EXPECT_NEAR(x[0], 0.25, 1e-6);

    // The value of the initial scheduler bounds the solution, but only as accurately as it was computed itself.
    EXPECT_FALSE(solver->hasSolutionLowerBounds());
    EXPECT_FALSE(solver->hasSolutionUpperBounds());
}

TEST(MinMaxLinearEquationSolverTest, PolicyIterationWithInexactInnerSolveAddsNoBounds) {
    auto A = buildTwoSccMatrix();
    std::vector<double> b = {0.0, 0.0, 0.25, 0.0};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
    env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
    env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration);

    std::vector<double> x(4);
    auto solver = createSolver(env, A);
    solver->setBounds(0.0, 1.0);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    // The inner solver encloses the values of the scheduler it was handed, but an inexact inner solve may have
    // let policy iteration stop at a suboptimal one, which those bounds say nothing about. So the reported
    // bounds are the ones that were known beforehand.
    ASSERT_TRUE(solver->hasSolutionLowerBounds());
    ASSERT_TRUE(solver->hasSolutionUpperBounds());
    EXPECT_EQ(0.0, solver->getSolutionLowerBounds()[0]);
    EXPECT_EQ(1.0, solver->getSolutionUpperBounds()[0]);
}

TEST(MinMaxLinearEquationSolverTest, TopologicalWithUnsoundSccMethodReportsNoBounds) {
    auto A = buildTwoSccMatrix();
    std::vector<double> b = {0.0, 0.0, 0.25, 0.0};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
    env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::ValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
    env.solver().setForceSoundness(true);

    std::vector<double> x(4);
    auto solver = createSolver(env, A);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    // The precision the SCCs were solved with is only an error bound if they certify it, which plain value
    // iteration does not.
    EXPECT_FALSE(solver->hasSolutionLowerBounds());
    EXPECT_FALSE(solver->hasSolutionUpperBounds());
}

TEST(MinMaxLinearEquationSolverTest, TopologicalWithSoundSccMethodReportsBounds) {
    auto A = buildTwoSccMatrix();
    std::vector<double> b = {0.0, 0.0, 0.25, 0.0};

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
    env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::SoundValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
    env.solver().setForceSoundness(true);

    std::vector<double> x(4);
    auto solver = createSolver(env, A);
    solver->setBounds(0.0, 1.0);
    ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Maximize, x, b));

    ASSERT_TRUE(solver->hasSolutionLowerBounds());
    ASSERT_TRUE(solver->hasSolutionUpperBounds());
    EXPECT_LE(solver->getSolutionLowerBounds()[0], 5.0 / 18.0);
    EXPECT_LE(5.0 / 18.0, solver->getSolutionUpperBounds()[0]);
}
}  // namespace
