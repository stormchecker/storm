#include "storm-config.h"
#include "test/storm_gtest.h"

#include <optional>
#include <unordered_map>

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/CvarFormula.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/cvar/CvarInterpretation.h"
#include "storm/modelchecker/cvar/CvarMethod.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/ZeroWeightTransformation.h"
#include "storm/modelchecker/cvar/helper/SparseCvarComputationHelper.h"
#include "storm/modelchecker/cvar/helper/SspParetoFront.h"
#include "storm/modelchecker/cvar/helper/SspParetoValueIterationOperator.h"
#include "storm/modelchecker/cvar/helper/SspZeroWeightComponentLpSolver.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessor.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/utility/constants.h"

namespace {
bool hasLpSolver() {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    return false;
#else
    return true;
#endif
}

bool hasExactLpSolver() {
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    return false;
#else
    return true;
#endif
}

class CvarQueryTest : public ::testing::Test {
   public:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

template<typename ValueType>
struct CvarTestInput {
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp;
    std::shared_ptr<storm::logic::Formula const> formula;
};

template<typename ValueType>
CvarTestInput<ValueType> buildCvarInput(std::string const& modelPath, std::string const& propertyString, std::string const& alpha) {
    storm::prism::Program program = storm::api::parseProgram(modelPath);
    auto properties = storm::api::parsePropertiesForPrismProgram(propertyString, program);
    std::vector<storm::jani::Property> cvarProperties = {storm::api::createCvarProperty(properties.front(), alpha)};
    auto formulas = storm::api::extractFormulasFromProperties(cvarProperties);
    auto mdp = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    return {mdp, cvarProperties.front().getRawFormula()};
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkInitialStateResult(CvarTestInput<ValueType> const& input) {
    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*input.formula, true);
    return checker.check(env, task);
}

template<typename ValueType>
ValueType checkInitialStateValue(CvarTestInput<ValueType> const& input) {
    auto result = checkInitialStateResult(input);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

template<typename ValueType>
ValueType checkInitialStateValueWithMethod(CvarTestInput<ValueType> const& input, storm::modelchecker::cvar::CvarMethod method) {
    storm::Environment env;
    env.modelchecker().cvar().setMethod(method);
    env.modelchecker().cvar().setInterpretationSelection(storm::modelchecker::cvar::CvarInterpretationSelection::Auto);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*input.formula, true);
    auto result = checker.check(env, task);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

template<typename ValueType>
ValueType checkInitialStateValueWithMethodAndInterpretationSelection(CvarTestInput<ValueType> const& input, storm::modelchecker::cvar::CvarMethod method,
                                                                     storm::modelchecker::cvar::CvarInterpretationSelection interpretationSelection) {
    storm::Environment env;
    env.modelchecker().cvar().setMethod(method);
    env.modelchecker().cvar().setInterpretationSelection(interpretationSelection);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*input.formula, true);
    auto result = checker.check(env, task);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

template<typename ParetoFront>
void expectParetoFrontPoints(ParetoFront const& front, std::vector<std::pair<double, double>> const& expectedPoints) {
    auto const& actualPoints = front.getPoints();
    ASSERT_EQ(expectedPoints.size(), actualPoints.size());
    for (std::size_t index = 0; index < expectedPoints.size(); ++index) {
        EXPECT_NEAR(expectedPoints[index].first, actualPoints[index].probability, 1e-10);
        EXPECT_NEAR(expectedPoints[index].second, actualPoints[index].expectedCost, 1e-10);
    }
}

storm::modelchecker::cvar::preprocessing::SspCvarPreprocessingResult<double> buildTinySspPreprocessingResult() {
    storm::storage::SparseMatrixBuilder<double> builder(4, 3, 4, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 1.0);
    builder.addNextValue(1, 1, 1.0);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, 1.0);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, 1.0);

