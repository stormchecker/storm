#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm-pomdp/modelchecker/BeliefExplorationPomdpModelChecker.h"
#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"
#include "storm-pomdp/transformer/KnownProbabilityTransformer.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/transformer/MakePOMDPCanonic.h"

namespace {
enum class PreprocessingType { None, SelfloopReduction, QualitativeReduction, All };

class DefaultDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::None;
};

class SelfloopReductionDefaultDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::SelfloopReduction;
};

class QualitativeReductionDefaultDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::QualitativeReduction;
};

class PreprocessedDefaultDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::All;
};

class FineDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.02);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>& options) {
        options.resolutionInit = 24;
    }
    static PreprocessingType const preprocessingType = PreprocessingType::None;
};

class RefineDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.005);
    }
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>& options) {
        options.refine = true;
        options.refinePrecision = precision();
    }
};

class PreprocessedRefineDoubleVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.005);
    }
    static PreprocessingType const preprocessingType = PreprocessingType::All;
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>& options) {
        options.refine = true;
        options.refinePrecision = precision();
    }
};

class DefaultDoubleOVIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().setForceSoundness(true);
        return env;
    }
    static bool const isExactModelChecking = false;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::None;
};

class DefaultRationalPIEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().setForceExact(true);
        return env;
    }
    static bool const isExactModelChecking = true;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::None;
};

class PreprocessedDefaultRationalPIEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().setForceExact(true);
        return env;
    }
    static bool const isExactModelChecking = true;
    static ValueType precision() {
        return storm::utility::convertNumber<ValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static void adaptOptions(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>&) { /* intentionally left empty */ }
    static PreprocessingType const preprocessingType = PreprocessingType::All;
};

