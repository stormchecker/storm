#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/solver/EliminationLinearEquationSolver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/TerminationCondition.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace {

class NativeDoublePowerEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-10"));
        return env;
    }
};

class NativeDoublePowerRegMultEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-10"));
        env.solver().native().setPowerMethodMultiplicationStyle(storm::solver::MultiplicationStyle::Regular);
        return env;
    }
};

class NativeDoubleSoundValueIterationEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration);
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-6"));
        return env;
    }
};

class NativeDoubleOptimisticValueIterationEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration);
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-6"));
        return env;
    }
};

class NativeDoubleIntervalIterationEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::IntervalIteration);
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-6"));
        return env;
    }
};

class NativeDoubleGuessingViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::GuessingValueIteration);
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-6"));
        return env;
    }
};

class NativeDoubleJacobiEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-10"));
        return env;
    }
};

class NativeDoubleGaussSeidelEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::GaussSeidel);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-10"));
        return env;
    }
};

class NativeDoubleSorEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-10"));
        return env;
    }
};

class NativeDoubleWalkerChaeEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        env.solver().native().setMaximalNumberOfIterations(500000);
        return env;
    }
};

class NativeRationalRationalSearchEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
        return env;
    }
};

class EliminationRationalEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
        return env;
    }
};

#ifdef STORM_HAVE_GMM
class GmmGmresIluEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class GmmGmresDiagonalEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Diagonal);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class GmmGmresNoneEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::None);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class GmmBicgstabIluEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Bicgstab);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class GmmQmrDiagonalEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Qmr);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Diagonal);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};
#endif

class EigenDGmresDiagonalEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Diagonal);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class EigenGmresIluEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::Gmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class EigenBicgstabNoneEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::Bicgstab);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::None);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-8"));
        return env;
    }
};

class EigenDoubleLUEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class EigenRationalLUEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class TopologicalEigenRationalLUEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Topological);
        env.solver().topological().setUnderlyingEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

template<typename TestType>
class LinearEquationSolverTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    LinearEquationSolverTest() : _environment(TestType::createEnvironment()) {}
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

typedef ::testing::Types<NativeDoublePowerEnvironment, NativeDoublePowerRegMultEnvironment, NativeDoubleSoundValueIterationEnvironment,
                         NativeDoubleOptimisticValueIterationEnvironment, NativeDoubleGuessingViEnvironment, NativeDoubleIntervalIterationEnvironment,
                         NativeDoubleJacobiEnvironment, NativeDoubleGaussSeidelEnvironment, NativeDoubleSorEnvironment, NativeDoubleWalkerChaeEnvironment,
                         NativeRationalRationalSearchEnvironment, EliminationRationalEnvironment,
#ifdef STORM_HAVE_GMM
                         GmmGmresIluEnvironment, GmmGmresDiagonalEnvironment, GmmGmresNoneEnvironment, GmmBicgstabIluEnvironment, GmmQmrDiagonalEnvironment,
#endif
                         EigenDGmresDiagonalEnvironment, EigenGmresIluEnvironment, EigenBicgstabNoneEnvironment, EigenDoubleLUEnvironment,
                         EigenRationalLUEnvironment, TopologicalEigenRationalLUEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(LinearEquationSolverTest, TestingTypes, );

TYPED_TEST(LinearEquationSolverTest, solveEquationSystem) {
    typedef typename TestFixture::ValueType ValueType;
    ASSERT_NO_THROW(storm::storage::SparseMatrixBuilder<ValueType> builder);
    storm::storage::SparseMatrixBuilder<ValueType> builder;
    ASSERT_NO_THROW(builder.addNextValue(0, 0, this->parseNumber("1/5")));
    ASSERT_NO_THROW(builder.addNextValue(0, 1, this->parseNumber("2/5")));
    ASSERT_NO_THROW(builder.addNextValue(0, 2, this->parseNumber("2/5")));
    ASSERT_NO_THROW(builder.addNextValue(1, 0, this->parseNumber("1/50")));
    ASSERT_NO_THROW(builder.addNextValue(1, 1, this->parseNumber("48/50")));
    ASSERT_NO_THROW(builder.addNextValue(1, 2, this->parseNumber("1/50")));
    ASSERT_NO_THROW(builder.addNextValue(2, 0, this->parseNumber("4/10")));
    ASSERT_NO_THROW(builder.addNextValue(2, 1, this->parseNumber("3/10")));
    ASSERT_NO_THROW(builder.addNextValue(2, 2, this->parseNumber("0")));

    storm::storage::SparseMatrix<ValueType> A;
    ASSERT_NO_THROW(A = builder.build());

    std::vector<ValueType> x(3);
    std::vector<ValueType> b = {this->parseNumber("3"), this->parseNumber("-0.01"), this->parseNumber("12")};

    auto factory = storm::solver::GeneralLinearEquationSolverFactory<ValueType>();
    if (factory.getEquationProblemFormat(this->env()) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem) {
        A.convertToEquationSystem();
    }

    auto requirements = factory.getRequirements(this->env());
    requirements.clearUpperBounds();
    requirements.clearLowerBounds();
    ASSERT_FALSE(requirements.hasEnabledRequirement());
    auto solver = factory.create(this->env(), A);
    solver->setBounds(this->parseNumber("-100"), this->parseNumber("100"));
    ASSERT_NO_THROW(solver->solveEquations(this->env(), x, b));
    EXPECT_NEAR(x[0], this->parseNumber("481/9"), this->precision());
    EXPECT_NEAR(x[1], this->parseNumber("457/9"), this->precision());
    EXPECT_NEAR(x[2], this->parseNumber("875/18"), this->precision());
}