    storm::modelchecker::cvar::preprocessing::SspCvarPreprocessingResult<double> result;
    result.rewardModelName = "cost";
    result.initialState = 0;
    result.targetStates = storm::storage::BitVector(3, false);
    result.targetStates.set(2, true);
    result.reachableStates = storm::storage::BitVector(3, true);
    result.liftedStateRewardsToChoiceCosts = true;
    result.normalizedTargetStatesToAbsorbing = true;
    result.maximalChoiceCost = 2;
    result.choiceCosts = {1.0, 2.0, 1.0, 0.0};
    result.expectedCostsToGoal = {0.0, 0.0, 0.0};
    result.transitionMatrix = builder.build();
    return result;
}

template<typename ValueType>
storm::models::sparse::Mdp<ValueType> buildZeroWeightSspMdp() {
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 5, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, storm::utility::one<ValueType>());
    builder.newRowGroup(1);
    builder.addNextValue(1, 2, storm::utility::one<ValueType>());
    builder.newRowGroup(2);
    builder.addNextValue(2, 3, storm::utility::one<ValueType>());
    builder.addNextValue(3, 3, storm::utility::one<ValueType>());
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, storm::utility::one<ValueType>());

    storm::models::sparse::StateLabeling labeling(4);
    storm::storage::BitVector initialStates(4, false);
    initialStates.set(0);
    labeling.addLabel("init", initialStates);
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels;
    auto const zero = storm::utility::zero<ValueType>();
    rewardModels.emplace(
        "cost", storm::models::sparse::StandardRewardModel<ValueType>(std::nullopt,
                                                                      std::vector<ValueType>{storm::utility::convertNumber<ValueType>(uint64_t{5}), zero, zero,
                                                                                             storm::utility::convertNumber<ValueType>(uint64_t{7}), zero},
                                                                      std::nullopt));
    return storm::models::sparse::Mdp<ValueType>(builder.build(), std::move(labeling), std::move(rewardModels));
}

TEST(CvarSspParetoFrontTest, CanonicalizesDuplicateDominatedAndConvexRedundantPoints) {
    using ParetoFront = storm::modelchecker::cvar::SspParetoFront<double>;

    ParetoFront front({{0.4, 5.0}, {0.2, 4.0}, {0.2, 3.0}, {0.6, 6.0}, {0.8, 9.0}, {0.5, 7.0}});

    expectParetoFrontPoints(front, {{0.2, 3.0}, {0.6, 6.0}, {0.8, 9.0}});
}

TEST(CvarSspParetoFrontTest, ScaledMinkowskiSumMergesConvexChains) {
    using ParetoFront = storm::modelchecker::cvar::SspParetoFront<double>;

    ParetoFront left({{0.0, 0.0}, {0.25, 1.0}, {0.5, 3.0}});
    ParetoFront right({{0.0, 0.0}, {0.25, 0.5}, {0.5, 2.0}});

    auto result = left.minkowskiSumScaled(right, 0.5);

    expectParetoFrontPoints(result, {{0.0, 0.0}, {0.125, 0.25}, {0.375, 1.25}, {0.5, 2.0}, {0.75, 4.0}});
}

TEST(CvarSspParetoFrontTest, CanonicalizesAfterFloatingPointTranslationCollapse) {
    using ParetoFront = storm::modelchecker::cvar::SspParetoFront<double>;

    ParetoFront front({{0.0, 0.0}, {1e-17, 1.0}});

    auto result = front.minkowskiSum(ParetoFront::singleton(1.0, 0.0));

    expectParetoFrontPoints(result, {{1.0, 0.0}});
}

TEST(CvarSspParetoValueIterationOperatorTest, AppliesActionCostsAndUnionsActionFronts) {
    using ParetoFront = storm::modelchecker::cvar::SspParetoFront<double>;
    using ParetoViOperator = storm::modelchecker::cvar::SspParetoValueIterationOperator<double>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    auto preprocessingResult = buildTinySspPreprocessingResult();
    ParetoViOperator paretoViOperator(preprocessingResult);
    FrontierWindow frontierWindow(3, FrontierLayer(3));
    for (auto& layer : frontierWindow) {
        layer[2] = ParetoViOperator::createTargetFrontier();
    }
    frontierWindow[0][1] = ParetoFront::singleton(0.8, 4.0);
    frontierWindow[1][1] = ParetoFront::singleton(0.5, 1.0);

    FrontierLayer outputLayer;
    paretoViOperator.apply(2, frontierWindow, outputLayer);

    expectParetoFrontPoints(outputLayer[0], {{0.5, 1.0}, {0.8, 4.0}});
    expectParetoFrontPoints(outputLayer[1], {{1.0, 0.0}});
    expectParetoFrontPoints(outputLayer[2], {{1.0, 0.0}});
}

