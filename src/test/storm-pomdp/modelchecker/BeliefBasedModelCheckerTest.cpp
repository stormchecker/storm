#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm-pomdp/beliefs/verification/BeliefBasedModelChecker.h"
#include "storm-pomdp/modelchecker/PreprocessingPomdpValueBoundsModelChecker.h"
#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"
#include "storm-pomdp/transformer/KnownProbabilityTransformer.h"
#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/transformer/MakePOMDPCanonic.h"
#include "storm/utility/graph.h"

namespace {
enum class PreprocessingType { None, SelfloopReduction, QualitativeReduction, All };

class DefaultDoubleVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class SelfloopReductionDefaultDoubleVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::SelfloopReduction;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class QualitativeReductionDefaultDoubleVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::QualitativeReduction;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class PreprocessedDefaultDoubleVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::All;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class FineDoubleVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.02);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static uint64_t overApproxResolution() {
        return 24;
    }
};

class DefaultDoubleOVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().setForceSoundness(true);
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class DefaultDoubleSVIEnvironment {
   public:
    typedef double POMDPValueType;
    typedef double BeliefValueType;
    typedef double BeliefMDPValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().setForceSoundness(true);
        return env;
    }
    static bool const isExactModelChecking = false;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class DefaultRationalPIEnvironment {
   public:
    typedef storm::RationalNumber POMDPValueType;
    typedef storm::RationalNumber BeliefValueType;
    typedef storm::RationalNumber BeliefMDPValueType;

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().setForceExact(true);
        return env;
    }
    static bool const isExactModelChecking = true;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::None;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

class PreprocessedDefaultRationalPIEnvironment {
   public:
    typedef storm::RationalNumber POMDPValueType;
    typedef storm::RationalNumber BeliefValueType;
    typedef storm::RationalNumber BeliefMDPValueType;

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().setForceExact(true);
        return env;
    }
    static bool const isExactModelChecking = true;
    static POMDPValueType precision() {
        return storm::utility::convertNumber<POMDPValueType>(0.12);
    }  // there actually aren't any precision guarantees, but we still want to detect if results are weird.
    static PreprocessingType const preprocessingType = PreprocessingType::All;
    static uint64_t overApproxResolution() {
        return 2;
    }
};

