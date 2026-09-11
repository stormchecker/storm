#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"

/*
 * These tests are about the sound bounds that a model checking call reports alongside its values, not about the
 * values themselves. The claim under test is the one the bounds make, so every property here has an answer that
 * is known in closed form and each case asserts
 *
 *     lower <= exact <= upper   and   lower <= reported value <= upper.
 *
 * Every configuration listed below is one that must report both bounds, so a path that silently stops reporting
 * them fails these tests rather than passing them vacuously.
 */

namespace {

class NativeSviEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        return env;
    }
};

class NativeIiEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::IntervalIteration);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::IntervalIteration);
        return env;
    }
};

class NativeOviEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
        return env;
    }
};

class NativeGviEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::GuessingValueIteration);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::GuessingValueIteration);
        return env;
    }
};

// The exact configurations end up at the solution rather than approaching it, so their two bounds must not merely
// enclose the answer but coincide with it. Policy iteration reports whatever its inner linear solver established,
// which is what makes it exact here.
class EliminationEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};

class EigenLuEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};

class RationalSearchEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch);
        return env;
    }
};

template<typename TestType>
class SolutionBoundsTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef storm::utility::ExtendedValueType<ValueType> ExtendedValueType;
    typedef storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType> QuantitativeResult;

    SolutionBoundsTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }

    ExtendedValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ExtendedValueType>(storm::utility::convertNumber<ValueType>(input));
    }

    ValueType parseValue(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

    /*!
     * How far outside the reported enclosure the exact answer may still lie. A bound is only ever as sound as the
     * arithmetic it is computed in: a procedure working in doubles can land a bound an ulp or two on the wrong
     * side of an answer that is not representable at all, as 11/3 is not. That is the rounding we accept, not a
     * gap in the reasoning, so an inexact configuration is given a slack far below its own solver precision while
     * an exact one is given none.
     */
    ExtendedValueType tolerance() const {
        return this->parseNumber(TestType::isExact ? "0" : "1e-12");
    }

    /*!
     * Builds the given PRISM model and checks the given property on it.
     */
    template<typename ModelType>
    std::unique_ptr<storm::modelchecker::CheckResult> check(std::string const& modelFile, std::string const& propertyString) const {
        // The CTMC test files spell their rates the way PRISM does, which storm accepts in compatibility mode only.
        bool const prismCompatibility = std::is_same_v<ModelType, storm::models::sparse::Ctmc<ValueType>>;
        storm::prism::Program program = storm::api::parseProgram(modelFile, prismCompatibility);
        program = program.preprocess();
        auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(propertyString, program));
        auto model = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<ModelType>();
        storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formulas.front(), true);
        if constexpr (std::is_same_v<ModelType, storm::models::sparse::Dtmc<ValueType>>) {
            return storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>(*model).check(this->env(), task);
        } else if constexpr (std::is_same_v<ModelType, storm::models::sparse::Mdp<ValueType>>) {
            return storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>(*model).check(this->env(), task);
        } else {
            return storm::modelchecker::SparseCtmcCslModelChecker<ModelType>(*model).check(this->env(), task);
        }
    }

    /*!
     * Asserts that the bounds the given result reports for the initial state enclose the given exact answer, and
     * that the value it reports lies between them as well.
     */
    void expectEncloses(std::unique_ptr<storm::modelchecker::CheckResult> const& result, ExtendedValueType const& exact) const {
        QuantitativeResult const& quantitative = result->template asExplicitQuantitativeCheckResult<ValueType>();
        ASSERT_TRUE(quantitative.hasLowerBounds()) << "No lower bound on the solution was reported at all.";
        ASSERT_TRUE(quantitative.hasUpperBounds()) << "No upper bound on the solution was reported at all.";
        uint64_t const offset = 0;
        ExtendedValueType const& value = quantitative.getValueVector()[offset];
        ExtendedValueType const& lower = quantitative.getLowerBoundVector()[offset];
        ExtendedValueType const& upper = quantitative.getUpperBoundVector()[offset];
        EXPECT_LE(lower, exact + this->tolerance()) << "The lower bound exceeds the exact answer.";
        EXPECT_LE(exact, upper + this->tolerance()) << "The upper bound falls below the exact answer.";
        EXPECT_LE(lower, value) << "The lower bound exceeds the reported value.";
        EXPECT_LE(value, upper) << "The reported value exceeds the upper bound.";
        if (TestType::isExact) {
            EXPECT_EQ(exact, lower) << "An exact procedure reported a lower bound strictly below the answer.";
            EXPECT_EQ(exact, upper) << "An exact procedure reported an upper bound strictly above the answer.";
        }
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<NativeSviEnvironment, NativeIiEnvironment, NativeOviEnvironment, NativeGviEnvironment, EliminationEnvironment, EigenLuEnvironment,
                         RationalSearchEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(SolutionBoundsTest, TestingTypes, );

// The Knuth-Yao die yields a one with probability exactly 1/6 and takes 11/3 coin flips on average.
TYPED_TEST(SolutionBoundsTest, DtmcUntilProbabilities) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Dtmc<ValueType>>(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F s=7 & d=1]");
    this->expectEncloses(result, this->parseNumber("1/6"));
}