TEST(IntervalIterationJustPreciseEnough, Native) {
    storm::storage::SparseMatrixBuilder<double> builder;
    builder.addNextValue(0, 0, 0.9);
    auto matrix = builder.build();
    std::vector<double> b = {0.099};  // Fixed point 0.99.
    double const precision = 1e-3;

    for (bool relative : {false, true}) {
        SCOPED_TRACE(::testing::Message() << "relative=" << relative);
        auto env = NativeDoubleIntervalIterationEnvironment::createEnvironment();
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(precision));
        env.solver().native().setRelativeTerminationCriterion(relative);
        auto solver = storm::solver::GeneralLinearEquationSolverFactory<double>().create(env, matrix);
        solver->setBounds(0.0, 1.0);
        std::vector<double> x(1);
        ASSERT_TRUE(solver->solveEquations(env, x, b));
        ASSERT_TRUE(solver->hasSolutionLowerBounds());
        ASSERT_TRUE(solver->hasSolutionUpperBounds());
        double const lower = solver->getSolutionLowerBounds()[0];
        double const upper = solver->getSolutionUpperBounds()[0];
        double const tolerance = precision * (relative ? lower : 1.0);
        EXPECT_LE(lower, 0.99);
        EXPECT_GE(upper, 0.99);
        EXPECT_DOUBLE_EQ(x[0], (lower + upper) / 2.0);
        EXPECT_NEAR(x[0], 0.99, precision * (relative ? 0.99 : 1.0));
        EXPECT_LE(upper - lower, 2.0 * tolerance);
        // With this slow contraction, stopping at half this width takes unnecessary iterations.
        EXPECT_GT(upper - lower, tolerance);
    }
}

TEST(GuessingValueIterationPrecision, NativeSmallValues) {
    // Also cover negative solutions and an exact zero when checking relative convergence.
    storm::storage::SparseMatrixBuilder<double> builder(3, 3, 2);
    builder.addNextValue(0, 0, 0.9);
    builder.addNextValue(1, 1, 0.9);
    auto matrix = builder.build();

    for (bool relative : {false, true}) {
        for (double scale : {1.0, 1e-8}) {
            SCOPED_TRACE(::testing::Message() << "relative=" << relative << ", scale=" << scale);
            auto env = NativeDoubleGuessingViEnvironment::createEnvironment();
            env.solver().native().setRelativeTerminationCriterion(relative);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-3"));
            env.solver().native().setMaximalNumberOfIterations(10000);
            auto solver = storm::solver::GeneralLinearEquationSolverFactory<double>().create(env, matrix);
            solver->setBounds(-1.0, 1.0);

            std::vector<double> x(3);
            std::vector<double> b = {1e-5 * scale, -1e-5 * scale, 0.0};
            ASSERT_TRUE(solver->solveEquations(env, x, b));
            double expected = 1e-4 * scale;
            double tolerance = relative ? 1e-3 * expected : 1e-3;
            EXPECT_NEAR(x[0], expected, tolerance);
            EXPECT_NEAR(x[1], -expected, tolerance);
            EXPECT_EQ(x[2], 0.0);
        }
    }
}

