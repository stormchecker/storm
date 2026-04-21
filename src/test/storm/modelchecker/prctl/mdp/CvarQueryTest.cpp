#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"

#include <set>

namespace {
constexpr uint64_t safeChoice = 0;
constexpr uint64_t balancedChoice = 1;
constexpr uint64_t adaptiveChoice = 2;
constexpr uint64_t riskyChoice = 3;
constexpr uint64_t cashChoice = 0;
constexpr uint64_t pushChoice = 1;

bool hasLpSolver() {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    return false;
#else
    return true;
#endif
}

template<typename ValueType>
struct CvarTestInput {
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp;
    std::shared_ptr<storm::logic::Formula const> formula;
};

template<typename ValueType>
CvarTestInput<ValueType> buildCvarInput(std::string const& modelPath, std::string const& propertyString, double alpha) {
    storm::prism::Program program = storm::api::parseProgram(modelPath);
    auto properties = storm::api::parsePropertiesForPrismProgram(propertyString, program);
    std::vector<storm::jani::Property> cvarProperties = {storm::api::createCvarProperty(properties.front(), alpha)};
    auto formulas = storm::api::extractFormulasFromProperties(cvarProperties);
    auto mdp = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    return {mdp, cvarProperties.front().getRawFormula()};
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkInitialStateResult(CvarTestInput<ValueType> const& input, bool produceScheduler = false) {
    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*input.formula, true);
    task.setProduceSchedulers(produceScheduler);
    return checker.check(env, task);
}

template<typename ValueType>
ValueType checkInitialStateValue(CvarTestInput<ValueType> const& input) {
    auto result = checkInitialStateResult(input);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

template<typename ValueType>
std::vector<uint64_t> getChoiceSuccessors(std::shared_ptr<storm::models::sparse::Mdp<ValueType>> const& mdp, uint64_t state, uint64_t localChoice) {
    std::vector<uint64_t> result;
    uint64_t row = mdp->getTransitionMatrix().getRowGroupIndices()[state] + localChoice;
    for (auto const& entry : mdp->getTransitionMatrix().getRow(row)) {
        result.push_back(entry.getColumn());
    }
    return result;
}

TEST(CvarQueryTest, SimpleMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    double alpha = 0.75;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxInput);
    EXPECT_NEAR(maxValue, 2.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minInput);
    EXPECT_NEAR(minValue, 5.0 / 3.0, 1e-10);
}

TEST(CvarQueryTest, ReachableBadMecIsPreprocessedToZeroTerminalReward) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    double alpha = 0.5;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_bad_mec_mdp.nm";

    auto maxInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxInput);
    EXPECT_NEAR(maxValue, 0.0, 1e-10);

    auto minInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minInput);
    EXPECT_NEAR(minValue, 0.0, 1e-10);
}

TEST(CvarQueryTest, RejectsNonAbsorbingOriginalTargetStates) {
    double alpha = 0.5;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_nonabsorbing_target_mdp.nm";

    auto input = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);

    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*input.mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, double> task(*input.formula, true);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::InvalidPropertyException);
}

TEST(CvarQueryTest, BranchingTradeoffMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";

    auto maxHalfInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.5);
    EXPECT_NEAR(checkInitialStateValue(maxHalfInput), 7.0, 1e-10);

    auto maxThreeQuarterInput = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.75);
    EXPECT_NEAR(checkInitialStateValue(maxThreeQuarterInput), 8.0, 1e-10);

    auto minHalfInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.5);
    EXPECT_NEAR(checkInitialStateValue(minHalfInput), 0.0, 1e-10);

    // this requires randomization of the strategy
    auto minThreeQuarterInput = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.75);
    EXPECT_NEAR(checkInitialStateValue(minThreeQuarterInput), 14.0 / 3.0, 1e-10);
}

TEST(CvarQueryTest, ProducesDeterministicSchedulerForMaxBranchingTradeoffMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.75);
    auto result = checkInitialStateResult(input, true);

    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    auto const& quantitativeResult = result->template asExplicitQuantitativeCheckResult<double>();
    ASSERT_TRUE(quantitativeResult.hasScheduler());
    EXPECT_NEAR(quantitativeResult.getMax(), 8.0, 1e-10);

    storm::storage::Scheduler<double> const& scheduler = quantitativeResult.getScheduler();
    uint64_t initialState = *input.mdp->getInitialStates().begin();
    auto adaptiveSuccessors = getChoiceSuccessors(input.mdp, initialState, adaptiveChoice);
    ASSERT_EQ(2, adaptiveSuccessors.size());
    EXPECT_TRUE(scheduler.isDeterministicScheduler());
    EXPECT_TRUE(scheduler.isMemorylessScheduler());
    EXPECT_FALSE(scheduler.isPartialScheduler());
    EXPECT_EQ(adaptiveChoice, scheduler.getChoice(initialState).getDeterministicChoice());
    std::set<uint64_t> branchChoices = {scheduler.getChoice(adaptiveSuccessors[0]).getDeterministicChoice(),
                                        scheduler.getChoice(adaptiveSuccessors[1]).getDeterministicChoice()};
    EXPECT_EQ(std::set<uint64_t>({cashChoice, pushChoice}), branchChoices);
}

TEST(CvarQueryTest, ProducesRandomizedSchedulerForMinBranchingTradeoffMdp) {
    if (!hasLpSolver()) {
        GTEST_SKIP() << "No LP solver available.";
    }

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";
    auto input = buildCvarInput<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.75);
    auto result = checkInitialStateResult(input, true);

    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    auto const& quantitativeResult = result->template asExplicitQuantitativeCheckResult<double>();
    ASSERT_TRUE(quantitativeResult.hasScheduler());
    EXPECT_NEAR(quantitativeResult.getMax(), 14.0 / 3.0, 1e-10);

    storm::storage::Scheduler<double> const& scheduler = quantitativeResult.getScheduler();
    EXPECT_FALSE(scheduler.isDeterministicScheduler());
    EXPECT_TRUE(scheduler.isMemorylessScheduler());
    EXPECT_FALSE(scheduler.isPartialScheduler());

    uint64_t initialState = *input.mdp->getInitialStates().begin();
    auto adaptiveSuccessors = getChoiceSuccessors(input.mdp, initialState, adaptiveChoice);
    ASSERT_EQ(2, adaptiveSuccessors.size());
    auto const& initialChoice = scheduler.getChoice(initialState);
    ASSERT_TRUE(initialChoice.isDefined());
    ASSERT_FALSE(initialChoice.isDeterministic());
    auto const& initialDistribution = initialChoice.getChoiceAsDistribution();
    EXPECT_NEAR(initialDistribution.getProbability(safeChoice), 0.5, 1e-10);
    EXPECT_NEAR(initialDistribution.getProbability(riskyChoice), 0.5, 1e-10);
    EXPECT_NEAR(initialDistribution.getProbability(balancedChoice), 0.0, 1e-10);
    EXPECT_NEAR(initialDistribution.getProbability(adaptiveChoice), 0.0, 1e-10);
    EXPECT_TRUE(scheduler.isDontCare(adaptiveSuccessors[0]));
    EXPECT_TRUE(scheduler.isDontCare(adaptiveSuccessors[1]));
}
}  // namespace