template<typename TestType>
class BeliefBasedModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::POMDPValueType POMDPValueType;
    typedef typename TestType::BeliefValueType BeliefValueType;
    typedef typename TestType::BeliefMDPValueType BeliefMDPValueType;
    typedef storm::utility::ExtendedValueType<BeliefMDPValueType> ExtendedBeliefMDPValueType;

    BeliefBasedModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }

    template<typename ValueType>
    ValueType parseNumber(std::string const& str) {
        return storm::utility::convertNumber<ValueType>(str);
    }
    struct Input {
        std::shared_ptr<storm::models::sparse::Pomdp<POMDPValueType>> model;
        std::shared_ptr<storm::logic::Formula const> formula;
        std::shared_ptr<storm::pomdp::beliefs::PropertyInformation> propertyInfo = std::make_shared<storm::pomdp::beliefs::PropertyInformation>();
    };
    Input buildPrism(std::string const& programFile, std::string const& formulaAsString, std::string const& constantsAsString = "") const {
        // Parse and build input
        storm::prism::Program program = storm::api::parseProgram(programFile);
        program = program.preprocess(constantsAsString);
        Input input;
        input.formula = storm::api::parsePropertiesForPrismProgram(formulaAsString, program).front().getRawFormula();
        input.model = storm::api::buildSparseModel<POMDPValueType>(program, {input.formula})->template as<storm::models::sparse::Pomdp<POMDPValueType>>();
        bool const isBoundedProbability =
            input.formula->isProbabilityOperatorFormula() && input.formula->asProbabilityOperatorFormula().getSubformula().isBoundedUntilFormula();

        // Preprocess
        storm::transformer::MakePOMDPCanonic<POMDPValueType> makeCanonic(*input.model);
        input.model = makeCanonic.transform();
        EXPECT_TRUE(input.model->isCanonic());
        if (!isBoundedProbability &&
            (TestType::preprocessingType == PreprocessingType::SelfloopReduction || TestType::preprocessingType == PreprocessingType::All)) {
            storm::transformer::GlobalPOMDPSelfLoopEliminator<POMDPValueType> selfLoopEliminator(*input.model);
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
        if (!isBoundedProbability &&
            (TestType::preprocessingType == PreprocessingType::QualitativeReduction || TestType::preprocessingType == PreprocessingType::All)) {
            EXPECT_TRUE(input.formula->isOperatorFormula());
            EXPECT_TRUE(input.formula->asOperatorFormula().hasOptimalityType());
            if (input.formula->isProbabilityOperatorFormula() && storm::solver::maximize(input.formula->asOperatorFormula().getOptimalityType())) {
                storm::analysis::QualitativeAnalysisOnGraphs<POMDPValueType> qualitativeAnalysis(*input.model);
                storm::storage::BitVector prob0States = qualitativeAnalysis.analyseProb0(input.formula->asProbabilityOperatorFormula());
                storm::storage::BitVector prob1States = qualitativeAnalysis.analyseProb1(input.formula->asProbabilityOperatorFormula());
                storm::pomdp::transformer::KnownProbabilityTransformer<POMDPValueType> kpt;
                input.model = kpt.transform(*input.model, prob0States, prob1States);
            }
        }
        EXPECT_TRUE(input.model->isCanonic());
        auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*input.model, *input.formula);
        std::optional<std::string> rewardModelName;
        std::set<uint32_t> targetObservations;
        EXPECT_TRUE(formulaInfo.isNonNestedReachabilityProbability() || formulaInfo.isNonNestedExpectedRewardFormula());
        if (formulaInfo.getTargetStates().observationClosed) {
            targetObservations = formulaInfo.getTargetStates().observations;
        } else {
            storm::transformer::MakeStateSetObservationClosed<POMDPValueType> obsCloser(input.model);
            std::tie(input.model, targetObservations) = obsCloser.transform(formulaInfo.getTargetStates().states);
        }
        if (formulaInfo.isNonNestedReachabilityProbability()) {
            if (!formulaInfo.getSinkStates().empty()) {
                storm::storage::sparse::ModelComponents<POMDPValueType> components;
                components.stateLabeling = input.model->getStateLabeling();
                components.rewardModels = input.model->getRewardModels();
                auto matrix = input.model->getTransitionMatrix();
                matrix.makeRowGroupsAbsorbing(formulaInfo.getSinkStates().states);
                components.transitionMatrix = matrix;
                components.observabilityClasses = input.model->getObservations();
                if (input.model->hasChoiceLabeling()) {
                    components.choiceLabeling = input.model->getChoiceLabeling();
                }
                if (input.model->hasObservationValuations()) {
                    components.observationValuations = input.model->getObservationValuations();
                }
                input.model = std::make_shared<storm::models::sparse::Pomdp<POMDPValueType>>(std::move(components), true);
                auto reachableFromSinkStates =
                    storm::utility::graph::getReachableStates(input.model->getTransitionMatrix(), formulaInfo.getSinkStates().states,
                                                              formulaInfo.getSinkStates().states, ~formulaInfo.getSinkStates().states);
                reachableFromSinkStates &= ~formulaInfo.getSinkStates().states;
                STORM_LOG_THROW(reachableFromSinkStates.empty(), storm::exceptions::NotSupportedException,
                                "There are sink states that can reach non-sink states. This is currently not supported");
            }
        } else {
            // Expected reward formula!
            rewardModelName = formulaInfo.getRewardModelName();
        }

        if (rewardModelName) {
            input.propertyInfo->kind = storm::pomdp::beliefs::PropertyInformation::Kind::ExpectedTotalReachabilityReward;
            input.propertyInfo->rewardModelName = rewardModelName;
        } else if (formulaInfo.isBounded()) {
            input.propertyInfo->kind = storm::pomdp::beliefs::PropertyInformation::Kind::RewardBoundedReachabilityProbability;
            auto const& boundedFormula = input.formula->asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();
            for (uint64_t i = 0; i < boundedFormula.getDimension(); ++i) {
                auto const& reference = boundedFormula.getTimeBoundReference(i);
                input.propertyInfo->rewardBounds.push_back({.rewardModelName = reference.getOptionalRewardModelName().get_value_or(""),
                                                            .lowerBound = boundedFormula.getLowerBoundAsOptionalTimeBound(i),
                                                            .upperBound = boundedFormula.getUpperBoundAsOptionalTimeBound(i)});
            }
        } else {
            input.propertyInfo->kind = storm::pomdp::beliefs::PropertyInformation::Kind::ReachabilityProbability;
        }
        input.propertyInfo->dir = formulaInfo.getOptimizationDirection();
        input.propertyInfo->targetObservations = targetObservations;

        return input;
    }
    POMDPValueType precision() const {
        return TestType::precision();
    }
    uint64_t overApproxResolution() const {
        return TestType::overApproxResolution();
    }
    template<typename ValueType>
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
                         PreprocessedDefaultDoubleVIEnvironment, FineDoubleVIEnvironment, DefaultDoubleOVIEnvironment, DefaultDoubleSVIEnvironment,
                         DefaultRationalPIEnvironment, PreprocessedDefaultRationalPIEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(BeliefBasedModelCheckerTest, TestingTypes, );

