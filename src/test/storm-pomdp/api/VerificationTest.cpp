#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-pomdp/api/verification.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace {

using Pomdp = storm::models::sparse::Pomdp<double>;
using Options = storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<double>;

// The initial state branches to hidden states 1 and 2 with probabilities 1/4 and 3/4.
// Action a reaches goal 3 with probabilities 1/5 and 4/5; action b reverses these.
// All other transitions go to absorbing failure state 4. The optimal value is 13/20.
Pomdp makePomdp(bool reverseActions = false, bool hiddenTarget = false) {
    storm::storage::SparseMatrixBuilder<double> matrix(0, 0, 0, false, true);
    std::vector<std::string> actionLabels;
    uint64_t row = 0;
    auto addAction = [&](std::string const& label, std::initializer_list<std::pair<uint64_t, double>> transitions) {
        for (auto const& [state, probability] : transitions) {
            matrix.addNextValue(row, state, probability);
        }
        actionLabels.push_back(label);
        ++row;
    };
    matrix.newRowGroup(row);
    addAction("start", {{1, 0.25}, {2, 0.75}});
    matrix.newRowGroup(row);
    addAction("a", {{3, 0.2}, {4, 0.8}});
    addAction("b", {{3, 0.8}, {4, 0.2}});
    matrix.newRowGroup(row);
    if (reverseActions) {
        addAction("b", {{3, 0.2}, {4, 0.8}});
        addAction("a", {{3, 0.8}, {4, 0.2}});
    } else {
        addAction("a", {{3, 0.8}, {4, 0.2}});
        addAction("b", {{3, 0.2}, {4, 0.8}});
    }
    matrix.newRowGroup(row);
    addAction("a", {{3, 1.0}});
    addAction("b", {{3, 1.0}});
    matrix.newRowGroup(row);
    addAction("stay", {{4, 1.0}});

    storm::models::sparse::StateLabeling labels(5);
    labels.addLabel("init");
    labels.addLabelToState("init", 0);
    labels.addLabel("goal");
    labels.addLabelToState("goal", 3);
    labels.addLabel("safe");
    labels.addLabelToState("safe", 0);
    labels.addLabelToState("safe", 1);
    storm::models::sparse::ChoiceLabeling choices(row);
    for (uint64_t choice = 0; choice < row; ++choice) {
        if (!choices.containsLabel(actionLabels[choice])) {
            choices.addLabel(actionLabels[choice]);
        }
        choices.addLabelToChoice(actionLabels[choice], choice);
    }
    storm::storage::sparse::ModelComponents<double> components(matrix.build(), std::move(labels));
    components.choiceLabeling = std::move(choices);
    components.observabilityClasses = hiddenTarget ? std::vector<uint32_t>{0, 1, 1, 1, 2} : std::vector<uint32_t>{0, 1, 1, 2, 3};
    return Pomdp(std::move(components), !reverseActions);
}

class PomdpVerificationTest : public ::testing::TestWithParam<bool> {
   protected:
    void SetUp() override {
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
    }

    auto verify(Pomdp const& pomdp, storm::logic::Formula const& formula) {
        if (GetParam()) {
            return storm::pomdp::api::verifyWithBeliefDiscretization(env, pomdp, formula, 4);
        }
        return storm::pomdp::api::verifyWithBeliefUnfolding(env, pomdp, formula, Options{});
    }

    storm::Environment env;
    storm::parser::FormulaParser parser;
};

TEST_P(PomdpVerificationTest, CanonicalizesActionOrderWithoutChangingInput) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "POMDP preprocessing requires Z3.";
#endif
    auto pomdp = makePomdp(true);
    ASSERT_FALSE(pomdp.isCanonic());
    auto const matrix = pomdp.getTransitionMatrix();
    auto const choices = pomdp.getChoiceLabeling();
    auto formula = parser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");

    auto const result = verify(pomdp, *formula);
    EXPECT_NEAR(result.value, 0.65, 1e-8);
    EXPECT_TRUE(result.completedExploration);
    EXPECT_FALSE(pomdp.isCanonic());
    EXPECT_EQ(pomdp.getTransitionMatrix(), matrix);
    EXPECT_EQ(pomdp.getChoiceLabeling(), choices);
}

