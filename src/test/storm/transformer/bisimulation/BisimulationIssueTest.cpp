#include "BisimulationTestHelper.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/transformer/StatePermuter.h"

/*!
 * Regression tests for bisimulation issues that were reported against the old implementation, see
 * https://github.com/stormchecker/storm/issues/91, /498 and /833.
 */
namespace {

using storm::test::bisimulation::buildFromPrism;
using storm::test::bisimulation::buildMarkovAutomaton;
using storm::test::bisimulation::buildModel;
using storm::test::bisimulation::checkFormula;
using storm::test::bisimulation::Options;
using storm::test::bisimulation::strongOptions;

using ValueType = double;

/*!
 * @return options with the tolerance that the command line uses for inexact value types.
 */
Options approximateOptions() {
    Options options = strongOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-9);
    return options;
}

// ------------------------------------------------------------
// Issue 91: the sparse MDP quotient was too eager
// ------------------------------------------------------------

/*!
 * The reported symptom was that `storm --prism wlan1.nm --prop "Pmax=? [F col=COL]" -const "COL=1" -bisim` returned a quotient with two states in which one
 * state carried both the label "init" and the label "col=COL", although no state of the original model carried both.
 *
 * The cause was the measure-driven initial partition, which starts from the states that reach the target with probability zero resp. one. That partition is
 * only sound for the one property it was derived from; it is not a bisimulation, which is why it collapsed states carrying different labels. The new
 * implementation does not use it, so the labels stay separated and the quotient preserves all of PCTL rather than just one reachability probability.
 */
TEST(BisimulationIssueTest, Issue91MdpQuotientIsNotMeasureDriven) {
    // A chain 0 -> 1 -> 2 -> 2 in which the goal is reached with probability one, plus a second choice in state 0 that takes a detour through state 3.
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 0, false, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 1.0);  // state 0, choice 0
    builder.addNextValue(1, 3, 1.0);  // state 0, choice 1
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, 1.0);  // state 1
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, 1.0);  // state 2, absorbing and labeled "goal"
    builder.newRowGroup(4);
    builder.addNextValue(4, 1, 1.0);  // state 3
    auto const model = buildModel<storm::models::sparse::Mdp<ValueType>>(builder.build(), {{"goal", {2}}});

    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]")};
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;

    // Every state reaches the goal with probability one, so a measure-driven initial partition would have merged all four states into a single one.
    EXPECT_EQ(4ull, quotient->getNumberOfStates());
    // No quotient state may carry both "init" and "goal", because no state of the original model does.
    auto const& labeling = quotient->getStateLabeling();
    ASSERT_TRUE(labeling.containsLabel("init"));
    ASSERT_TRUE(labeling.containsLabel("goal"));
    EXPECT_TRUE(labeling.getStates("init").isDisjointFrom(labeling.getStates("goal")));
    // A step-bounded property distinguishes the states that a measure-driven partition would have merged.
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Pmax=? [F<=1 \"goal\"]"), checkFormula<ValueType>(model, "Pmax=? [F<=1 \"goal\"]"), 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Pmax=? [F<=2 \"goal\"]"), checkFormula<ValueType>(model, "Pmax=? [F<=2 \"goal\"]"), 1e-12);
}

/*!
 * The same check on a benchmark model: `Pmax=? [F "finished"]` is one in every state of coin2-2, so a measure-driven partition would collapse the model.
 */
TEST(BisimulationIssueTest, Issue91CoinQuotientIsNotMeasureDriven) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string const formulaString = "Pmax=? [F \"finished\"]";
    auto const input = buildFromPrism<ValueType>(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", formulaString);
    ASSERT_NEAR(1.0, checkFormula<ValueType>(input.model, formulaString), 1e-9);

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*input.model, input.formulas, strongOptions()).quotient;
    EXPECT_EQ(55ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), 1.0, 1e-9);
    // The quotient preserves all of PCTL, in particular the step-bounded variant of the formula it was built for.
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Pmax=? [F<=10 \"finished\"]"), checkFormula<ValueType>(input.model, "Pmax=? [F<=10 \"finished\"]"), 1e-9);
}

