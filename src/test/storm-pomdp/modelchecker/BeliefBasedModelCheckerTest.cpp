#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm-pomdp/beliefs/abstraction/RewardBoundedBeliefSplitter.h"
#include "storm-pomdp/beliefs/exploration/BeliefExploration.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm-pomdp/beliefs/verification/BeliefBasedModelChecker.h"
#include "storm-pomdp/modelchecker/PreprocessingPomdpValueBoundsModelChecker.h"
#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"
#include "storm-pomdp/transformer/KnownProbabilityTransformer.h"
#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
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

TYPED_TEST(BeliefBasedModelCheckerTest, cut_zero_gap) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefValueType = typename TestFixture::BeliefValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;

    auto check = [&](std::string const& formula, std::string const& expectedValue) {
        SCOPED_TRACE(formula);
        auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", formula, "slippery=0");
        ASSERT_EQ(data.model->getInitialStates().getNumberOfSetBits(), 1ul);
        auto const initialState = data.model->getInitialStates().getNextSetIndex(0);
        auto const expected = this->template parseNumber<BeliefMDPValueType>(expectedValue);
        auto const initialValue = this->template parseNumber<POMDPValueType>(expectedValue);
        auto const delta = this->template parseNumber<POMDPValueType>("1/100000000");
        storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
        storm::pomdp::modelchecker::PreprocessingPomdpValueBoundsModelChecker<POMDPType> preprocessChecker(*data.model);
        storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
        bounds.preprocessingBounds = preprocessChecker.getValueBounds(this->env(), *data.formula);
        auto setInitialBounds = [&](POMDPValueType const& lower, POMDPValueType const& upper) {
            for (auto& values : bounds.preprocessingBounds->lower) {
                values[initialState] = lower;
            }
            for (auto& values : bounds.preprocessingBounds->upper) {
                values[initialState] = upper;
            }
        };

        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
        EXPECT_FALSE(options.cutZeroGap);
        options.buildChoiceLabeling = false;
        options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
        auto propertyInfo = *data.propertyInfo;
        for (int const mode : {0, 1, 2}) {
            SCOPED_TRACE(mode);  // Unfolding, static discretization, dynamic discretization.
            auto run = [&]() {
                return mode == 0 ? checker.checkUnfold(this->env(), propertyInfo, options, bounds)
                                 : checker.checkDiscretize(this->env(), propertyInfo, options, this->overApproxResolution(), mode == 2, bounds);
            };

            setInitialBounds(initialValue, initialValue);
            options.cutZeroGap = true;
            auto const cut = run();
            EXPECT_TRUE(cut.completedExploration);
            EXPECT_EQ(cut.statistics.discoveredBeliefs, 1ul);
            EXPECT_EQ(cut.statistics.exploredBeliefs, 0ul);
            EXPECT_LE(storm::utility::abs(cut.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>());

            options.cutZeroGap = false;
            auto const uncut = run();
            EXPECT_TRUE(uncut.completedExploration);
            EXPECT_GT(uncut.statistics.discoveredBeliefs, 1ul);
            EXPECT_GT(uncut.statistics.exploredBeliefs, 0ul);

            // A positive gap below the floating-point solver precision must still be explored.
            setInitialBounds(initialValue - delta, initialValue + delta);
            options.cutZeroGap = true;
            auto const positiveGap = run();
            EXPECT_TRUE(positiveGap.completedExploration);
            EXPECT_GT(positiveGap.statistics.discoveredBeliefs, 1ul);
            EXPECT_GT(positiveGap.statistics.exploredBeliefs, 0ul);
        }
    };

    check("Pmax=? [F \"goal\" ]", "7/10");
    check("Pmin=? [F \"goal\" ]", "3/10");
    check("Rmax=? [F s>4 ]", "29/50");
    check("Rmin=? [F s>4 ]", "19/50");
}

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