template<typename ValueType>
void testEliminationWithAbsorbingStates(std::vector<std::vector<std::pair<uint64_t, ValueType>>> const& rows, std::vector<ValueType> const& b,
                                        std::vector<ValueType> const& expected) {
    storm::storage::SparseMatrixBuilder<ValueType> builder(rows.size(), rows.size(), 0);
    for (uint64_t row = 0; row < rows.size(); ++row) {
        for (auto const& entry : rows[row]) {
            builder.addNextValue(row, entry.first, entry.second);
        }
    }
    auto matrix = builder.build();

    storm::Environment env;
    env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
    storm::solver::EliminationLinearEquationSolver<ValueType> solver(matrix);
    std::vector<ValueType> x(b.size());
    ASSERT_NO_THROW(solver.solveEquations(env, x, b));
    ASSERT_EQ(expected.size(), x.size());
    for (uint64_t i = 0; i < x.size(); ++i) {
        EXPECT_EQ(expected[i], x[i]);
    }
}

TEST(EliminationLinearEquationSolver, AbsorbingState) {
    // Regression test for issue #86: the elimination-based solver must handle a single absorbing state
    // (a one on the diagonal), for which the least fixed point is zero.
    auto zero = storm::utility::zero<storm::RationalNumber>();
    testEliminationWithAbsorbingStates<storm::RationalNumber>({{{0, storm::utility::one<storm::RationalNumber>()}}}, {zero}, {zero});
}

TEST(EliminationLinearEquationSolver, AbsorbingTwoCycle) {
    // Regression test for issue #86: when eliminating one state of a probability-1 cycle, the remaining state
    // obtains a one on the diagonal (i.e., it becomes absorbing) and must be handled by the eliminator.
    auto one = storm::utility::one<storm::RationalNumber>();
    auto zero = storm::utility::zero<storm::RationalNumber>();
    testEliminationWithAbsorbingStates<storm::RationalNumber>({{{1, one}}, {{0, one}}}, {zero, zero}, {zero, zero});

    // Also check the double instantiation.
    testEliminationWithAbsorbingStates<double>({{{1, 1.0}}, {{0, 1.0}}}, {0.0, 0.0}, {0.0, 0.0});
}

TEST(EliminationLinearEquationSolver, AbsorbingSinkReachability) {
    // A transient state can either reach the target with probability one half or fall into an absorbing sink.
    auto half = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/2");
    auto one = storm::utility::one<storm::RationalNumber>();
    auto zero = storm::utility::zero<storm::RationalNumber>();
    testEliminationWithAbsorbingStates<storm::RationalNumber>({{{1, half}}, {{2, one}}, {{1, one}}}, {half, zero, zero}, {half, zero, zero});
}

TEST(NativeLinearEquationSolver, SoundMethodsTerminateEarly) {
    // State 0 has value 1 but converges slowly due to its self-loop, state 1 has value 0.5.
    // The thresholds are already met after the first iteration, so every sound method should stop far from convergence.
    storm::storage::SparseMatrixBuilder<double> builder;
    builder.addNextValue(0, 0, 0.99);
    builder.addNextValue(1, 1, 0.5);
    storm::storage::SparseMatrix<double> A = builder.build(2, 2);
    std::vector<double> b = {0.01, 0.25};
    storm::storage::BitVector filter(2, std::vector<uint64_t>{0});

    for (auto method :
         {storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration, storm::solver::NativeLinearEquationSolverMethod::IntervalIteration,
          storm::solver::NativeLinearEquationSolverMethod::GuessingValueIteration}) {
        for (bool useLowerBound : {true, false}) {
            SCOPED_TRACE(storm::solver::toString(method) + (useLowerBound ? " with lower bound" : " with upper bound"));
            storm::Environment env;
            env.solver().setForceSoundness(true);
            env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
            env.solver().native().setMethod(method);
            env.solver().native().setRelativeTerminationCriterion(false);
            env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber, std::string>("1e-6"));

            auto factory = storm::solver::GeneralLinearEquationSolverFactory<double>();
            ASSERT_EQ(factory.getEquationProblemFormat(env), storm::solver::LinearEquationSolverProblemFormat::FixedPointSystem);
            auto solver = factory.create(env, A);
            solver->setBounds(0.0, 10.0);
            if (useLowerBound) {
                solver->setTerminationCondition(
                    std::make_unique<storm::solver::TerminateIfFilteredExtremumExceedsThreshold<double>>(filter, false, 0.005, false));
            } else {
                solver->setTerminationCondition(std::make_unique<storm::solver::TerminateIfFilteredExtremumBelowThreshold<double>>(filter, false, 9.95, true));
            }
            std::vector<double> x(2);
            ASSERT_NO_THROW(solver->solveEquations(env, x, b));
            // Without early termination, x[0] would be within the precision of 1.
            EXPECT_GT(std::abs(x[0] - 1.0), 1e-3) << "x[0] = " << x[0];
        }
    }
}
}  // namespace