TYPED_TEST(BeliefBasedModelCheckerTest, simple_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    auto expected = this->template parseNumber<BeliefMDPValueType>("7/10");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GT(overCheckResult.statistics.discoveredBeliefs, 0ul);
    EXPECT_GT(overCheckResult.statistics.exploredBeliefs, 0ul);
    EXPECT_GT(overCheckResult.statistics.beliefMdpStates, 0ul);
    EXPECT_GT(overCheckResult.statistics.beliefMdpChoices, 0ul);
    EXPECT_GT(overCheckResult.statistics.beliefMdpTransitions, 0ul);
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());

    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("3/10");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_slippery_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("7/10");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();

    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_slippery_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    POMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("3/10");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    if (this->isExact()) {
        // This model's value can only be approximated arbitrarily close but never reached
        // Exact arithmetics will thus not reach the value with absoulute precision either.
        POMDPValueType approxPrecision = storm::utility::convertNumber<POMDPValueType>(1e-5);
        EXPECT_GE(underResultValue, expected - approxPrecision);
        EXPECT_LE(overResultValue, expected + approxPrecision);
    } else {
        EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
        EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    }
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("29/50");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("19/50");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_slippery_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);
    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("29/30");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, simple_slippery_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("19/30");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, maze2_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("74/91");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, maze2_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), overResultValue);

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), underResultValue);
}

TYPED_TEST(BeliefBasedModelCheckerTest, maze2_slippery_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("80/91");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, maze2_slippery_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), overResultValue);

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), underResultValue);
}

TYPED_TEST(BeliefBasedModelCheckerTest, refuel_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("38/155");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, refuel_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("0");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_simple_min_max) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto check = [this](std::string const& formula, std::string const& expectedValue) {
        SCOPED_TRACE(formula);
        auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism", formula, "slippery=0");
        storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
        storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
        precomputedBeliefBounds.preprocessingBounds.emplace();
        precomputedBeliefBounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
        precomputedBeliefBounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());

        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
        options.buildChoiceLabeling = false;
        options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
        options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();

        std::vector<std::string> rewardModelNames;
        for (auto const& rewardBound : data.propertyInfo->rewardBounds) {
            rewardModelNames.push_back(rewardBound.rewardModelName);
        }
        auto const expected = this->template parseNumber<BeliefMDPValueType>(expectedValue);
        for (bool const discretize : {false, true}) {
            SCOPED_TRACE(discretize ? "discretize" : "unfold");
            // Resolution 10 represents the model's (0.7, 0.3) beliefs exactly.
            auto const result =
                discretize ? checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, precomputedBeliefBounds, rewardModelNames)
                           : checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds, rewardModelNames);
            EXPECT_TRUE(result.completedExploration);
            EXPECT_LE(storm::utility::abs(result.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>());
        }
    };

    // The goal is first reachable after three unit rewards; the scheduler chooses between success probabilities 0.3 and 0.7.
    check("Pmax=? [ true Urew<=3 \"goal\" ]", "7/10");
    check("Pmin=? [ true Urew<=3 \"goal\" ]", "3/10");
    check("Pmax=? [ true Urew<=2 \"goal\" ]", "0");
    check("Pmin=? [ true Urew<=2 \"goal\" ]", "0");
    // Target beliefs must remain explorable: their rewarded self-loop can satisfy a lower bound after the first visit.
    check("Pmax=? [ true Urew>=4 \"goal\" ]", "7/10");
    check("Pmin=? [ true Urew>=4 \"goal\" ]", "3/10");
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_simple_early_frontier_uses_cutoff) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto check = [this](std::string const& formula, std::string const& expectedValue) {
        SCOPED_TRACE(formula);
        auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism", formula, "slippery=0");
        storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
        storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
        precomputedBeliefBounds.preprocessingBounds.emplace();
        precomputedBeliefBounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
        precomputedBeliefBounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());

        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
        options.buildChoiceLabeling = false;
        options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
        options.maxExplorationSize = 1;

        std::vector<std::string> rewardModelNames;
        for (auto const& rewardBound : data.propertyInfo->rewardBounds) {
            rewardModelNames.push_back(rewardBound.rewardModelName);
        }
        auto const expected = this->template parseNumber<BeliefMDPValueType>(expectedValue);
        for (bool const discretize : {false, true}) {
            SCOPED_TRACE(discretize ? "discretize" : "unfold");
            auto const result =
                discretize ? checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, precomputedBeliefBounds, rewardModelNames)
                           : checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds, rewardModelNames);
            EXPECT_FALSE(result.completedExploration);
            EXPECT_EQ(result.statistics.exploredBeliefs, 1);
            EXPECT_GT(result.statistics.discoveredBeliefs, result.statistics.exploredBeliefs);
            EXPECT_LE(storm::utility::abs(result.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>());
        }
    };

    // With only the initial belief explored, minimization uses the upper cut-off and maximization the lower cut-off.
    check("Pmin=? [ true Urew<=3 \"goal\" ]", "1");
    check("Pmax=? [ true Urew<=3 \"goal\" ]", "0");
    // The first transition still costs one reward: even the upper cut-off must fail a zero reward budget.
    check("Pmin=? [ true Urew<=0 \"goal\" ]", "0");
    check("Pmax=? [ true Urew<=0 \"goal\" ]", "0");
}