TEST(CvarSspRewardParetoFrontTest, KeepsLowerLeftIncomparablePoints) {
    using ParetoFront = storm::modelchecker::cvar::SspRewardParetoFront<double>;

    ParetoFront front({{0.1, 0.9}, {0.5, 0.5}, {0.7, 0.8}, {0.1, 1.1}, {0.9, 0.2}});

    expectParetoFrontPoints(front, {{0.1, 0.9}, {0.5, 0.5}, {0.9, 0.2}});
}

TEST(CvarSspZeroWeightComponentLpSolverTest, SupportsCostAndRewardFronts) {
    if (!hasExactLpSolver()) {
        GTEST_SKIP() << "No exact LP solver available.";
    }

    using ValueType = storm::RationalNumber;
    using CostSolver = storm::modelchecker::cvar::SspZeroWeightComponentLpSolver<ValueType>;
    using RewardSolver = storm::modelchecker::cvar::SspZeroWeightComponentLpSolver<ValueType, storm::modelchecker::cvar::SspParetoFrontKind::RewardLowerTail>;
    auto const zero = storm::utility::zero<ValueType>();
    auto const one = storm::utility::one<ValueType>();
    auto const quarter = ValueType("1/4");
    auto const half = ValueType("1/2");
    auto const threeQuarters = ValueType("3/4");

    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 3, 5, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.addNextValue(1, 1, half);
    builder.addNextValue(1, 2, half);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    auto matrix = builder.build();
    storm::transformer::ZeroWeightComponent component{{0}, {0}, {}, {1, 2}};
    std::vector<ValueType> weights{zero, zero, zero, zero};
    storm::Environment env;

    CostSolver costSolver(env, matrix, weights, component);
    CostSolver::FrontierLayer costFronts(3);
    costFronts[1] = CostSolver::ParetoFront({{quarter, one}, {threeQuarters, one + one}});
    costFronts[2] = CostSolver::ParetoFront::singleton(threeQuarters, one + half);
    auto costResult = costSolver.support(0, costFronts, ValueType(4));
    EXPECT_EQ((std::vector<ValueType>{half, half}), costResult.boundaryProbabilities);
    EXPECT_EQ(threeQuarters, costResult.point.probability);
    EXPECT_EQ(one + threeQuarters, costResult.point.expectedCost);

    RewardSolver rewardSolver(env, matrix, weights, component);
    RewardSolver::FrontierLayer rewardFronts(3);
    rewardFronts[1] = RewardSolver::ParetoFront({{quarter, one + one}, {threeQuarters, one}});
    rewardFronts[2] = RewardSolver::ParetoFront::singleton(quarter, one);
    auto rewardResult = rewardSolver.support(0, rewardFronts, ValueType(8));
    EXPECT_EQ((std::vector<ValueType>{half, half}), rewardResult.boundaryProbabilities);
    EXPECT_EQ(quarter, rewardResult.point.probability);
    EXPECT_EQ(one + half, rewardResult.point.expectedCost);
}

TEST(CvarSspRewardParetoValueIterationOperatorTest, AppliesRewardShiftsAndUnionsActionFronts) {
    using ParetoFront = storm::modelchecker::cvar::SspRewardParetoFront<double>;
    using ParetoViOperator = storm::modelchecker::cvar::SspParetoValueIterationOperator<double, storm::modelchecker::cvar::SspParetoFrontKind::RewardLowerTail>;
    using FrontierLayer = std::vector<ParetoFront>;
    using FrontierWindow = std::vector<FrontierLayer>;

    auto preprocessingResult = buildTinySspPreprocessingResult();
    ParetoViOperator paretoViOperator(preprocessingResult);
    FrontierWindow frontierWindow(3, FrontierLayer(3));
    for (uint64_t threshold = 0; threshold < frontierWindow.size(); ++threshold) {
        frontierWindow[threshold][2] = ParetoViOperator::createTargetFrontier(threshold);
    }
    frontierWindow[2][1] = ParetoFront::singleton(0.5, 0.1);
    frontierWindow[1][1] = ParetoFront::singleton(0.2, 0.5);

    FrontierLayer outputLayer;
    paretoViOperator.apply(3, frontierWindow, outputLayer);

    expectParetoFrontPoints(outputLayer[0], {{0.2, 0.5}, {0.5, 0.1}});
    expectParetoFrontPoints(outputLayer[1], {{1.0, 2.0}});
    expectParetoFrontPoints(outputLayer[2], {{1.0, 3.0}});
}

