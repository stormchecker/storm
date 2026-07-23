#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm-pomdp/api/verification.h"
#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"
#include "storm-pomdp/transformer/KnownProbabilityTransformer.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/transformer/MakePOMDPCanonic.h"

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
};

template<typename TestType>
class BeliefExplorationAPITest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    BeliefExplorationAPITest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
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
        return input;
    }
    ValueType precision() const {
        return TestType::precision();
    }
    ValueType modelcheckingPrecision() const {
        if (TestType::isExactModelChecking)
            return storm::utility::zero<ValueType>();
        else
            return storm::utility::convertNumber<ValueType>(1e-6);
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DefaultDoubleVIEnvironment> TestingTypes;

TYPED_TEST_SUITE(BeliefExplorationAPITest, TestingTypes, );

TYPED_TEST(BeliefExplorationAPITest, simple_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("3/10");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_slippery_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("7/10");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_slippery_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("3/10");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));

    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("29/50");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("19/50");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_slippery_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("29/30");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 5), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple_slippery_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("19/30");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 5), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, maze2_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("74/91");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, maze2_slippery_Rmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("80/91");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, refuel_Pmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("38/155");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(2ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 2), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, refuel_Pmin) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 100);

    ValueType expected = this->parseNumber("0");
    EXPECT_GE(result.upperBound, expected - this->modelcheckingPrecision());

    EXPECT_EQ(1ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1), std::out_of_range);
}

TYPED_TEST(BeliefExplorationAPITest, simple2_Rmax) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple2.prism", "Rmax=?[F \"goal\"]");
    auto task = storm::api::createTask<ValueType>(data.formula, false);
    auto result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 10);

    ValueType expected = this->parseNumber("59040588757/103747000000");
    EXPECT_LE(result.lowerBound, expected + this->modelcheckingPrecision());

    EXPECT_EQ(2ul, storm::pomdp::api::getNumberOfPreprocessingSchedulers<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::extractSchedulerAsMarkovChain<ValueType>(result));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 0));
    EXPECT_NO_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 1));
    EXPECT_THROW(storm::pomdp::api::getCutoffScheduler<ValueType>(result, 2), std::out_of_range);

    std::vector<std::unordered_map<uint64_t, ValueType>> obs0vals{{{0, 0}, {1, 0}}, {{0, 0.7}}, {{0, 1}, {1, 1}}};
    std::vector<std::unordered_map<uint64_t, ValueType>> obs1vals{{{2, 1}}, {{2, 1}}};
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> additionalVals{obs0vals, obs1vals};

    result = storm::pomdp::api::underapproximateWithCutoffs<ValueType>(data.model, task, 10, additionalVals);

    EXPECT_LE(result.lowerBound, storm::utility::one<ValueType>() + this->modelcheckingPrecision());
}

TYPED_TEST(BeliefExplorationAPITest, noHeuristicValues) {
    typedef typename TestFixture::ValueType ValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple2.prism", "Rmax=?[F \"goal\"]");
    auto task = storm::api::createTask<ValueType>(data.formula, false);

    std::vector<std::unordered_map<uint64_t, ValueType>> obs0vals{{{0, 0}, {1, 0}}, {{0, 0.7}}, {{0, 1}, {1, 1}}};
    std::vector<std::unordered_map<uint64_t, ValueType>> obs1vals{{{2, 1}}, {{2, 1}}};
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> additionalVals{obs0vals, obs1vals};

    auto result = storm::pomdp::api::underapproximateWithoutHeuristicValues<ValueType>(data.model, task, 10, additionalVals);

    EXPECT_LE(result.lowerBound, storm::utility::one<ValueType>() + this->modelcheckingPrecision());
}