TYPED_TEST(SolutionBoundsTest, DtmcReachabilityRewards) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Dtmc<ValueType>>(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "R=? [F s=7]");
    this->expectEncloses(result, this->parseNumber("11/3"));
}

TYPED_TEST(SolutionBoundsTest, MdpUntilProbabilities) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Mdp<ValueType>>(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm",
                                                                              "Pmin=? [F \"finished\" & \"all_coins_equal_1\"]");
    this->expectEncloses(result, this->parseNumber("49/128"));
}

TYPED_TEST(SolutionBoundsTest, MdpReachabilityRewards) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Mdp<ValueType>>(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Rmax=? [F \"finished\"]");
    this->expectEncloses(result, this->parseNumber("75"));
}

// In the five state CTMC, state three is reached after 91/24 time units and 23/8 units of "rew1" on average.
TYPED_TEST(SolutionBoundsTest, CtmcReachabilityTimes) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Ctmc<ValueType>>(STORM_TEST_RESOURCES_DIR "/ctmc/simple2.sm", "T=? [F s=3]");
    this->expectEncloses(result, this->parseNumber("91/24"));
}

TYPED_TEST(SolutionBoundsTest, CtmcReachabilityRewards) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Ctmc<ValueType>>(STORM_TEST_RESOURCES_DIR "/ctmc/simple2.sm", "R{\"rew1\"}=? [F s=3]");
    this->expectEncloses(result, this->parseNumber("23/8"));
}

/*
 * A four state CTMC whose reachability probability is not representable in binary:
 *
 *   0 --1--> 1 (target)        p(0) = 1/3 + 2/3 p(2)
 *   0 --2--> 2                 p(2) = 3/4 p(0)         so p(0) = 2/3.
 *   2 --3--> 0
 *   2 --1--> 3 (a dead end)
 */
TYPED_TEST(SolutionBoundsTest, CtmcUntilProbabilities) {
    typedef typename TestFixture::ValueType ValueType;
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 4, 6);
    builder.addNextValue(0, 1, this->parseValue("1"));
    builder.addNextValue(0, 2, this->parseValue("2"));
    builder.addNextValue(1, 1, this->parseValue("1"));
    builder.addNextValue(2, 0, this->parseValue("3"));
    builder.addNextValue(2, 3, this->parseValue("1"));
    builder.addNextValue(3, 3, this->parseValue("1"));

    storm::models::sparse::StateLabeling labeling(4);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("target");
    labeling.addLabelToState("target", 1);
    storm::models::sparse::Ctmc<ValueType> ctmc(builder.build(), labeling);

    storm::parser::FormulaParser formulaParser;
    auto formula = formulaParser.parseSingleFormulaFromString("P=? [F \"target\"]");
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);
    auto result = storm::modelchecker::SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<ValueType>>(ctmc).check(this->env(), task);
    this->expectEncloses(result, this->parseNumber("2/3"));
}

/*
 * A property that the graph analysis settles on its own never reaches an equation solver, but its values are
 * known exactly all the same, so they must be reported as their own bounds rather than as no bound at all. In
 * this three state chain the target is reached almost surely from the first two states and not at all from the
 * third, and every configuration must say so exactly.
 */
TYPED_TEST(SolutionBoundsTest, QualitativelySettledValuesAreExact) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::ExtendedValueType ExtendedValueType;

    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3, 3);
    builder.addNextValue(0, 1, this->parseValue("1"));
    builder.addNextValue(1, 1, this->parseValue("1"));
    builder.addNextValue(2, 2, this->parseValue("1"));

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("target");
    labeling.addLabelToState("target", 1);
    storm::models::sparse::Dtmc<ValueType> dtmc(builder.build(), labeling);

    storm::parser::FormulaParser formulaParser;
    auto formula = formulaParser.parseSingleFormulaFromString("P=? [F \"target\"]");
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);
    auto result = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>>(dtmc).check(this->env(), task);

    auto const& quantitative = result->template asExplicitQuantitativeCheckResult<ValueType>();
    ASSERT_TRUE(quantitative.hasLowerBounds()) << "No lower bound was reported for a value the graph analysis settled.";
    ASSERT_TRUE(quantitative.hasUpperBounds()) << "No upper bound was reported for a value the graph analysis settled.";
    std::vector<ExtendedValueType> const expected{this->parseNumber("1"), this->parseNumber("1"), this->parseNumber("0")};
    EXPECT_EQ(expected, quantitative.getValueVector());
    EXPECT_EQ(expected, quantitative.getLowerBoundVector());
    EXPECT_EQ(expected, quantitative.getUpperBoundVector());
}