TEST_P(PomdpVerificationTest, UntilFailureCannotLaterReachTarget) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "POMDP preprocessing requires Z3.";
#endif
    auto pomdp = makePomdp();
    ASSERT_EQ(pomdp.getObservation(1), pomdp.getObservation(2));
    auto const matrix = pomdp.getTransitionMatrix();
    auto until = parser.parseSingleFormulaFromString("Pmax=? [\"safe\" U \"goal\"]");
    auto reachability = parser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");

    // State 2 violates safe, even though both of its actions can reach the goal later.
    // Only the 1/4 branch through state 1 contributes, using b with success probability 4/5.
    auto const result = verify(pomdp, *until);
    EXPECT_NEAR(result.value, 0.2, 1e-8);
    EXPECT_TRUE(result.completedExploration);
    EXPECT_NEAR(verify(pomdp, *reachability).value, 0.65, 1e-8);
    EXPECT_EQ(pomdp.getTransitionMatrix(), matrix);
}

TEST_P(PomdpVerificationTest, RejectsThresholds) {
    auto pomdp = makePomdp();
    auto formula = parser.parseSingleFormulaFromString("Pmax>=0.5 [F \"goal\"]");
    STORM_SILENT_EXPECT_THROW(verify(pomdp, *formula), storm::exceptions::NotSupportedException);
}

TEST_P(PomdpVerificationTest, RejectsNestedFormulas) {
    auto pomdp = makePomdp();
    auto formula = parser.parseSingleFormulaFromString("Pmax=? [F (Pmax>=0.5 [F \"goal\"])]");
    STORM_SILENT_EXPECT_THROW(verify(pomdp, *formula), storm::exceptions::NotSupportedException);
}

TEST_P(PomdpVerificationTest, RejectsMissingOptimizationDirection) {
    auto pomdp = makePomdp();
    for (auto const& query : {"P=? [F \"goal\"]", "R=? [F \"goal\"]"}) {
        SCOPED_TRACE(query);
        auto formula = parser.parseSingleFormulaFromString(query);
        STORM_SILENT_EXPECT_THROW(verify(pomdp, *formula), storm::exceptions::InvalidPropertyException);
    }
}