TEST(CvarSspPreprocessorTest, AppliesSelectedZeroWeightTransformationsToCosts) {
    auto const model = buildZeroWeightSspMdp<double>();
    storm::storage::BitVector targetStates(4, false);
    targetStates.set(3);
    storm::modelchecker::cvar::CvarQueryInformation queryInformation{storm::RationalNumber("1/2"), storm::solver::OptimizationDirection::Minimize,
                                                                     storm::modelchecker::cvar::CvarInterpretation::Cost, std::string("cost"), nullptr};
    storm::Environment env;

    STORM_SILENT_EXPECT_THROW(storm::modelchecker::cvar::preprocessing::preprocessSspCvar(env, model, queryInformation, targetStates),
                              storm::exceptions::InvalidPropertyException);
    env.modelchecker().cvar().setZeroWeightTransformation(storm::modelchecker::cvar::ZeroWeightTransformation::LocalElimination);
    auto const result = storm::modelchecker::cvar::preprocessing::preprocessSspCvar(env, model, queryInformation, targetStates);

    storm::storage::SparseMatrixBuilder<double> expectedMatrixBuilder(4, 3, 4, true, true, 3);
    expectedMatrixBuilder.newRowGroup(0);
    expectedMatrixBuilder.addNextValue(0, 1, 1.0);
    expectedMatrixBuilder.addNextValue(1, 2, 1.0);
    expectedMatrixBuilder.newRowGroup(2);
    expectedMatrixBuilder.addNextValue(2, 1, 1.0);
    expectedMatrixBuilder.newRowGroup(3);
    expectedMatrixBuilder.addNextValue(3, 1, 1.0);
    EXPECT_EQ(expectedMatrixBuilder.build(), result.transitionMatrix);
    EXPECT_EQ((std::vector<double>{5.0, 5.0, 0.0, 7.0}), result.choiceCosts);
    EXPECT_EQ(0ull, result.initialState);
    EXPECT_EQ(7ull, result.maximalChoiceCost);
    EXPECT_EQ(storm::storage::BitVector(3, true), result.reachableStates);
    storm::storage::BitVector expectedTargets(3, false);
    expectedTargets.set(1);
    EXPECT_EQ(expectedTargets, result.targetStates);
    ASSERT_EQ(3ull, result.expectedCostsToGoal.size());
    EXPECT_NEAR(5.0, result.expectedCostsToGoal[0], 1e-10);
    EXPECT_NEAR(0.0, result.expectedCostsToGoal[1], 1e-10);
    EXPECT_NEAR(7.0, result.expectedCostsToGoal[2], 1e-10);

    storm::modelchecker::cvar::SparseCvarComputationHelper<storm::models::sparse::Mdp<double>> helper(model, queryInformation, targetStates);
    EXPECT_NEAR(5.0, helper.computeCvar(env).value, 1e-10);
    if (hasLpSolver()) {
        env.modelchecker().cvar().setZeroWeightTransformation(storm::modelchecker::cvar::ZeroWeightTransformation::GlobalCollapse);
        auto const globalResult = storm::modelchecker::cvar::preprocessing::preprocessSspCvar(env, model, queryInformation, targetStates);
        EXPECT_EQ(result.choiceCosts, globalResult.choiceCosts);
        EXPECT_EQ(result.targetStates, globalResult.targetStates);
        EXPECT_EQ(result.initialState, globalResult.initialState);
        EXPECT_NEAR(5.0, helper.computeCvar(env).value, 1e-10);
    }
}

