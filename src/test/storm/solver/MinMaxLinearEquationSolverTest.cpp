#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/TerminationCondition.h"
#include "storm/storage/SparseMatrix.h"

namespace {

class DoubleViEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
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

    ASSERT_NO_THROW(solver->solveEquations(this->env(), storm::OptimizationDirection::Maximize, x, b));
    EXPECT_NEAR(x[0], this->parseNumber("0.99"), this->precision());
}

TEST(MinMaxLinearEquationSolverTest, SoundMethodsTerminateEarly) {
    // State 0 has value 1 but converges slowly due to its self-loop, state 1 has value 0.5.
    // The thresholds are already met after the first iteration, so every sound method should stop far from convergence.
    storm::storage::SparseMatrixBuilder<double> builder(0, 0, 0, false, true);
    builder.newRowGroup(0);
    builder.addNextValue(0, 0, 0.99);
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, 0.5);
    storm::storage::SparseMatrix<double> A = builder.build(2, 2, 2);
    std::vector<double> b = {0.01, 0.25};
    storm::storage::BitVector filter(2, std::vector<uint64_t>{0});

    for (auto method : {storm::solver::MinMaxMethod::SoundValueIteration, storm::solver::MinMaxMethod::IntervalIteration,
                        storm::solver::MinMaxMethod::GuessingValueIteration}) {
        for (bool useLowerBound : {true, false}) {
            SCOPED_TRACE(storm::solver::toString(method) + (useLowerBound ? " with lower bound" : " with upper bound"));
            storm::Environment env;
            env.solver().minMax().setMethod(method);
            env.solver().setForceSoundness(true);
            env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));

            auto solver = storm::solver::GeneralMinMaxLinearEquationSolverFactory<double>().create(env, A);
            solver->setHasUniqueSolution(true);
            solver->setHasNoEndComponents(true);
            solver->setBounds(0.0, 10.0);
            if (useLowerBound) {
                solver->setTerminationCondition(
                    std::make_unique<storm::solver::TerminateIfFilteredExtremumExceedsThreshold<double>>(filter, false, 0.005, false));
            } else {
                solver->setTerminationCondition(std::make_unique<storm::solver::TerminateIfFilteredExtremumBelowThreshold<double>>(filter, false, 9.95, true));
            }
            std::vector<double> x(2);
            ASSERT_NO_THROW(solver->solveEquations(env, storm::OptimizationDirection::Minimize, x, b));
            // Without early termination, x[0] would be within the precision of 1.
            EXPECT_GT(std::abs(x[0] - 1.0), 1e-3) << "x[0] = " << x[0];
        }
    }
}
}  // namespace