// ------------------------------------------------------------
// Issue 498: bisimulation changed the expected time on a Markov automaton
// ------------------------------------------------------------

/*!
 * The reported symptom was that `Tmin=? [F "done"]` on a Markov automaton evaluated to 4.74 instead of 11.01 after minimization. The report concerns the
 * symbolic (hybrid) engine and the model in question is not public, so the test below pins the property that was violated there for the sparse
 * implementation: the value of an expected-time property is only preserved if states with different exit rates are kept apart, since the time spent in a
 * Markovian state is exponentially distributed with its exit rate.
 */
TEST(BisimulationIssueTest, Issue498MarkovAutomatonExpectedTime) {
    // The probabilistic state 0 chooses between the Markovian states 1 and 2, which both move on to the "done" state 3 but at different rates.
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 0, false, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 1.0);  // state 0 is probabilistic, so its rows hold probabilities: choice 0
    builder.addNextValue(1, 2, 1.0);  // state 0, choice 1
    builder.newRowGroup(2);
    builder.addNextValue(2, 3, 1.0);  // state 1, Markovian with exit rate 1
    builder.newRowGroup(3);
    builder.addNextValue(3, 3, 2.0);  // state 2, Markovian with exit rate 2
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, 1.0);  // state 3, Markovian, absorbing and labeled "done"
    auto const model = buildMarkovAutomaton<ValueType>(builder.build(), storm::storage::BitVector(4, {1, 2, 3}), {{"done", {3}}});
    ASSERT_NEAR(0.5, checkFormula<ValueType>(model, "Tmin=? [F \"done\"]"), 1e-12);
    ASSERT_NEAR(1.0, checkFormula<ValueType>(model, "Tmax=? [F \"done\"]"), 1e-12);

    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString("Tmin=? [F \"done\"]")};
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;

    // States 1 and 2 have the same distribution over the blocks, so only their exit rates keep them apart.
    EXPECT_EQ(4ull, quotient->getNumberOfStates());
    EXPECT_NEAR(0.5, checkFormula<ValueType>(quotient, "Tmin=? [F \"done\"]"), 1e-12);
    EXPECT_NEAR(1.0, checkFormula<ValueType>(quotient, "Tmax=? [F \"done\"]"), 1e-12);
}

/*!
 * Expected time is preserved on a benchmark Markov automaton, too.
 */
TEST(BisimulationIssueTest, Issue498JobschedulerExpectedTime) {
    auto const model = storm::api::buildExplicitDRNModel<ValueType>(STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.drn");
    ASSERT_EQ(storm::models::ModelType::MarkovAutomaton, model->getType());

    storm::parser::FormulaParser formulaParser;
    for (std::string const& formulaString : {"Tmin=? [F \"all_jobs_finished\"]", "Tmax=? [F \"all_jobs_finished\"]"}) {
        std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString(formulaString)};
        auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;
        EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), checkFormula<ValueType>(model, formulaString), 1e-9) << "For " << formulaString << ".";
    }
}

// ------------------------------------------------------------
// Issue 833: the CTMC quotient depended on the order of the states
// ------------------------------------------------------------

/*!
 * The reported symptom was that the quotient of this CTMC had a different size on Linux than on macOS, and that exporting the model to a DRN file and
 * reading it back in changed the size again - even in exact arithmetic, where no rounding can be involved. In other words, the computed partition depended
 * on the order in which the states happened to be stored.
 */