TEST(CvarSspPreprocessorTest, AppliesSelectedZeroWeightTransformationsToRewards) {
    auto const model = buildZeroWeightSspMdp<storm::RationalNumber>();
    storm::storage::BitVector targetStates(4, false);
    targetStates.set(3);
    storm::modelchecker::cvar::CvarQueryInformation queryInformation{storm::RationalNumber("1/2"), storm::solver::OptimizationDirection::Maximize,
                                                                     storm::modelchecker::cvar::CvarInterpretation::Reward, std::string("cost"), nullptr};
    storm::Environment env;

    STORM_SILENT_EXPECT_THROW(storm::modelchecker::cvar::preprocessing::preprocessSspRewardCvar(env, model, queryInformation, targetStates),
                              storm::exceptions::InvalidPropertyException);
    storm::modelchecker::cvar::SparseCvarComputationHelper<storm::models::sparse::Mdp<storm::RationalNumber>> helper(model, queryInformation, targetStates);
    for (auto transformation :
         {storm::modelchecker::cvar::ZeroWeightTransformation::GlobalCollapse, storm::modelchecker::cvar::ZeroWeightTransformation::LocalElimination}) {
        if (transformation == storm::modelchecker::cvar::ZeroWeightTransformation::GlobalCollapse && !hasExactLpSolver()) {
            continue;
        }
        env.modelchecker().cvar().setZeroWeightTransformation(transformation);
        EXPECT_EQ(storm::RationalNumber(12), helper.computeCvar(env).value);
    }
}

TEST_F(CvarQueryTest, SimpleMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string alpha = "0.75";
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxInput);
    EXPECT_NEAR(maxValue, 2.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minInput);
    EXPECT_NEAR(minValue, 2.0, 1e-10);
}

TEST_F(CvarQueryTest, ReachableBadMecIsPreprocessedToZeroTerminalReward) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string alpha = "0.5";
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_bad_mec_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxInput);
    EXPECT_NEAR(maxValue, 0.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minInput);
    EXPECT_NEAR(minValue, 4.0, 1e-10);
}

TEST_F(CvarQueryTest, TargetReachingMecIsCollapsed) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_target_reaching_mec_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValue(maxInput), 6.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValue(minInput), 4.0, 1e-10);
}

TEST_F(CvarQueryTest, RejectsNonAbsorbingOriginalTargetStates) {
    std::string alpha = "0.5";
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_nonabsorbing_target_mdp.nm";

    auto input = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);

    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, double> task(*input.formula, true);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::InvalidPropertyException);
}

TEST_F(CvarQueryTest, BranchingTradeoffMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";

    auto maxHalfInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.5");
    EXPECT_NEAR(checkInitialStateValue(maxHalfInput), 7.0, 1e-10);

    auto maxThreeQuarterInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValue(maxThreeQuarterInput), 8.0, 1e-10);

    auto minHalfInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.5");
    EXPECT_NEAR(checkInitialStateValue(minHalfInput), 7.0, 1e-10);

    auto minThreeQuarterInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValue(minThreeQuarterInput), 7.0, 1e-10);
}