TEST_P(PomdpVerificationTest, SupportsRationalPolicyProbabilitiesWithDoubleModelAndBeliefs) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "POMDP preprocessing requires Z3.";
#endif
    auto pomdp = makePomdp();
    auto formula = parser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");
    Options options;
    options.generatePolicy = !GetParam();

    for (bool const defaultEnvironment : {false, true}) {
        SCOPED_TRACE(defaultEnvironment);
        auto const result = [&]() -> storm::pomdp::beliefs::BeliefBasedModelCheckerResult<double, storm::RationalNumber> {
            if (GetParam()) {
                return defaultEnvironment ? storm::pomdp::api::verifyWithBeliefDiscretization<double, double, double, storm::RationalNumber>(pomdp, *formula, 4,
                                                                                                                                             false, options)
                                          : storm::pomdp::api::verifyWithBeliefDiscretization<double, double, double, storm::RationalNumber>(
                                                env, pomdp, *formula, 4, false, options);
            }
            return defaultEnvironment
                       ? storm::pomdp::api::verifyWithBeliefUnfolding<double, double, double, storm::RationalNumber>(pomdp, *formula, options)
                       : storm::pomdp::api::verifyWithBeliefUnfolding<double, double, double, storm::RationalNumber>(env, pomdp, *formula, options);
        }();
        EXPECT_NEAR(result.value, 0.65, 1e-8);
        EXPECT_TRUE(result.completedExploration);
        if (GetParam()) {
            EXPECT_FALSE(result.policy.has_value());
        } else {
            ASSERT_TRUE(result.policy.has_value());
            auto const& policy = *result.policy;
            auto const initialNode = policy.getInitialNodeId();
            ASSERT_TRUE(policy.hasOutputForObservationInNode(initialNode, 0));
            auto const [initialAction, hiddenNode] = policy.getActionAndSuccessorForObservationInNode(initialNode, 0);
            EXPECT_EQ(initialAction, 0ul);
            ASSERT_TRUE(policy.hasOutputForObservationInNode(hiddenNode, 1));
            // The optimal action at the hidden belief is a (local action 0), with value 13/20.
            EXPECT_EQ(policy.getActionForObservationInNode(hiddenNode, 1), 0ul);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(BeliefApi, PomdpVerificationTest, ::testing::Values(false, true),
                         [](auto const& info) { return info.param ? "Discretization" : "Unfolding"; });

TEST(PomdpVerificationClippingTest, RefinesTargetsAndRemapsResolutionsWithoutChangingInput) {
#if !defined(STORM_HAVE_Z3) || !defined(STORM_HAVE_GLPK)
    GTEST_SKIP() << "Clipping test requires Z3 preprocessing and GLPK.";
#endif
    auto pomdp = makePomdp(false, true);
    ASSERT_EQ(pomdp.getNrObservations(), 3ul);
    ASSERT_EQ(pomdp.getObservation(1), pomdp.getObservation(3));
    auto const observations = pomdp.getObservations();
    storm::parser::FormulaParser parser;
    auto formula = parser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");
    storm::Environment env;
    env.solver().setLpSolverType(storm::solver::LpSolverType::Glpk);
    Options options;
    options.maxExplorationSize = 1;
    options.useClipping = true;
    // The hidden belief (1/4, 3/4) is off the resolution-3 grid, forcing clipping.
    options.clippingResolutions = {2, 3, 5};

    auto const result = storm::pomdp::api::verifyWithBeliefUnfolding(env, pomdp, *formula, options);

    // Explicitly separate the target, preserving the original observation IDs for other states.
    // The fresh observation 3 must inherit resolution 3 from original observation 1.
    auto refined = pomdp;
    refined.updateObservations({0, 1, 1, 3, 2}, true);
    auto refinedOptions = options;
    refinedOptions.clippingResolutions = {2, 3, 5, 3};
    auto const expected = storm::pomdp::api::verifyWithBeliefUnfolding(env, refined, *formula, refinedOptions);
    EXPECT_NEAR(result.value, 0.65, 1e-8);
    EXPECT_NEAR(result.value, expected.value, 1e-8);
    EXPECT_EQ(result.statistics.discoveredBeliefs, expected.statistics.discoveredBeliefs);
    EXPECT_GT(result.statistics.exploredBeliefs, 1ul);
    EXPECT_EQ(pomdp.getObservations(), observations);
    ASSERT_TRUE(options.clippingResolutions.has_value());
    EXPECT_EQ(*options.clippingResolutions, (std::vector<uint64_t>{2, 3, 5}));
    EXPECT_EQ(options.maxExplorationSize, 1ul);
}

TEST(PomdpVerificationBudgetTest, AcceptsExplicitUnfoldingBudgetAndRejectsDiscretizationBudget) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "POMDP preprocessing requires Z3.";
#endif
    auto pomdp = makePomdp();
    storm::parser::FormulaParser parser;
    auto formula = parser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");
    Options options;
    options.maxExplorationSize = 0;

    auto const result = storm::pomdp::api::verifyWithBeliefUnfolding(pomdp, *formula, options);
    EXPECT_FALSE(result.completedExploration);
    EXPECT_EQ(result.statistics.exploredBeliefs, 0ul);
    EXPECT_EQ(result.statistics.discoveredBeliefs, 1ul);
    EXPECT_GE(result.value, 0.0);
    EXPECT_LE(result.value, 0.65 + 1e-8);
    STORM_SILENT_EXPECT_THROW(storm::pomdp::api::verifyWithBeliefDiscretization(pomdp, *formula, 4, false, options), storm::exceptions::NotSupportedException);
}

}  // namespace