TEST(BisimulationIssueTest, Issue833EmbeddedCtmcQuotient) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string const formulaString = "P=? [F<=10000 \"down\"]";
    auto const exactInput = buildFromPrism<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulaString, true);
    ASSERT_EQ(3478ull, exactInput.model->getNumberOfStates());
    ASSERT_EQ(14639ull, exactInput.model->getNumberOfTransitions());

    Options options = strongOptions();
    options.preserveAllStateLabels = true;
    auto const labeledQuotient =
        storm::bisimulation::performBisimulationMinimization<storm::RationalNumber>(*exactInput.model, exactInput.formulas, options).quotient;
    EXPECT_EQ(1127ull, labeledQuotient->getNumberOfStates());
    EXPECT_EQ(5730ull, labeledQuotient->getNumberOfTransitions());

    // Without the labels the quotient is much coarser. This is the configuration in which the reported sizes differed the most (136 on macOS vs 180 on
    // Linux), since the initial partition consists of a single block there.
    options.preserveAllStateLabels = false;
    auto const unlabeledQuotient = storm::bisimulation::performBisimulationMinimization<storm::RationalNumber>(*exactInput.model, {}, options).quotient;
    EXPECT_EQ(98ull, unlabeledQuotient->getNumberOfStates());
    EXPECT_EQ(539ull, unlabeledQuotient->getNumberOfTransitions());
}

/*!
 * The same model in floating point arithmetic. The rates of this model are not exactly representable as doubles, so the comparison needs a tolerance; with
 * the one that the command line uses by default, the quotient coincides with the exact one computed above.
 */
TEST(BisimulationIssueTest, Issue833EmbeddedCtmcQuotientWithDoubles) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string const formulaString = "P=? [F<=10000 \"down\"]";
    auto const input = buildFromPrism<double>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulaString, true);

    Options options = strongOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-9);
    options.preserveAllStateLabels = true;
    auto const labeledQuotient = storm::bisimulation::performBisimulationMinimization<double>(*input.model, input.formulas, options).quotient;
    EXPECT_EQ(1127ull, labeledQuotient->getNumberOfStates());
    EXPECT_EQ(5730ull, labeledQuotient->getNumberOfTransitions());

    options.preserveAllStateLabels = false;
    auto const unlabeledQuotient = storm::bisimulation::performBisimulationMinimization<double>(*input.model, {}, options).quotient;
    EXPECT_EQ(98ull, unlabeledQuotient->getNumberOfStates());
    EXPECT_EQ(539ull, unlabeledQuotient->getNumberOfTransitions());
}

/*!
 * Renaming the states of a model must not change the size of its quotient. In exact arithmetic this is a mathematical property of the coarsest bisimulation,
 * so a violation always indicates a bug in the refinement. The check is run with all labels preserved, which makes the initial partition (and hence the
 * quotient) as fine as possible.
 */
template<typename VT>
void testPermutationInvariance(std::string const& prismFile, std::string const& formulaString, uint64_t const numSeeds, Options options = strongOptions()) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    options.preserveAllStateLabels = true;
    auto const input = buildFromPrism<VT>(prismFile, formulaString, true);
    auto const quotient = storm::bisimulation::performBisimulationMinimization<VT>(*input.model, input.formulas, options).quotient;

    std::vector<uint64_t> permutation(input.model->getNumberOfStates());
    std::iota(permutation.begin(), permutation.end(), 0ull);
    for (uint64_t seed = 0; seed < numSeeds; ++seed) {
        std::mt19937_64 rng(seed);
        std::shuffle(permutation.begin(), permutation.end(), rng);
        auto const permutedModel = storm::transformer::permuteStates(*input.model, permutation);
        auto const permutedQuotient = storm::bisimulation::performBisimulationMinimization<VT>(*permutedModel, input.formulas, options).quotient;
        EXPECT_EQ(quotient->getNumberOfStates(), permutedQuotient->getNumberOfStates()) << "Seed " << seed << ".";
        EXPECT_EQ(quotient->getNumberOfTransitions(), permutedQuotient->getNumberOfTransitions()) << "Seed " << seed << ".";
    }
}

TEST(BisimulationIssueTest, Issue833PermutationInvarianceExact) {
    testPermutationInvariance<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 3ull);
}

TEST(BisimulationIssueTest, Issue833PermutationInvarianceDouble) {
    testPermutationInvariance<double>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 3ull, approximateOptions());
}

TEST(BisimulationIssueTest, Issue833PermutationInvarianceMdp) {
    testPermutationInvariance<double>(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 3ull, approximateOptions());
}

}  // namespace