TEST_F(CvarQueryTest, BranchingTradeoffMdpRationalNumbers) {
    if (!hasExactLpSolver()) {
        GTEST_SKIP() << "No exact LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";

    auto maxInput = buildCvarInput<storm::RationalNumber>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    EXPECT_EQ(storm::RationalNumber(8), checkInitialStateValue(maxInput));

    auto minInput = buildCvarInput<storm::RationalNumber>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.75");
    EXPECT_EQ(storm::RationalNumber(7), checkInitialStateValue(minInput));
}

TEST_F(CvarQueryTest, InterpretationOverridesOnSimpleMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValueWithMethodAndInterpretationSelection(maxInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                           storm::modelchecker::cvar::CvarInterpretationSelection::Reward),
                2.0, 1e-10);
    EXPECT_NEAR(checkInitialStateValueWithMethodAndInterpretationSelection(maxInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                           storm::modelchecker::cvar::CvarInterpretationSelection::Cost),
                7.0 / 3.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.75");
    EXPECT_NEAR(checkInitialStateValueWithMethodAndInterpretationSelection(minInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                           storm::modelchecker::cvar::CvarInterpretationSelection::Cost),
                2.0, 1e-10);
    EXPECT_NEAR(checkInitialStateValueWithMethodAndInterpretationSelection(minInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                           storm::modelchecker::cvar::CvarInterpretationSelection::Reward),
                5.0 / 3.0, 1e-10);
}

TEST_F(CvarQueryTest, InterpretationOverridesOnSimpleMdpRationalNumbers) {
    if (!hasExactLpSolver()) {
        GTEST_SKIP() << "No exact LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto maxInput = buildCvarInput<storm::RationalNumber>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    EXPECT_EQ(storm::RationalNumber(2),
              checkInitialStateValueWithMethodAndInterpretationSelection(maxInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Reward));
    EXPECT_EQ(storm::RationalNumber("7/3"),
              checkInitialStateValueWithMethodAndInterpretationSelection(maxInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Cost));

    auto minInput = buildCvarInput<storm::RationalNumber>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", "0.75");
    EXPECT_EQ(storm::RationalNumber(2),
              checkInitialStateValueWithMethodAndInterpretationSelection(minInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Cost));
    EXPECT_EQ(storm::RationalNumber("5/3"),
              checkInitialStateValueWithMethodAndInterpretationSelection(minInput, storm::modelchecker::cvar::CvarMethod::WeightedReachability,
                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Reward));
}

TEST_F(CvarQueryTest, EquivalentExactAlphaSyntaxes) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto decimalInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "0.75");
    auto fractionInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "3/4");
    auto scientificInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "7.5e-1");

    EXPECT_NEAR(checkInitialStateValue(decimalInput), 2.0, 1e-10);
    EXPECT_NEAR(checkInitialStateValue(fractionInput), 2.0, 1e-10);
    EXPECT_NEAR(checkInitialStateValue(scientificInput), 2.0, 1e-10);
}

TEST(CvarFormulaTest, RejectsInvalidAlphaSyntaxes) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";
    storm::prism::Program program = storm::api::parseProgram(modelPath);
    auto properties = storm::api::parsePropertiesForPrismProgram("R{\"term\"}max=? [ F \"target\" ];", program);

    for (auto const& alpha : {"0", "1", "-0.1", "abc", "0/1", "1/1"}) {
        STORM_SILENT_EXPECT_THROW(storm::api::createCvarProperty(properties.front(), std::string(alpha)), storm::exceptions::BaseException);
    }
}

TEST(CvarFormulaTest, CvarFormulaValidatesAlphaAndSubformula) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";
    storm::prism::Program program = storm::api::parseProgram(modelPath);
    auto properties = storm::api::parsePropertiesForPrismProgram("R{\"term\"}max=? [ F \"target\" ];", program);
    auto formula = properties.front().getRawFormula();

    EXPECT_NO_THROW(storm::logic::CvarFormula(storm::RationalNumber("3/4"), formula));
    STORM_SILENT_EXPECT_THROW(storm::logic::CvarFormula(storm::utility::zero<storm::RationalNumber>(), formula), storm::exceptions::InvalidArgumentException);
    STORM_SILENT_EXPECT_THROW(storm::logic::CvarFormula(storm::utility::one<storm::RationalNumber>(), formula), storm::exceptions::InvalidArgumentException);
    STORM_SILENT_EXPECT_THROW(storm::logic::CvarFormula(storm::RationalNumber("1/2"), nullptr), storm::exceptions::InvalidArgumentException);
}

TEST_F(CvarQueryTest, CheckTaskExtractsWrappedRewardMetadata) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", "3/4");

    auto const& cvarFormula = input.formula->asCvarFormula();
    storm::modelchecker::CheckTask<storm::logic::CvarFormula, double> task(cvarFormula, true);

    ASSERT_TRUE(task.isOptimizationDirectionSet());
    EXPECT_EQ(storm::solver::OptimizationDirection::Maximize, task.getOptimizationDirection());
    ASSERT_TRUE(task.isRewardModelSet());
    EXPECT_EQ("term", task.getRewardModel());
    EXPECT_FALSE(task.isBoundSet());
}

TEST_F(CvarQueryTest, DeterministicSspPathMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_deterministic_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"cost\"}min=? [ F \"goal\" ];", "0.5");

    double value = checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi);
    EXPECT_NEAR(value, 5.0, 1e-10);
}