template<typename TestType>
class BeliefExplorationPomdpModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    BeliefExplorationPomdpModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> options() const {
        storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> opt(true, true);  // Always compute both bounds (lower and upper)
        opt.gapThresholdInit = 0;
        TestType::adaptOptions(opt);
        return opt;
    }
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> optionsWithStateElimination() const {
        storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> opt(true, true);  // Always compute both bounds (lower and upper)
        opt.gapThresholdInit = 0;
        TestType::adaptOptions(opt);
        opt.useStateEliminationCutoff = true;
        return opt;
    }
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> optionsWithClipping() const {
        storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType> opt(true, true);  // Always compute both bounds (lower and upper)
        opt.gapThresholdInit = 0;
        TestType::adaptOptions(opt);
        opt.useClipping = true;
        return opt;
    }
    ValueType parseNumber(std::string const& str) {
        return storm::utility::convertNumber<ValueType>(str);
    }
    struct Input {
        std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> model;
        std::shared_ptr<storm::logic::Formula const> formula;
    };
    Input buildPrism(std::string const& programFile, std::string const& formulaAsString, std::string const& constantsAsString = "") const {
        // Parse and build input
        storm::prism::Program program = storm::api::parseProgram(programFile);
        program = program.preprocess(constantsAsString);
        Input input;
        input.formula = storm::api::parsePropertiesForPrismProgram(formulaAsString, program).front().getRawFormula();
        input.model = storm::api::buildSparseModel<ValueType>(program, {input.formula})->template as<storm::models::sparse::Pomdp<ValueType>>();

        // Preprocess
        storm::transformer::MakePOMDPCanonic<ValueType> makeCanonic(*input.model);
        input.model = makeCanonic.transform();
        EXPECT_TRUE(input.model->isCanonic());
        if (TestType::preprocessingType == PreprocessingType::SelfloopReduction || TestType::preprocessingType == PreprocessingType::All) {
            storm::transformer::GlobalPOMDPSelfLoopEliminator<ValueType> selfLoopEliminator(*input.model);
            if (selfLoopEliminator.preservesFormula(*input.formula)) {
                input.model = selfLoopEliminator.transform();
            } else {
                EXPECT_TRUE(input.formula->isOperatorFormula());
                EXPECT_TRUE(input.formula->asOperatorFormula().hasOptimalityType());
                bool maximizing = storm::solver::maximize(input.formula->asOperatorFormula().getOptimalityType());
                // Valid reasons for unpreserved formulas:
                EXPECT_TRUE(maximizing || input.formula->isProbabilityOperatorFormula());
                EXPECT_TRUE(!maximizing || input.formula->isRewardOperatorFormula());
            }
        }
        if (TestType::preprocessingType == PreprocessingType::QualitativeReduction || TestType::preprocessingType == PreprocessingType::All) {
            EXPECT_TRUE(input.formula->isOperatorFormula());
            EXPECT_TRUE(input.formula->asOperatorFormula().hasOptimalityType());
            if (input.formula->isProbabilityOperatorFormula() && storm::solver::maximize(input.formula->asOperatorFormula().getOptimalityType())) {
                storm::analysis::QualitativeAnalysisOnGraphs<ValueType> qualitativeAnalysis(*input.model);
                storm::storage::BitVector prob0States = qualitativeAnalysis.analyseProb0(input.formula->asProbabilityOperatorFormula());
                storm::storage::BitVector prob1States = qualitativeAnalysis.analyseProb1(input.formula->asProbabilityOperatorFormula());
                storm::pomdp::transformer::KnownProbabilityTransformer<ValueType> kpt;
                input.model = kpt.transform(*input.model, prob0States, prob1States);
            }
        }
        EXPECT_TRUE(input.model->isCanonic());
        return input;
    }
    ValueType precision() const {
        return TestType::precision();
    }
    ValueType modelcheckingPrecision() const {
        if (TestType::isExactModelChecking) {
            return storm::utility::zero<ValueType>();
        } else {
            return storm::utility::convertNumber<ValueType>(1e-6);
        }
    }
    bool isExact() const {
        return TestType::isExactModelChecking;
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DefaultDoubleVIEnvironment, SelfloopReductionDefaultDoubleVIEnvironment, QualitativeReductionDefaultDoubleVIEnvironment,
                         PreprocessedDefaultDoubleVIEnvironment, FineDoubleVIEnvironment, RefineDoubleVIEnvironment, PreprocessedRefineDoubleVIEnvironment,
                         DefaultDoubleOVIEnvironment, DefaultRationalPIEnvironment, PreprocessedDefaultRationalPIEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(BeliefExplorationPomdpModelCheckerTest, TestingTypes, );

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    if (this->isExact()) {
        // This model's value can only be approximated arbitrarily close but never reached
        // Exact arithmetics will thus not reach the value with absoulute precision either.
        ValueType approxPrecision = storm::utility::convertNumber<ValueType>(1e-5);
        EXPECT_LE(result.lowerBound, expected + approxPrecision);
        EXPECT_GE(result.upperBound, expected - approxPrecision);
    } else {
        EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
        EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    }
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    if (this->isExact()) {
        // This model's value can only be approximated arbitrarily close but never reached
        // Exact arithmetics will thus not reach the value with absoulute precision either.
        ValueType approxPrecision = storm::utility::convertNumber<ValueType>(1e-5);
        EXPECT_LE(result.lowerBound, expected + approxPrecision);
        EXPECT_GE(result.upperBound, expected - approxPrecision);
    } else {
        EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
        EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    }
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("74/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("74/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("80/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("80/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("38/155");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmax_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("38/155");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->options());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("0");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmin_SE) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model,
                                                                                                                    this->optionsWithStateElimination());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("0");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Pmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Pmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("3/10");
    if (this->isExact()) {
        // This model's value can only be approximated arbitrarily close but never reached
        // Exact arithmetics will thus not reach the value with absoulute precision either.
        ValueType approxPrecision = storm::utility::convertNumber<ValueType>(1e-4);
        EXPECT_LE(result.lowerBound, expected + approxPrecision);
        EXPECT_GE(result.upperBound, expected - approxPrecision);
    } else {
        EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision() * 10);
        EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision() * 10);
    }
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_Rmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("29/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, simple_slippery_Rmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("19/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("74/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_Rmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("80/91");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, maze2_slippery_Rmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    EXPECT_TRUE(storm::utility::isInfinity(result.lowerBound));
    EXPECT_TRUE(storm::utility::isInfinity(result.upperBound));
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmax_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("38/155");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefExplorationPomdpModelCheckerTest, refuel_Pmin_Clip) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>> checker(data.model, this->optionsWithClipping());
    auto result = checker.check(this->env(), *data.formula);

    ValueType expected = this->parseNumber("0");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());
    // Use relative difference of bounds for this one
    EXPECT_LE(result.diff(), this->precision())
        << "Result [" << result.lowerBound << ", " << result.upperBound
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

}  // namespace
