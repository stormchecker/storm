#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
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

class NativeIiEnvironment {
   public:
    typedef double ValueType;
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
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
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

    /*!
     * How far outside the reported enclosure the exact answer may still lie. A bound is only ever as sound as the
     * arithmetic it is computed in: a procedure working in doubles can land a bound an ulp or two on the wrong
     * side of an answer that is not representable at all. That is the rounding we accept, not a gap in the
     * reasoning, so the slack is far below the solver precision these configurations ask for.
     */
    ExtendedValueType tolerance() const {
        return this->parseNumber("1e-12");
    }

    /*!
     * Builds the given PRISM model and checks the given property on it.
     */
    template<typename ModelType>
    std::unique_ptr<storm::modelchecker::CheckResult> check(std::string const& modelFile, std::string const& propertyString) const {
        storm::prism::Program program = storm::api::parseProgram(modelFile);
        program = program.preprocess();
        auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(propertyString, program));
        auto model = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<ModelType>();
        storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formulas.front(), true);
        if constexpr (std::is_same_v<ModelType, storm::models::sparse::Dtmc<ValueType>>) {
            return storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>(*model).check(this->env(), task);
        } else {
            return storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>(*model).check(this->env(), task);
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
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<NativeIiEnvironment, NativeOviEnvironment> TestingTypes;

TYPED_TEST_SUITE(SolutionBoundsTest, TestingTypes, );

// The Knuth-Yao die yields a one with probability exactly 1/6.
TYPED_TEST(SolutionBoundsTest, DtmcUntilProbabilities) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Dtmc<ValueType>>(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F s=7 & d=1]");
    this->expectEncloses(result, this->parseNumber("1/6"));
}

TYPED_TEST(SolutionBoundsTest, MdpUntilProbabilities) {
    typedef typename TestFixture::ValueType ValueType;
    auto result = this->template check<storm::models::sparse::Mdp<ValueType>>(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm",
                                                                              "Pmin=? [F \"finished\" & \"all_coins_equal_1\"]");
    this->expectEncloses(result, this->parseNumber("49/128"));
}

}  // namespace