/*
 * The aggregating filters report the aggregate of each bound alongside the aggregate of the values. Checking that
 * against a model would only ever exercise whichever enclosure the solver happened to produce, so the result here
 * is built by hand: three values, each enclosed by a bound that is deliberately loose in both directions.
 */
TYPED_TEST(SolutionBoundsTest, AggregationCarriesTheBounds) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::ExtendedValueType ExtendedValueType;
    using storm::modelchecker::FilterType;

    std::vector<ExtendedValueType> const values{this->parseNumber("1"), this->parseNumber("2"), this->parseNumber("6")};
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType> result(values);
    result.setLowerBounds({this->parseNumber("0"), this->parseNumber("1"), this->parseNumber("5")});
    result.setUpperBounds({this->parseNumber("2"), this->parseNumber("4"), this->parseNumber("7")});

    auto const min = result.aggregate(FilterType::MIN);
    EXPECT_EQ(this->parseNumber("1"), min.value);
    ASSERT_TRUE(min.hasLower() && min.hasUpper());
    EXPECT_EQ(this->parseNumber("0"), *min.lower);
    EXPECT_EQ(this->parseNumber("2"), *min.upper);

    auto const max = result.aggregate(FilterType::MAX);
    EXPECT_EQ(this->parseNumber("6"), max.value);
    ASSERT_TRUE(max.hasLower() && max.hasUpper());
    EXPECT_EQ(this->parseNumber("5"), *max.lower);
    EXPECT_EQ(this->parseNumber("7"), *max.upper);

    auto const sum = result.aggregate(FilterType::SUM);
    EXPECT_EQ(this->parseNumber("9"), sum.value);
    ASSERT_TRUE(sum.hasLower() && sum.hasUpper());
    EXPECT_EQ(this->parseNumber("6"), *sum.lower);
    EXPECT_EQ(this->parseNumber("13"), *sum.upper);

    auto const average = result.aggregate(FilterType::AVG);
    EXPECT_EQ(this->parseNumber("3"), average.value);
    ASSERT_TRUE(average.hasLower() && average.hasUpper());
    EXPECT_EQ(this->parseNumber("2"), *average.lower);
    EXPECT_EQ(this->parseNumber("13/3"), *average.upper);
}

// A result that carries no bounds must aggregate to an aggregate without any, rather than to one that quietly
// repeats the value as though it were a bound on itself.
TYPED_TEST(SolutionBoundsTest, AggregationWithoutBounds) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::ExtendedValueType ExtendedValueType;

    std::vector<ExtendedValueType> const values{this->parseNumber("1"), this->parseNumber("2")};
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType> const result(values);
    auto const sum = result.aggregate(storm::modelchecker::FilterType::SUM);
    EXPECT_EQ(this->parseNumber("3"), sum.value);
    EXPECT_FALSE(sum.hasLower());
    EXPECT_FALSE(sum.hasUpper());
}

/*
 * The whole point of these bounds is that they do not wait for convergence: sound value iteration maintains an
 * enclosure of the solution at every step, so cutting it short must still yield an enclosure, only a wider one.
 * This is checked on its own rather than as a typed test because how far a given method gets in a given number of
 * iterations is a property of that method, not something the other configurations share.
 */
TEST(SolutionBoundsTest, AbortedSoundValueIterationStillEncloses) {
    auto environmentWithIterationCap = [](uint64_t maximalIterations) {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setMaximalNumberOfIterations(maximalIterations);
        return env;
    };

    storm::prism::Program program = storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm").preprocess();
    auto formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram("Pmin=? [F \"finished\" & \"all_coins_equal_1\"]", program));
    auto model = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    storm::modelchecker::CheckTask<storm::logic::Formula, double> task(*formulas.front(), true);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*model);

    // The answer is 49/128, and twenty iterations are not nearly enough to reach it.
    double const exact = 49.0 / 128.0;
    auto const aborted = checker.check(environmentWithIterationCap(20), task)->asExplicitQuantitativeCheckResult<double>().getSolutionBounds();
    auto const converged = checker.check(environmentWithIterationCap(100), task)->asExplicitQuantitativeCheckResult<double>().getSolutionBounds();

    ASSERT_TRUE(aborted.hasLower() && aborted.hasUpper()) << "An aborted run reported no bounds at all.";
    ASSERT_TRUE(converged.hasLower() && converged.hasUpper());
    EXPECT_LE((*aborted.lower)[0], exact) << "The lower bound of an aborted run exceeds the exact answer.";
    EXPECT_LE(exact, (*aborted.upper)[0]) << "The upper bound of an aborted run falls below the exact answer.";
    EXPECT_LT((*aborted.lower)[0], (*converged.lower)[0]) << "Stopping earlier did not give a wider enclosure.";
    EXPECT_LT((*converged.upper)[0], (*aborted.upper)[0]) << "Stopping earlier did not give a wider enclosure.";
}

}  // namespace