TYPED_TEST(BeliefBasedModelCheckerTest, discretization_rejects_exploration_limits) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefValueType = typename TestFixture::BeliefValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Pmin=? [F \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
    bounds.preprocessingBounds.emplace();
    bounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
    bounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.maxExplorationSize = 1;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;

    STORM_SILENT_EXPECT_THROW(checker.checkDiscretize(this->env(), *data.propertyInfo, options, 10, false, bounds), storm::exceptions::NotSupportedException);

    auto const unfolded = checker.checkUnfold(this->env(), *data.propertyInfo, options, bounds);
    EXPECT_FALSE(unfolded.completedExploration);

    options.maxExplorationSize.reset();
    auto const discretized = checker.checkDiscretize(this->env(), *data.propertyInfo, options, 10, false, bounds);
    EXPECT_TRUE(discretized.completedExploration);
    EXPECT_GT(discretized.statistics.exploredBeliefs, 1);
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
    check("Pmax=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "7/10");
    check("Pmin=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "3/10");
    check("Pmax=? [ true Urew{\"rew\"}<=2 \"goal\" ]", "0");
    check("Pmin=? [ true Urew{\"rew\"}<=2 \"goal\" ]", "0");
    // Target beliefs must remain explorable: their rewarded self-loop can satisfy a lower bound after the first visit.
    check("Pmax=? [ true Urew{\"rew\"}>=4 \"goal\" ]", "7/10");
    check("Pmin=? [ true Urew{\"rew\"}>=4 \"goal\" ]", "3/10");
}

TYPED_TEST(BeliefBasedModelCheckerTest, reused_explorer_clears_reward_model) {
    using POMDPType = storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType>;
    using BeliefType = storm::pomdp::beliefs::Belief<typename TestFixture::BeliefValueType>;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using Explorer = storm::pomdp::beliefs::BeliefExploration<BeliefMDPValueType, POMDPType, BeliefType>;
    using StandardInfo = storm::pomdp::beliefs::StandardExplorationInformation<BeliefMDPValueType, BeliefType>;
    using ClippingInfo = storm::pomdp::beliefs::ClippingExplorationInformation<BeliefMDPValueType, BeliefType>;
    using RewardAwareInfo = storm::pomdp::beliefs::RewardAwareExplorationInformation<BeliefMDPValueType, BeliefType>;
    using NoAbstraction = storm::pomdp::beliefs::NoAbstractionType;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    Explorer explorer(*data.model);
    std::string const rewardModelName;
    auto const selectRewardModel = [&]() {
        auto info = explorer.template initializeExploration<StandardInfo>(data.model->getNrObservations());
        explorer.resumeExploration(info, {}, {}, rewardModelName, storm::OptionalRef<NoAbstraction>{});
        EXPECT_GT(info.matrix.rows(), 0ul);
        EXPECT_EQ(info.actionRewards.size(), info.matrix.rows());
    };

    selectRewardModel();
    auto ordinary = explorer.template initializeExploration<StandardInfo>(data.model->getNrObservations());
    explorer.resumeExploration(ordinary, {}, {}, storm::NullRef, storm::OptionalRef<NoAbstraction>{});
    EXPECT_GT(ordinary.matrix.rows(), 0ul);
    EXPECT_TRUE(ordinary.actionRewards.empty());

    selectRewardModel();
    auto clipping = explorer.template initializeExploration<ClippingInfo>(data.model->getNrObservations());
    explorer.resumeClippingExploration(clipping, {}, {}, storm::NullRef, storm::OptionalRef<NoAbstraction>{});
    EXPECT_GT(clipping.matrix.rows(), 0ul);
    EXPECT_TRUE(clipping.actionRewards.empty());

    selectRewardModel();
    storm::pomdp::beliefs::RewardBoundedBeliefSplitter<BeliefMDPValueType, POMDPType, BeliefType> splitter(*data.model);
    splitter.setRewardModel();
    auto rewardAware = explorer.template initializeExploration<RewardAwareInfo>(data.model->getNrObservations());
    explorer.resumeRewardAwareExploration(rewardAware, {}, {}, splitter, storm::OptionalRef<NoAbstraction>{});
    EXPECT_GT(rewardAware.matrix.rows(), 0ul);
    EXPECT_TRUE(rewardAware.actionRewards.empty());
    ASSERT_FALSE(rewardAware.matrix.transitions.empty());
    for (auto const& transition : rewardAware.matrix.transitions) {
        EXPECT_EQ(transition.data.size(), 1ul);
    }
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_aware_resume_preserves_observation_indices) {
    using POMDPType = storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType>;
    using BeliefType = storm::pomdp::beliefs::Belief<typename TestFixture::BeliefValueType>;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using Explorer = storm::pomdp::beliefs::BeliefExploration<BeliefMDPValueType, POMDPType, BeliefType>;
    using Info = storm::pomdp::beliefs::RewardAwareExplorationInformation<BeliefMDPValueType, BeliefType>;
    using Splitter = storm::pomdp::beliefs::RewardBoundedBeliefSplitter<BeliefMDPValueType, POMDPType, BeliefType>;
    using NoAbstraction = storm::pomdp::beliefs::NoAbstractionType;

    // Different state/action rewards ensure that new reward vectors are encountered after resuming.
    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "Rmax=? [F s>4 ]", "slippery=0");
    Explorer explorer(*data.model);
    Splitter uninterruptedSplitter(*data.model);
    uninterruptedSplitter.setRewardModel();
    auto uninterrupted = explorer.template initializeExploration<Info>(data.model->getNrObservations(), storm::pomdp::beliefs::ExplorationQueueOrder::FIFO);
    explorer.resumeRewardAwareExploration(uninterrupted, {}, {}, uninterruptedSplitter, storm::OptionalRef<NoAbstraction>{});

    Splitter resumedSplitter(*data.model);
    resumedSplitter.setRewardModel();
    auto resumed = explorer.template initializeExploration<Info>(data.model->getNrObservations(), storm::pomdp::beliefs::ExplorationQueueOrder::FIFO);
    for (uint64_t steps = 0; resumed.queue.hasNext() && steps < 100; ++steps) {
        auto const limit = resumed.exploredBeliefs.size() + 1;
        explorer.resumeRewardAwareExploration(
            resumed, {}, [&]() { return resumed.exploredBeliefs.size() >= limit; }, resumedSplitter, storm::OptionalRef<NoAbstraction>{});
    }
    ASSERT_FALSE(resumed.queue.hasNext());
    ASSERT_EQ(resumed.discoveredBeliefs.getNumberOfBeliefIds(), uninterrupted.discoveredBeliefs.getNumberOfBeliefIds());
    for (uint64_t id = 0; id < uninterrupted.discoveredBeliefs.getNumberOfBeliefIds(); ++id) {
        EXPECT_TRUE(resumed.discoveredBeliefs.containsBelief(uninterrupted.discoveredBeliefs.getBeliefFromId(id)));
    }
    ASSERT_EQ(resumed.matrix.transitions.size(), uninterrupted.matrix.transitions.size());
    for (uint64_t i = 0; i < uninterrupted.matrix.transitions.size(); ++i) {
        auto const& expected = uninterrupted.matrix.transitions[i];
        auto const& actual = resumed.matrix.transitions[i];
        EXPECT_EQ(actual.probability, expected.probability);
        EXPECT_EQ(actual.data, expected.data);
        EXPECT_EQ(resumed.discoveredBeliefs.getBeliefFromId(actual.targetBelief), uninterrupted.discoveredBeliefs.getBeliefFromId(expected.targetBelief));
    }
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_requires_explicit_reward_model_name) {
    auto const programFile = STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism";
    auto data = this->buildPrism(programFile, "Pmax=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "slippery=0");
    auto program = storm::api::parseProgram(programFile).preprocess("slippery=0");
    auto unnamedFormula = storm::api::parsePropertiesForPrismProgram("Pmax=? [ true Urew<=3 \"goal\" ]", program).front().getRawFormula();

    STORM_SILENT_EXPECT_THROW(storm::pomdp::analysis::getFormulaInformation(*data.model, *unnamedFormula), storm::exceptions::NotSupportedException);

    auto unnamedProperty = *data.propertyInfo;
    unnamedProperty.rewardBounds.front().rewardModelName.clear();
    storm::pomdp::beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType>, typename TestFixture::BeliefValueType,
                                                   typename TestFixture::BeliefMDPValueType>
        checker(*data.model);
    storm::pomdp::storage::BeliefExplorationBounds<typename TestFixture::POMDPValueType> bounds;
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<typename TestFixture::BeliefMDPValueType> options;
    STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareUnfold(this->env(), unnamedProperty, options, bounds, {"rew"}), storm::exceptions::NotSupportedException);
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_requires_matching_reward_model_selection) {
    using POMDPType = storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType>;
    using BeliefType = storm::pomdp::beliefs::Belief<typename TestFixture::BeliefValueType>;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism", "Pmax=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "slippery=0");
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, typename TestFixture::BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::storage::BeliefExplorationBounds<typename TestFixture::POMDPValueType> bounds;
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;

    STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, bounds, {}),
                              storm::exceptions::IllegalArgumentException);
    STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, bounds, {}),
                              storm::exceptions::IllegalArgumentException);
    STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, bounds, {"other"}),
                              storm::exceptions::IllegalArgumentException);
    STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, bounds, {"other", "rew"}),
                              storm::exceptions::IllegalArgumentException);

    storm::pomdp::beliefs::RewardBoundedBeliefSplitter<BeliefMDPValueType, POMDPType, BeliefType> splitter(*data.model);
    STORM_SILENT_EXPECT_THROW(splitter.setRewardModels({}), storm::exceptions::IllegalArgumentException);
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_allows_additional_observed_reward_model) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;
    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism", "Pmax=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "slippery=0");
    auto extraRewardModel = data.model->getRewardModel("rew");
    data.model->addRewardModel("extra", extraRewardModel);

    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
    bounds.preprocessingBounds.emplace();
    bounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
    bounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, typename TestFixture::BeliefValueType, BeliefMDPValueType> checker(*data.model);

    auto const expected = this->template parseNumber<BeliefMDPValueType>("7/10");
    for (bool const discretize : {false, true}) {
        auto const result = discretize ? checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, bounds, {"rew", "extra"})
                                       : checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, bounds, {"rew", "extra"});
        EXPECT_TRUE(result.completedExploration);
        EXPECT_LE(storm::utility::abs(result.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>());
    }
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_simple_early_frontier_uses_cutoff) {
    typedef storm::models::sparse::Pomdp<typename TestFixture::POMDPValueType> POMDPType;
    typedef typename TestFixture::POMDPValueType POMDPValueType;
    typedef typename TestFixture::BeliefValueType BeliefValueType;
    typedef typename TestFixture::BeliefMDPValueType BeliefMDPValueType;

    auto check = [this](std::string const& formula, std::string const& frontierValue, std::string const& completeValue) {
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
        for (bool const discretize : {false, true}) {
            SCOPED_TRACE(discretize ? "discretize" : "unfold");
            if (discretize) {
                STORM_SILENT_EXPECT_THROW(
                    checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, precomputedBeliefBounds, rewardModelNames),
                    storm::exceptions::NotSupportedException);
                options.maxExplorationSize.reset();
            }
            auto const result =
                discretize ? checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 10, false, precomputedBeliefBounds, rewardModelNames)
                           : checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, precomputedBeliefBounds, rewardModelNames);
            EXPECT_EQ(result.completedExploration, discretize);
            if (discretize) {
                EXPECT_GT(result.statistics.exploredBeliefs, 1);
            } else {
                EXPECT_EQ(result.statistics.exploredBeliefs, 1);
                EXPECT_GT(result.statistics.discoveredBeliefs, result.statistics.exploredBeliefs);
            }
            auto const expected = this->template parseNumber<BeliefMDPValueType>(discretize ? completeValue : frontierValue);
            EXPECT_LE(storm::utility::abs(result.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>());
        }
    };

    // With only the initial belief explored, minimization uses the upper cut-off and maximization the lower cut-off.
    check("Pmin=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "1", "3/10");
    check("Pmax=? [ true Urew{\"rew\"}<=3 \"goal\" ]", "0", "7/10");
    // The first transition still costs one reward: even the upper cut-off must fail a zero reward budget.
    check("Pmin=? [ true Urew{\"rew\"}<=0 \"goal\" ]", "0", "0");
    check("Pmax=? [ true Urew{\"rew\"}<=0 \"goal\" ]", "0", "0");
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_hidden_transition_costs) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefValueType = typename TestFixture::BeliefValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;

    auto check = [this](std::string const& formula, std::string const& expectedValue, bool stopAtTargetFrontier, std::string const& completedValue = "") {
        SCOPED_TRACE(formula);
        auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", formula);
        storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, BeliefValueType, BeliefMDPValueType> checker(*data.model);
        storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
        bounds.preprocessingBounds.emplace();
        bounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
        bounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());

        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
        options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
        if (stopAtTargetFrontier) {
            // Explore the draw and finish actions, leaving the target beliefs on the frontier.
            options.maxExplorationSize = 2;
        }

        for (bool const discretize : {false, true}) {
            SCOPED_TRACE(discretize ? "discretize" : "unfold");
            if (stopAtTargetFrontier && discretize) {
                STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 2, false, bounds, {"cost"}),
                                          storm::exceptions::NotSupportedException);
                options.maxExplorationSize.reset();
            }
            auto const result = discretize ? checker.checkRewardAwareDiscretize(this->env(), *data.propertyInfo, options, 2, false, bounds, {"cost"})
                                           : checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, bounds, {"cost"});
            EXPECT_EQ(result.completedExploration, discretize || !stopAtTargetFrontier);
            if (stopAtTargetFrontier && !discretize) {
                EXPECT_EQ(result.statistics.exploredBeliefs, 2);
                EXPECT_GT(result.statistics.discoveredBeliefs, result.statistics.exploredBeliefs);
            }
            auto const expected = this->template parseNumber<BeliefMDPValueType>(discretize && !completedValue.empty() ? completedValue : expectedValue);
            EXPECT_LE(storm::utility::abs(result.value - expected), this->template modelcheckingPrecision<BeliefMDPValueType>())
                << "actual: " << result.value << ", expected: " << expected;
        }
    };

    // The hidden states have the same observation, but finishing costs one or two rewards respectively.
    check("Pmin=? [ true Urew{\"cost\"}<=1 \"goal\" ]", "1/2", false);
    check("Pmax=? [ true Urew{\"cost\"}<=1 \"goal\" ]", "1/2", false);
    check("Pmin=? [ true Urew{\"cost\"}<=2 \"goal\" ]", "1", false);
    check("Pmax=? [ true Urew{\"cost\"}<=2 \"goal\" ]", "1", false);
    // A target reached at cost one may keep accumulating reward before satisfying a lower bound.
    check("Pmin=? [ true Urew{\"cost\"}>=2 \"goal\" ]", "1", false);
    check("Pmax=? [ true Urew{\"cost\"}>=2 \"goal\" ]", "1", false);
    // Target beliefs on the frontier still count as targets; the incoming edge cost decides which half succeeds.
    check("Pmin=? [ true Urew{\"cost\"}<=1 \"goal\" ]", "1/2", true);
    check("Pmax=? [ true Urew{\"cost\"}<=1 \"goal\" ]", "1/2", true);
    // The optimistic Pmin sink collects missing reward; Pmax only counts reward already accumulated at the frontier.
    check("Pmin=? [ true Urew{\"cost\"}>=2 \"goal\" ]", "1", true);
    check("Pmax=? [ true Urew{\"cost\"}>=2 \"goal\" ]", "1/2", true, "1");
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_min_lower_bound_frontier_collects_reward) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", "Pmin=? [ true Urew{\"cost\"}>=2 \"goal\" ]");
    auto extraRewardModel = data.model->getRewardModel("cost");
    data.model->addRewardModel("extra", extraRewardModel);
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, typename TestFixture::BeliefValueType, BeliefMDPValueType> checker(*data.model);
    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
    bounds.preprocessingBounds.emplace();
    bounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
    bounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());

    std::vector<storm::pomdp::beliefs::PropertyInformation> properties{*data.propertyInfo};
    auto strictProperty = *data.propertyInfo;
    strictProperty.rewardBounds.front().lowerBound = storm::logic::TimeBound(true, data.propertyInfo->rewardBounds.front().lowerBound->getBound());
    properties.push_back(strictProperty);
    auto zeroBoundData = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", "Pmin=? [ true Urew{\"cost\"}>=0 \"goal\" ]");
    properties.push_back(*zeroBoundData.propertyInfo);
    auto multidimensionalProperty = *data.propertyInfo;
    auto extraLowerBound = strictProperty.rewardBounds.front();
    extraLowerBound.rewardModelName = "extra";
    multidimensionalProperty.rewardBounds.push_back(extraLowerBound);
    properties.push_back(multidimensionalProperty);

    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    options.explorationQueueOrder = storm::pomdp::beliefs::ExplorationQueueOrder::FIFO;
    for (uint64_t const sizeLimit : {1, 2}) {
        SCOPED_TRACE(sizeLimit);
        // Size one leaves non-target beliefs; size two leaves target beliefs whose cost-one branch still needs reward.
        options.maxExplorationSize = sizeLimit;
        for (std::size_t i = 0; i < properties.size(); ++i) {
            SCOPED_TRACE(i);
            std::vector<std::string> rewardModelNames;
            for (auto const& rewardBound : properties[i].rewardBounds) {
                rewardModelNames.push_back(rewardBound.rewardModelName);
            }
            auto const result = checker.checkRewardAwareUnfold(this->env(), properties[i], options, bounds, rewardModelNames);
            EXPECT_FALSE(result.completedExploration);
            EXPECT_EQ(result.statistics.exploredBeliefs, sizeLimit);
            EXPECT_LE(storm::utility::abs(result.value - storm::utility::one<BeliefMDPValueType>()),
                      this->template modelcheckingPrecision<BeliefMDPValueType>());
        }
    }

    // The first dimension has only an upper bound and must earn no reward in the synthetic sink.
    auto upperBoundData = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", "Pmin=? [ true Urew{\"cost\"}<=1 \"goal\" ]");
    auto upperOnlyBound = upperBoundData.propertyInfo->rewardBounds.front();
    upperOnlyBound.rewardModelName = "extra";
    auto mixedProperty = strictProperty;
    mixedProperty.rewardBounds.insert(mixedProperty.rewardBounds.begin(), upperOnlyBound);
    options.maxExplorationSize = 2;
    auto const mixedResult = checker.checkRewardAwareUnfold(this->env(), mixedProperty, options, bounds, {"extra", "cost"});
    EXPECT_FALSE(mixedResult.completedExploration);
    auto const mixedExpected = this->template parseNumber<BeliefMDPValueType>("1/2");
    EXPECT_LE(storm::utility::abs(mixedResult.value - mixedExpected), this->template modelcheckingPrecision<BeliefMDPValueType>());

    // Limits that leave no frontier must still allow the exact value, rather than rejecting the options themselves.
    for (auto const criterion :
         {storm::pomdp::beliefs::MAX_EXPLORATION_SIZE, storm::pomdp::beliefs::MAX_EXPLORATION_TIME, storm::pomdp::beliefs::MAX_EXPLORATION_SIZE_AND_TIME}) {
        SCOPED_TRACE(criterion);
        options.maxExplorationSize.reset();
        options.maxExplorationTime.reset();
        if (criterion != storm::pomdp::beliefs::MAX_EXPLORATION_TIME) {
            options.maxExplorationSize = 1000;
        }
        if (criterion != storm::pomdp::beliefs::MAX_EXPLORATION_SIZE) {
            options.maxExplorationTime = 3600;
        }
        auto const result = checker.checkRewardAwareUnfold(this->env(), *data.propertyInfo, options, bounds, {"cost"});
        EXPECT_TRUE(result.completedExploration);
        EXPECT_LE(storm::utility::abs(result.value - storm::utility::one<BeliefMDPValueType>()), this->template modelcheckingPrecision<BeliefMDPValueType>());
    }
}

TYPED_TEST(BeliefBasedModelCheckerTest, reward_bounded_min_rejects_two_sided_dimensions) {
    using POMDPValueType = typename TestFixture::POMDPValueType;
    using BeliefMDPValueType = typename TestFixture::BeliefMDPValueType;
    using POMDPType = storm::models::sparse::Pomdp<POMDPValueType>;

    auto data = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", "Pmin=? [ true Urew{\"cost\"}>=2 \"goal\" ]");
    auto extraRewardModel = data.model->getRewardModel("cost");
    data.model->addRewardModel("extra", extraRewardModel);
    storm::pomdp::beliefs::BeliefBasedModelChecker<POMDPType, typename TestFixture::BeliefValueType, BeliefMDPValueType> checker(*data.model);

    auto intervalProperty = *data.propertyInfo;
    intervalProperty.rewardBounds.front().upperBound = intervalProperty.rewardBounds.front().lowerBound;
    auto zeroBoundData = this->buildPrism(STORM_TEST_RESOURCES_DIR "/pomdp/hidden_transition_rewards.prism", "Pmin=? [ true Urew{\"cost\"}>=0 \"goal\" ]");
    auto zeroIntervalProperty = intervalProperty;
    zeroIntervalProperty.rewardBounds.front().lowerBound = zeroBoundData.propertyInfo->rewardBounds.front().lowerBound;
    auto multidimensionalProperty = *data.propertyInfo;
    auto extraIntervalBound = intervalProperty.rewardBounds.front();
    extraIntervalBound.rewardModelName = "extra";
    multidimensionalProperty.rewardBounds.push_back(extraIntervalBound);

    // No preprocessing bounds: validation must reject the property before exploration accesses them.
    storm::pomdp::storage::BeliefExplorationBounds<POMDPValueType> bounds;
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMDPValueType> options;
    for (auto const& property : {intervalProperty, zeroIntervalProperty, multidimensionalProperty}) {
        std::vector<std::string> rewardModelNames;
        for (auto const& rewardBound : property.rewardBounds) {
            rewardModelNames.push_back(rewardBound.rewardModelName);
        }
        for (bool const limited : {false, true}) {
            SCOPED_TRACE(limited);
            options.maxExplorationSize = limited ? std::make_optional<uint64_t>(2) : std::nullopt;
            STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareUnfold(this->env(), property, options, bounds, rewardModelNames),
                                      storm::exceptions::NotSupportedException);
            STORM_SILENT_EXPECT_THROW(checker.checkRewardAwareDiscretize(this->env(), property, options, 2, false, bounds, rewardModelNames),
                                      storm::exceptions::NotSupportedException);
        }
    }

    // Pmax still reaches the underlying engine, whose existing interval restriction is unchanged.
    intervalProperty.dir = storm::OptimizationDirection::Maximize;
    options.maxExplorationSize.reset();
    bounds.preprocessingBounds.emplace();
    bounds.preprocessingBounds->lower.emplace_back(data.model->getNumberOfStates(), storm::utility::zero<POMDPValueType>());
    bounds.preprocessingBounds->upper.emplace_back(data.model->getNumberOfStates(), storm::utility::one<POMDPValueType>());
    for (bool const discretize : {false, true}) {
        SCOPED_TRACE(discretize);
        auto check = [&]() {
            try {
                if (discretize) {
                    checker.checkRewardAwareDiscretize(this->env(), intervalProperty, options, 2, false, bounds, {"cost"});
                } else {
                    checker.checkRewardAwareUnfold(this->env(), intervalProperty, options, bounds, {"cost"});
                }
            } catch (storm::exceptions::NotSupportedException const& exception) {
                EXPECT_NE(std::string(exception.what()).find("Bounded until formulas are only supported by this method"), std::string::npos);
                throw;
            }
        };
        STORM_SILENT_EXPECT_THROW(check(), storm::exceptions::NotSupportedException);
    }
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