#if defined STORM_HAVE_LP_SOLVER
TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    auto expected = this->template parseNumber<BeliefMDPValueType>("7/10");

    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("3/10");

    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;
    EXPECT_GE(overResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << overResultValue << ", " << underResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_Pmin_early_clipping_is_sound) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.maxExplorationSize = 1;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    auto underResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    auto expected = this->template parseNumber<BeliefMDPValueType>("3/10");
    EXPECT_GE(underResult.value, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_slippery_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmax=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("7/10");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();

    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_slippery_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType overResultValue;
    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    POMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("3/10");
    auto overCheckResult = checker.checkDiscretize(this->env(), *data.propertyInfo, options, this->overApproxResolution(), true, precomputedBeliefBounds);
    overResultValue = overCheckResult.value;

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    if (this->isExact()) {
        // This model's value can only be approximated arbitrarily close but never reached
        // Exact arithmetics will thus not reach the value with absoulute precision either.
        POMDPValueType approxPrecision = storm::utility::convertNumber<POMDPValueType>(1e-5);
        EXPECT_GE(underResultValue, expected - approxPrecision);
        EXPECT_LE(overResultValue, expected + approxPrecision);
    } else {
        EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
        EXPECT_LE(overResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
    }
    EXPECT_LE(storm::utility::abs(overResultValue - underResultValue), this->precision())
        << "Result [" << underResultValue << ", " << overResultValue
        << "] is not precise enough. If (only) this fails, the result bounds are still correct, but they might be unexpectedly imprecise.\n";
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("29/50");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("19/50");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_slippery_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("29/30");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_simple_slippery_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmin=? [F s>4 ]", "slippery=0.4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("19/30");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_maze2_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("74/91");
    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_maze2_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), underResultValue);
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_maze2_slippery_Rmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmin=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("80/91");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_maze2_slippery_Rmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "Rmax=? [F \"goal\"]", "sl=0.075");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
    precomputedBeliefBounds.extremeBounds = preprocessChecker.getExtremeValueBound(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_EQ(storm::utility::positiveInfinity<BeliefMDPValueType>(), underResultValue);
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_refuel_Pmax) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmax=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("38/155");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_LE(underResultValue, expected + this->template modelcheckingPrecision<BeliefMDPValueType>());
}

TYPED_TEST(BeliefBasedModelCheckerTest, clip_refuel_Pmin) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism", "Pmin=?[\"notbad\" U \"goal\"]", "N=4");
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<POMDPValueType>, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> precomputedBeliefBounds;
    precomputedBeliefBounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.buildChoiceLabeling = false;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    options.useClipping = true;
    options.clippingResolutions = std::vector<uint64_t>(data.model->getNrObservations(), 2);

    typename TestFixture::ExtendedBeliefMDPValueType underResultValue;

    BeliefMDPValueType expected = this->template parseNumber<BeliefMDPValueType>("0");

    options.maxExplorationSize = data.model->getNumberOfStates() * data.model->getMaxNrStatesWithSameObservation();
    auto underCheckResult = checker.checkUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds);
    underResultValue = underCheckResult.value;
    EXPECT_GE(underResultValue, expected - this->template modelcheckingPrecision<BeliefMDPValueType>());
}

#endif  // defined STORM_HAVE_LP_SOLVER

}  // namespace