TEST_F(CvarQueryTest, DeterministicRewardSspPathMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_deterministic_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"cost\"}max=? [ F \"goal\" ];", "0.5");

    EXPECT_NEAR(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 5.0, 1e-10);
    EXPECT_NEAR(checkInitialStateValueWithMethodAndInterpretationSelection(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi,
                                                                           storm::modelchecker::cvar::CvarInterpretationSelection::Reward),
                5.0, 1e-10);
}

TEST_F(CvarQueryTest, BranchingSspTradeoffMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_branching_tradeoff_mdp.nm";

    auto halfInput = buildCvarInput<double>(modelPath, "R{\"cost\"}min=? [ F \"goal\" ];", "0.5");
    EXPECT_NEAR(checkInitialStateValueWithMethod(halfInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 6.0, 1e-10);

    auto nineTenthsInput = buildCvarInput<double>(modelPath, "R{\"cost\"}min=? [ F \"goal\" ];", "0.9");
    EXPECT_NEAR(checkInitialStateValueWithMethod(nineTenthsInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 16.0 / 3.0, 1e-10);
}

TEST_F(CvarQueryTest, SafeRiskyRewardSspMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_reward_safe_risky_mdp.nm";

    auto quarterInput = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.25");
    EXPECT_NEAR(checkInitialStateValueWithMethod(quarterInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 6.0, 1e-10);

    auto fourFifthsInput = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.8");
    EXPECT_NEAR(checkInitialStateValueWithMethod(fourFifthsInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 8.0, 1e-10);
}

TEST_F(CvarQueryTest, DelayedChallengerRewardSspMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_reward_delayed_challenger_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.5");

    EXPECT_NEAR(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 8.0, 1e-10);
}

TEST_F(CvarQueryTest, GeometricRewardSspMdp) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_reward_geometric_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.8");

    EXPECT_NEAR(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi), 1.4375, 1e-10);
}

TEST_F(CvarQueryTest, RejectsRewardSspProb1AChoiceViolation) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_reward_prob1a_choice_violation_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.5");

    STORM_SILENT_EXPECT_THROW(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi),
                              storm::exceptions::InvalidPropertyException);
}

TEST_F(CvarQueryTest, RejectsRewardSspProb1AProbabilisticViolation) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_reward_prob1a_probabilistic_violation_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"reward\"}max=? [ F \"goal\" ];", "0.5");

    STORM_SILENT_EXPECT_THROW(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi),
                              storm::exceptions::InvalidPropertyException);
}

TEST_F(CvarQueryTest, RejectsUnsupportedSspInterpretationCombinations) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_deterministic_mdp.nm";

    auto minRewardInput = buildCvarInput<double>(modelPath, "R{\"cost\"}min=? [ F \"goal\" ];", "0.5");
    STORM_SILENT_EXPECT_THROW(checkInitialStateValueWithMethodAndInterpretationSelection(minRewardInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi,
                                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Reward),
                              storm::exceptions::InvalidPropertyException);

    auto maxCostInput = buildCvarInput<double>(modelPath, "R{\"cost\"}max=? [ F \"goal\" ];", "0.5");
    STORM_SILENT_EXPECT_THROW(checkInitialStateValueWithMethodAndInterpretationSelection(maxCostInput, storm::modelchecker::cvar::CvarMethod::SspParetoVi,
                                                                                         storm::modelchecker::cvar::CvarInterpretationSelection::Cost),
                              storm::exceptions::InvalidPropertyException);
}

TEST_F(CvarQueryTest, RejectsSspTransitionRewards) {
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_ssp_deterministic_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"cost\"}max=? [ F \"goal\" ];", "0.5");

    storm::models::sparse::StandardRewardModel<double> transitionRewardModel(
        std::nullopt, std::make_optional(std::vector<double>(input.mdp->getNumberOfChoices(), 1.0)), std::make_optional(input.mdp->getTransitionMatrix()));
    input.mdp->getRewardModels()["cost"] = std::move(transitionRewardModel);

    STORM_SILENT_EXPECT_THROW(checkInitialStateValueWithMethod(input, storm::modelchecker::cvar::CvarMethod::SspParetoVi),
                              storm::exceptions::NotImplementedException);
}
}  // namespace
