#include "BisimulationTestHelper.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <set>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/transformer/StatePermuter.h"

namespace {

using storm::test::bisimulation::buildFromPrism;
using storm::test::bisimulation::buildMarkovAutomaton;
using storm::test::bisimulation::buildModel;
using storm::test::bisimulation::BuildOptions;
using storm::test::bisimulation::checkFormula;
using storm::test::bisimulation::Options;
using storm::test::bisimulation::StateLabelPreservation;
using storm::test::bisimulation::strongOptions;

using ValueType = double;

/*!
 * Checks the number of states, transitions and choices of the bisimulation quotient of the model built from the given PRISM file, plus that the quotient
 * preserves the value of the formula.
 *
 * If `options.stateLabelPreservation` is `All`, the full state space is built with all labels of the program. Otherwise, the formula may restrict the
 * exploration, e.g. by making the target states of a reachability formula absorbing.
 */
void testQuotient(std::string const& prismFile, std::string const& formulaString, uint64_t expectedModelStates, uint64_t expectedStates,
                  uint64_t expectedTransitions, uint64_t expectedChoices, Options const options = strongOptions(), BuildOptions buildOptions = {}) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    if (options.stateLabelPreservation == StateLabelPreservation::All) {
        buildOptions.allLabels = true;
    }
    auto const input = buildFromPrism<ValueType>(prismFile, formulaString, buildOptions);
    ASSERT_EQ(expectedModelStates, input.model->getNumberOfStates());

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*input.model, input.formulas, options).quotient;
    EXPECT_EQ(input.model->getType(), quotient->getType());
    EXPECT_EQ(expectedStates, quotient->getNumberOfStates());
    EXPECT_EQ(expectedTransitions, quotient->getNumberOfTransitions());
    EXPECT_EQ(expectedChoices, quotient->getNumberOfChoices());
    // The quotient keeps the choice labels and the choice origins of the model (but not necessarily an empty choice labeling).
    auto const choiceLabels = [](auto const& model) {
        std::set<std::string> result;
        if (model->hasChoiceLabeling()) {
            auto const labels = model->getChoiceLabeling().getLabels();
            result.insert(labels.begin(), labels.end());
        }
        return result;
    };
    EXPECT_EQ(choiceLabels(input.model), choiceLabels(quotient));
    EXPECT_EQ(input.model->hasChoiceOrigins(), quotient->hasChoiceOrigins());

    // The formulas were parsed for the PRISM program, so unlike the formula string they can be checked even if they contain atomic expressions.
    for (auto const& formula : input.formulas) {
        EXPECT_NEAR(checkFormula<ValueType>(quotient, formula), checkFormula<ValueType>(input.model, formula), 1e-9) << *formula;
    }
}

/*!
 * @return options that make the quotient preserve every state label of the model.
 */
Options allLabelOptions() {
    Options options = strongOptions();
    options.stateLabelPreservation = StateLabelPreservation::All;
    return options;
}

/*!
 * @return options with a default tolerance used for inexact value types. Besides grouping almost-equal values, a positive tolerance also
 * switches signature-based refinement from exact to approximative signatures.
 */
Options approximateOptions() {
    Options options = strongOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-9);
    return options;
}

// ------------------------------------------------------------
// Deterministic models
// ------------------------------------------------------------

/*!
 * Two states with the same distribution over the blocks are merged; unlike weak bisimulation, strong bisimulation cannot collapse a chain of silent states.
 */
TEST(StrongBisimulationTest, SilentChainIsNotCollapsed) {
    // 0 -> 1 -> 2 -> {3, 4}; 3 and 4 absorbing, 3 labeled "goal".
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5);
    builder.addNextValue(0, 1, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 3, 0.5);
    builder.addNextValue(2, 4, 0.5);
    builder.addNextValue(3, 3, 1.0);
    builder.addNextValue(4, 4, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {3}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(5ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 0.5, 1e-12);
}

/*!
 * States with identical distributions over the blocks are merged, even if they are reached differently.
 */
TEST(StrongBisimulationTest, IdenticalDistributions) {
    // 1 and 2 have the same distribution over {3} and {4}, so they are merged; 0 keeps them apart only through its own distribution.
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(1, 3, 0.25);
    builder.addNextValue(1, 4, 0.75);
    builder.addNextValue(2, 3, 0.25);
    builder.addNextValue(2, 4, 0.75);
    builder.addNextValue(3, 3, 1.0);
    builder.addNextValue(4, 4, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {3}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(4ull, quotient->getNumberOfStates());  // {0}, {1,2}, {3}, {4}
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 0.25, 1e-12);
}

/*!
 * On a CTMC the rate into the own block is observable for strong bisimulation, in contrast to weak bisimulation.
 */
TEST(StrongBisimulationTest, CtmcInternalRateIsObservable) {
    // 0 -> 1 with rate 100 and 0 -> 2 with rate 1; 1 -> 2 with rate 1; 2 absorbing and labeled "goal".
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 100.0);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(storm::models::ModelType::Ctmc, quotient->getType());
    EXPECT_EQ(3ull, quotient->getNumberOfStates());  // weak bisimulation merges 0 and 1 here, strong bisimulation does not
}

/*!
 * Two CTMC states with the same successor distribution but different exit rates are not bisimilar.
 */
TEST(StrongBisimulationTest, CtmcExitRateIsObservable) {
    // 0 and 1 both move to 2 only, but at different speeds.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 2.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
}

/*!
 * The formula only observes the labels through the propositional subformula "a" & !"b", even if that subformula is nested within another operator. Hence,
 * the states 1 and 2 are merged although they differ in both labels, unless the labels are preserved individually.
 */
TEST(StrongBisimulationTest, PropositionalSubformulas) {
    // 0 moves uniformly to 1, 2 and 3, which are absorbing. 1 is labeled with "a" and "b", 2 is unlabeled and 3 is labeled with "a".
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 4);
    builder.addNextValue(0, 1, 1.0 / 3);
    builder.addNextValue(0, 2, 1.0 / 3);
    builder.addNextValue(0, 3, 1.0 / 3);
    builder.addNextValue(1, 1, 1.0);
    builder.addNextValue(2, 2, 1.0);
    builder.addNextValue(3, 3, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"a", {1, 3}}, {"b", {1}}});
    Options individualOptions = strongOptions();
    individualOptions.stateLabelPreservation = StateLabelPreservation::FormulaIndividual;

    storm::parser::FormulaParser formulaParser;
    for (std::string const formulaString : {"P=? [F \"a\" & !\"b\"]", "P=? [F P>=0.5 [F \"a\" & !\"b\"]]"}) {
        std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString(formulaString)};
        auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;
        EXPECT_EQ(3ull, quotient->getNumberOfStates()) << formulaString;  // {0}, {1, 2} and {3}
        EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), 1.0 / 3, 1e-12) << formulaString;

        auto const individualQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, individualOptions).quotient;
        EXPECT_EQ(4ull, individualQuotient->getNumberOfStates()) << formulaString;
        EXPECT_NEAR(checkFormula<ValueType>(individualQuotient, formulaString), 1.0 / 3, 1e-12) << formulaString;
    }
}

/*!
 * The quotient takes a preserved "init" label from the representative states. Hence, the blocks have to respect this label on its own, even if it only occurs
 * within a larger propositional subformula.
 */
TEST(StrongBisimulationTest, InitLabelInPropositionalSubformula) {
    // 0 and 1 both move to the absorbing state 2, which is labeled "a". Only 1 is initial, so merging 0 and 1 would make 0 the representative of the initial
    // block and thereby lose the initial state.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"a", {2}}});
    model->getStateLabeling().removeLabelFromState("init", 0);
    model->getStateLabeling().addLabelToState("init", 1);

    std::string const formulaString = "P=? [F \"a\" & !\"init\"]";
    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString(formulaString)};
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    ASSERT_EQ(1ull, quotient->getInitialStates().getNumberOfSetBits());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), 1.0, 1e-12);
}

/*!
 * A quotient state gets the "init" label if it represents an initial state, whereas its other labels are taken from the representative state (cf. Quotient).
 * A formula that combines "init" with another label would thus be evaluated on a mix of two states, unless the blocks respect the "init" label.
 */
TEST(StrongBisimulationTest, InitLabelCombinedWithOtherLabel) {
    // 0 (labeled "a") and 1 (the only initial state) both move to the absorbing state 2. No state satisfies "init" & "a", but a block {0, 1, 2} would be
    // represented by state 0 and thus yield a quotient state that is both initial and labeled "a".
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"a", {0}}});
    model->getStateLabeling().removeLabelFromState("init", 0);
    model->getStateLabeling().addLabelToState("init", 1);

    std::string const formulaString = "P=? [F \"init\" & \"a\"]";
    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString(formulaString)};
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;
    EXPECT_EQ(2ull, quotient->getNumberOfStates());  // {1} and {0, 2}
    EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), 0.0, 1e-12);
}

// ------------------------------------------------------------
// Nondeterministic models
// ------------------------------------------------------------

/*!
 * Two states are bisimilar if their sets of choice distributions coincide. Neither the order of the choices nor duplicates among them matter.
 */
TEST(StrongBisimulationTest, ChoiceSetsAreCompared) {
    // States 0 and 1 offer the same two distributions over {2} and {3}, but in a different order, and 1 offers one of them twice.
    storm::storage::SparseMatrixBuilder<ValueType> builder(7, 4, 0, false, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 2, 1.0);  // state 0, choice 0
    builder.addNextValue(1, 2, 0.5);  // state 0, choice 1
    builder.addNextValue(1, 3, 0.5);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, 0.5);  // state 1, choice 0
    builder.addNextValue(2, 3, 0.5);
    builder.addNextValue(3, 2, 1.0);  // state 1, choice 1
    builder.addNextValue(4, 2, 1.0);  // state 1, choice 2 (a duplicate of choice 1)
    builder.newRowGroup(5);
    builder.addNextValue(5, 2, 1.0);  // state 2, absorbing and labeled "goal"
    builder.newRowGroup(6);
    builder.addNextValue(6, 3, 1.0);  // state 3, absorbing
    auto const model = buildModel<storm::models::sparse::Mdp<ValueType>>(builder.build(), {{"goal", {2}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());  // {0,1}, {2}, {3}
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Pmin=? [F \"goal\"]"), 0.5, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Pmax=? [F \"goal\"]"), 1.0, 1e-12);
}

/*!
 * If the bisimulation is action-sensitive, the i-th choice of a state can only be matched with the i-th choice of another state, so the order of the choices
 * matters.
 */
TEST(StrongBisimulationTest, ChoiceOrderMattersIfActionSensitive) {
    // States 0 and 1 offer the same two distributions over {2} and {3}, but in a different order.
    storm::storage::SparseMatrixBuilder<ValueType> builder(6, 4, 0, false, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 2, 1.0);  // state 0, choice 0
    builder.addNextValue(1, 2, 0.5);  // state 0, choice 1
    builder.addNextValue(1, 3, 0.5);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, 0.5);  // state 1, choice 0
    builder.addNextValue(2, 3, 0.5);
    builder.addNextValue(3, 2, 1.0);  // state 1, choice 1
    builder.newRowGroup(4);
    builder.addNextValue(4, 2, 1.0);  // state 2, absorbing and labeled "goal"
    builder.newRowGroup(5);
    builder.addNextValue(5, 3, 1.0);  // state 3, absorbing
    auto const model = buildModel<storm::models::sparse::Mdp<ValueType>>(builder.build(), {{"goal", {2}}});

    EXPECT_EQ(3ull, storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient->getNumberOfStates());

    Options options = strongOptions();
    options.actionSensitive = true;
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
    EXPECT_EQ(4ull, quotient->getNumberOfStates());
    EXPECT_EQ(6ull, quotient->getNumberOfChoices());
}

/*!
 * With a positive tolerance, two states are bisimilar if every choice of one has an approximately equal choice in the other. That partner does not need to be
 * at the same position of the (sorted) signature.
 */
TEST(StrongBisimulationTest, ApproximateChoicePartnersAtDifferentPositions) {
    // States 0 and 1 have three choices each over "goal" (2) and "sink" (3). Every choice has a partner within 0.01 in the other state, but the middle
    // choices are 0.011 apart.
    storm::storage::SparseMatrixBuilder<ValueType> builder(8, 4, 0, false, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 2, 0.001);
    builder.addNextValue(0, 3, 0.999);
    builder.addNextValue(1, 2, 0.019);
    builder.addNextValue(1, 3, 0.981);
    builder.addNextValue(2, 2, 0.039);
    builder.addNextValue(2, 3, 0.961);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, 0.010);
    builder.addNextValue(3, 3, 0.990);
    builder.addNextValue(4, 2, 0.030);
    builder.addNextValue(4, 3, 0.970);
    builder.addNextValue(5, 2, 0.048);
    builder.addNextValue(5, 3, 0.952);
    builder.newRowGroup(6);
    builder.addNextValue(6, 2, 1.0);
    builder.newRowGroup(7);
    builder.addNextValue(7, 3, 1.0);
    auto const model = buildModel<storm::models::sparse::Mdp<ValueType>>(builder.build(), {{"goal", {2}}, {"sink", {3}}});

    EXPECT_EQ(4ull, storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient->getNumberOfStates());
    Options options = strongOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(0.02);  // i.e., halfTolerance 0.01
    EXPECT_EQ(3ull, storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient->getNumberOfStates());
}

/*!
 * Regression test for https://github.com/stormchecker/storm/issues/91: `storm --prism wlan1.nm --prop "Pmax=? [F col=COL]" -const "COL=1" -bisim` returned a
 * quotient with two states in which one state carried both the label "init" and the label "col=COL", although no state of the original model carried both.
 *
 * The cause was the measure-driven initial partition of the old implementation, which starts from the states that reach the target with probability zero
 * resp. one. That partition is only sound for the one property it was derived from; it is not a bisimulation, which is why it collapsed states carrying
 * different labels. The quotient has to preserve all of PCTL rather than just one reachability probability.
 */
TEST(StrongBisimulationTest, MdpQuotientIsNotMeasureDriven) {
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
 * Markov automata mix Markovian and probabilistic states. The Markovian flag and the exit rates are part of the initial partition, so the two kinds of state
 * are never merged.
 */
TEST(StrongBisimulationTest, MarkovAutomaton) {
    auto const model = storm::api::buildExplicitDRNModel<ValueType>(STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.drn");
    ASSERT_EQ(storm::models::ModelType::MarkovAutomaton, model->getType());

    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString("Tmin=? [F \"all_jobs_finished\"]")};

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strongOptions()).quotient;
    EXPECT_EQ(storm::models::ModelType::MarkovAutomaton, quotient->getType());
    EXPECT_LE(quotient->getNumberOfStates(), model->getNumberOfStates());
    auto const markovAutomaton = quotient->template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
    // Markovian and probabilistic states must not be mixed, and a Markovian state has exactly one choice.
    for (uint64_t state = 0; state < quotient->getNumberOfStates(); ++state) {
        if (markovAutomaton->isMarkovianState(state)) {
            EXPECT_EQ(1ull, quotient->getTransitionMatrix().getRowGroupSize(state));
        }
    }
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Tmin=? [F \"all_jobs_finished\"]"), checkFormula<ValueType>(model, "Tmin=? [F \"all_jobs_finished\"]"),
                1e-9);
    // Expected times are preserved in both optimization directions, cf. https://github.com/stormchecker/storm/issues/498.
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "Tmax=? [F \"all_jobs_finished\"]"), checkFormula<ValueType>(model, "Tmax=? [F \"all_jobs_finished\"]"),
                1e-9);
}

/*!
 * Regression test for https://github.com/stormchecker/storm/issues/498, where `Tmin=? [F "done"]` on a Markov automaton evaluated to 4.74 instead of 11.01
 * after minimization. The report concerns the symbolic (hybrid) engine and the model in question is not public, so this test pins the property that was
 * violated there: the value of an expected-time property is only preserved if states with different exit rates are kept apart, since the time spent in a
 * Markovian state is exponentially distributed with its exit rate.
 */
TEST(StrongBisimulationTest, MarkovAutomatonExpectedTime) {
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

// ------------------------------------------------------------
// Benchmark models
// ------------------------------------------------------------

TEST(StrongBisimulationTest, Die) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]", 13ull, 5ull, 8ull, 5ull);
}

TEST(StrongBisimulationTest, DieAllLabels) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]", 13ull, 11ull, 17ull, 11ull, allLabelOptions());
}

/*!
 * A reward operator without a reward model name refers to the unique reward model of the model.
 */
TEST(StrongBisimulationTest, DieUnnamedRewardModel) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "R=? [F \"done\"]", 13ull, 5ull, 7ull, 5ull);
}

/*!
 * The states where the die shows one or two can be merged.
 */
TEST(StrongBisimulationTest, DiePropositionalSubformulas) {
    std::string const formulaString = "P=? [F (s=7 & d=1) | \"two\"]";
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulaString, 13ull, 6ull, 10ull, 6ull);
    Options options = strongOptions();
    options.stateLabelPreservation = StateLabelPreservation::FormulaIndividual;
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulaString, 13ull, 7ull, 11ull, 7ull, options);
}

TEST(StrongBisimulationTest, Crowds) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 7403ull, 65ull, 105ull, 65ull);
}

TEST(StrongBisimulationTest, CrowdsAllLabels) {
    // The model is larger than above because the target states of the formula are no longer made absorbing.
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 8607ull, 2149ull, 3912ull, 2149ull, allLabelOptions());
}

TEST(StrongBisimulationTest, CtmcEmbedded) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 2076ull, 310ull, 1752ull, 310ull, approximateOptions());
}

TEST(StrongBisimulationTest, CtmcCluster) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", "P=? [F<=100 !\"minimum\"]", 276ull, 147ull, 569ull, 147ull);
}

TEST(StrongBisimulationTest, CtmcClusterChoiceLabelsAndOrigins) {
    // The actions of the left and the right cluster have different names and stem from different modules, which rules out any reduction.
    testQuotient(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", "P=? [F<=100 !\"minimum\"]", 276ull, 276ull, 1120ull, 276ull, strongOptions(),
                 {.choiceLabels = true, .choiceOrigins = true});
}

TEST(StrongBisimulationTest, DtmcBrpChoiceLabelsAndOrigins) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [F \"target\"]", 613ull, 412ull, 572ull, 412ull, strongOptions(),
                 {.choiceLabels = true, .choiceOrigins = true});
}

/*!
 * Regression test for https://github.com/stormchecker/storm/issues/833: the quotient of this CTMC had a different size on Linux than on macOS, and exporting
 * the model to a DRN file and reading it back in changed the size again - even in exact arithmetic, where no rounding can be involved. In other words, the
 * computed partition depended on the order in which the states happened to be stored, cf. the PermutationInvariance tests.
 */
TEST(StrongBisimulationTest, CtmcEmbeddedExact) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string const formulaString = "P=? [F<=10000 \"down\"]";
    auto const exactInput = buildFromPrism<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulaString, {.allLabels = true});
    ASSERT_EQ(3478ull, exactInput.model->getNumberOfStates());
    ASSERT_EQ(14639ull, exactInput.model->getNumberOfTransitions());

    Options options = strongOptions();
    options.stateLabelPreservation = StateLabelPreservation::All;
    auto const labeledQuotient =
        storm::bisimulation::performBisimulationMinimization<storm::RationalNumber>(*exactInput.model, exactInput.formulas, options).quotient;
    EXPECT_EQ(1127ull, labeledQuotient->getNumberOfStates());
    EXPECT_EQ(5730ull, labeledQuotient->getNumberOfTransitions());

    // Without the labels the quotient is much coarser. This is the configuration in which the reported sizes differed the most (136 on macOS vs 180 on
    // Linux), since the initial partition consists of a single block there.
    options.stateLabelPreservation = StateLabelPreservation::None;
    auto const unlabeledQuotient = storm::bisimulation::performBisimulationMinimization<storm::RationalNumber>(*exactInput.model, {}, options).quotient;
    EXPECT_EQ(98ull, unlabeledQuotient->getNumberOfStates());
    EXPECT_EQ(539ull, unlabeledQuotient->getNumberOfTransitions());
}

/*!
 * The same model in floating point arithmetic. The rates of this model are not exactly representable as doubles, so the comparison needs a tolerance; with
 * the one that the command line uses by default, the quotient coincides with the exact one computed in CtmcEmbeddedExact.
 */
TEST(StrongBisimulationTest, CtmcEmbeddedWithTolerance) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string const formulaString = "P=? [F<=10000 \"down\"]";
    auto const input = buildFromPrism<double>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulaString, {.allLabels = true});

    Options options = approximateOptions();
    options.stateLabelPreservation = StateLabelPreservation::All;
    auto const labeledQuotient = storm::bisimulation::performBisimulationMinimization<double>(*input.model, input.formulas, options).quotient;
    EXPECT_EQ(1127ull, labeledQuotient->getNumberOfStates());
    EXPECT_EQ(5730ull, labeledQuotient->getNumberOfTransitions());

    options.stateLabelPreservation = StateLabelPreservation::None;
    auto const unlabeledQuotient = storm::bisimulation::performBisimulationMinimization<double>(*input.model, {}, options).quotient;
    EXPECT_EQ(98ull, unlabeledQuotient->getNumberOfStates());
    EXPECT_EQ(539ull, unlabeledQuotient->getNumberOfTransitions());
}

TEST(StrongBisimulationTest, MdpTwoDice) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 169ull, 11ull, 26ull, 14ull);
}

TEST(StrongBisimulationTest, MdpTwoDiceAllLabels) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 169ull, 77ull, 183ull, 97ull, allLabelOptions());
}

TEST(StrongBisimulationTest, MdpCoin) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmin=? [F \"finished\"]", 272ull, 55ull, 96ull, 78ull);
}

/*!
 * Like MdpQuotientIsNotMeasureDriven on a benchmark model: `Pmax=? [F "finished"]` is one in every state of coin2-2, so a measure-driven partition would
 * collapse the model.
 */
TEST(StrongBisimulationTest, MdpCoinQuotientIsNotMeasureDriven) {
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

TEST(StrongBisimulationTest, MdpTwoDiceActionSensitive) {
    Options options = strongOptions();
    options.actionSensitive = true;
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 169ull, 33ull, 97ull, 58ull, options);
}

TEST(StrongBisimulationTest, MdpCoinChoiceOrigins) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmin=? [F \"finished\"]", 272ull, 195ull, 399ull, 321ull, strongOptions(),
                 {.choiceOrigins = true});
}

TEST(StrongBisimulationTest, MdpLeaderChoiceLabels) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm", "Pmin=? [F \"elected\"]", 364ull, 169ull, 325ull, 262ull, strongOptions(), {.choiceLabels = true});
}

TEST(StrongBisimulationTest, MdpLeaderChoiceLabelsAndOriginsActionSensitive) {
    Options options = strongOptions();
    options.actionSensitive = true;
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm", "Pmin=? [F \"elected\"]", 364ull, 169ull, 325ull, 262ull, options,
                 {.choiceLabels = true, .choiceOrigins = true});
}

/*!
 * The transition probabilities of these models are far enough apart that the approximative signatures group exactly the same values as the exact ones.
 */
TEST(StrongBisimulationTest, MdpTwoDiceApproximative) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 169ull, 11ull, 26ull, 14ull, approximateOptions());
}

TEST(StrongBisimulationTest, MdpCoinApproximative) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmin=? [F \"finished\"]", 272ull, 55ull, 96ull, 78ull, approximateOptions());
}

// ------------------------------------------------------------
// Consistency between the refinement strategies and the value types
// ------------------------------------------------------------

/*!
 * Deterministic models are refined with the splitter-based algorithm by default, but signature-based refinement has to compute the same quotient.
 */
template<typename VT>
void testRefinementStrategiesAgree(std::string const& prismFile, std::string const& formulaString) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto const input = buildFromPrism<VT>(prismFile, formulaString);

    auto splitterOptions = strongOptions();
    splitterOptions.preferSignatureRefinement = false;
    auto const splitterQuotient = storm::bisimulation::performBisimulationMinimization<VT>(*input.model, input.formulas, splitterOptions).quotient;

    auto signatureOptions = splitterOptions;
    signatureOptions.preferSignatureRefinement = true;
    auto const signatureQuotient = storm::bisimulation::performBisimulationMinimization<VT>(*input.model, input.formulas, signatureOptions).quotient;

    EXPECT_EQ(splitterQuotient->getNumberOfStates(), signatureQuotient->getNumberOfStates());
    EXPECT_EQ(splitterQuotient->getNumberOfTransitions(), signatureQuotient->getNumberOfTransitions());
}

TEST(StrongBisimulationTest, RefinementStrategiesAgreeOnDie) {
    testRefinementStrategiesAgree<ValueType>(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]");
}

TEST(StrongBisimulationTest, RefinementStrategiesAgreeOnCrowds) {
    testRefinementStrategiesAgree<ValueType>(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]");
}

/*!
 * With doubles the two strategies do not agree on this model: the values are accumulated sums, the two strategies accumulate them in a different order, and
 * rounding alone then decides some of the splits. In exact arithmetic that effect is gone, cf. CtmcEmbeddedExact.
 */
TEST(StrongBisimulationTest, RefinementStrategiesAgreeOnCtmc) {
    testRefinementStrategiesAgree<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]");
}

/*!
 * On a model whose transition values are exactly representable as doubles, the approximate computation has to yield the same quotient as the exact one.
 */
void testExactAgreesWithApproximate(std::string const& prismFile, std::string const& formulaString) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto const doubleInput = buildFromPrism<double>(prismFile, formulaString);
    auto const exactInput = buildFromPrism<storm::RationalNumber>(prismFile, formulaString);

    auto const doubleQuotient =
        storm::bisimulation::performBisimulationMinimization<double>(*doubleInput.model, doubleInput.formulas, strongOptions()).quotient;
    auto const exactQuotient =
        storm::bisimulation::performBisimulationMinimization<storm::RationalNumber>(*exactInput.model, exactInput.formulas, strongOptions()).quotient;

    EXPECT_EQ(doubleQuotient->getNumberOfStates(), exactQuotient->getNumberOfStates());
    EXPECT_EQ(doubleQuotient->getNumberOfTransitions(), exactQuotient->getNumberOfTransitions());
}

TEST(StrongBisimulationTest, ExactAgreesWithApproximateOnDie) {
    testExactAgreesWithApproximate(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]");
}

TEST(StrongBisimulationTest, ExactAgreesWithApproximateOnTwoDice) {
    testExactAgreesWithApproximate(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]");
}

/*!
 * Renaming the states of a model must not change the size of its quotient, cf. https://github.com/stormchecker/storm/issues/833 and CtmcEmbeddedExact. In exact
 * arithmetic this is a mathematical property of the coarsest bisimulation, so a violation always indicates a bug in the refinement. The check is run with all
 * labels preserved, which makes the initial partition (and hence the quotient) as fine as possible.
 */
template<typename VT>
void testPermutationInvariance(std::string const& prismFile, std::string const& formulaString, uint64_t const numSeeds, Options options = strongOptions()) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    options.stateLabelPreservation = StateLabelPreservation::All;
    auto const input = buildFromPrism<VT>(prismFile, formulaString, {.allLabels = true});
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

TEST(StrongBisimulationTest, PermutationInvarianceExactCtmc) {
    testPermutationInvariance<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 3ull);
}

TEST(StrongBisimulationTest, PermutationInvarianceCtmc) {
    testPermutationInvariance<double>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 3ull, approximateOptions());
}

TEST(StrongBisimulationTest, PermutationInvarianceMdp) {
    testPermutationInvariance<double>(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [F \"two\"]", 3ull, approximateOptions());
}

// ------------------------------------------------------------
// Randomized self-checks
// ------------------------------------------------------------

/*!
 * Builds a pseudo-random model. Deterministic models get one to three successors per state, nondeterministic ones additionally get one or two choices per
 * state. Every state carries a random subset of the labels "a" and "b".
 */
template<typename ModelType>
std::shared_ptr<ModelType> buildRandomModel(uint64_t const numStates, uint64_t const seed) {
    bool constexpr isNondeterministic = std::is_same_v<ModelType, storm::models::sparse::Mdp<ValueType>>;
    std::mt19937_64 rng(seed);
    storm::storage::SparseMatrixBuilder<ValueType> builder(0, numStates, 0, false, isNondeterministic, isNondeterministic ? numStates : 0);
    uint64_t row = 0;
    for (uint64_t state = 0; state < numStates; ++state) {
        if constexpr (isNondeterministic) {
            builder.newRowGroup(row);
        }
        uint64_t const numChoices = isNondeterministic ? 1 + rng() % 2 : 1;
        for (uint64_t choice = 0; choice < numChoices; ++choice, ++row) {
            std::map<uint64_t, ValueType> distribution;
            uint64_t const numSuccessors = 1 + rng() % 3;
            for (uint64_t i = 0; i < numSuccessors; ++i) {
                // Small values keep the rows short and make coinciding distributions (and thus actual merges) reasonably likely.
                distribution[rng() % numStates] += static_cast<ValueType>(1 + rng() % 4);
            }
            ValueType sum = storm::utility::zero<ValueType>();
            for (auto const& [_, value] : distribution) {
                sum += value;
            }
            for (auto const& [column, value] : distribution) {
                builder.addNextValue(row, column, value / sum);
            }
        }
    }
    std::map<std::string, std::vector<uint64_t>> labels{{"a", {}}, {"b", {}}};
    for (uint64_t state = 0; state < numStates; ++state) {
        if (rng() % 3 == 0) {
            labels["a"].push_back(state);
        }
        if (rng() % 4 == 0) {
            labels["b"].push_back(state);
        }
    }
    return buildModel<ModelType>(builder.build(row, numStates, numStates), labels);
}

/*!
 * Minimizing an already minimal model must not change it any further. This is a strong self-check: if the refinement split a block that it should not have,
 * the second round has a chance to merge it again, and if it stopped too early, so does the first.
 *
 * Unlike weak bisimulation, strong bisimulation never divides by an accumulated sum, so comparing doubles exactly (i.e. with tolerance zero, the default)
 * is a genuine equivalence here and the refinement is reproducible.
 */
template<typename ModelType>
void testIdempotence(uint64_t const numStates, uint64_t const numSeeds, std::string const& formulaString) {
    for (uint64_t seed = 0; seed < numSeeds; ++seed) {
        auto const model = buildRandomModel<ModelType>(numStates, seed);
        Options const options = strongOptions();
        auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
        ASSERT_LE(quotient->getNumberOfStates(), model->getNumberOfStates()) << "Seed " << seed << ".";
        auto const quotientOfQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*quotient, {}, options).quotient;
        EXPECT_EQ(quotient->getNumberOfStates(), quotientOfQuotient->getNumberOfStates()) << "Not idempotent for seed " << seed << ".";
        EXPECT_EQ(quotient->getNumberOfTransitions(), quotientOfQuotient->getNumberOfTransitions()) << "Not idempotent for seed " << seed << ".";
        // The comparison is against the default precision of the underlying equation solver, not against the exactness of the quotient.
        EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), checkFormula<ValueType>(model, formulaString), 1e-4) << "Seed " << seed << ".";
    }
}

TEST(StrongBisimulationTest, RandomDtmcs) {
    testIdempotence<storm::models::sparse::Dtmc<ValueType>>(12, 500, "P=? [F \"a\"]");
    testIdempotence<storm::models::sparse::Dtmc<ValueType>>(60, 200, "P=? [(!\"b\") U \"a\"]");
}

TEST(StrongBisimulationTest, RandomCtmcs) {
    testIdempotence<storm::models::sparse::Ctmc<ValueType>>(12, 500, "P=? [F \"a\"]");
    testIdempotence<storm::models::sparse::Ctmc<ValueType>>(60, 200, "P=? [(!\"b\") U \"a\"]");
}

TEST(StrongBisimulationTest, RandomMdps) {
    testIdempotence<storm::models::sparse::Mdp<ValueType>>(12, 500, "Pmin=? [F \"a\"]");
    testIdempotence<storm::models::sparse::Mdp<ValueType>>(60, 200, "Pmax=? [(!\"b\") U \"a\"]");
}

/*!
 * Signature-based refinement has to compute the same quotient as the splitter-based one on the randomly generated deterministic models, too.
 */
template<typename ModelType>
void testRefinementStrategiesAgreeOnRandomModels(uint64_t const numStates, uint64_t const numSeeds) {
    for (uint64_t seed = 0; seed < numSeeds; ++seed) {
        auto const model = buildRandomModel<ModelType>(numStates, seed);
        auto splitterOptions = strongOptions();
        splitterOptions.preferSignatureRefinement = false;
        auto const splitterQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, splitterOptions).quotient;
        auto signatureOptions = splitterOptions;
        signatureOptions.preferSignatureRefinement = true;
        auto const signatureQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, signatureOptions).quotient;
        EXPECT_EQ(splitterQuotient->getNumberOfStates(), signatureQuotient->getNumberOfStates()) << "Seed " << seed << ".";
        EXPECT_EQ(splitterQuotient->getNumberOfTransitions(), signatureQuotient->getNumberOfTransitions()) << "Seed " << seed << ".";
    }
}

TEST(StrongBisimulationTest, RefinementStrategiesAgreeOnRandomDtmcs) {
    testRefinementStrategiesAgreeOnRandomModels<storm::models::sparse::Dtmc<ValueType>>(12, 500);
    testRefinementStrategiesAgreeOnRandomModels<storm::models::sparse::Dtmc<ValueType>>(60, 200);
}

TEST(StrongBisimulationTest, RefinementStrategiesAgreeOnRandomCtmcs) {
    testRefinementStrategiesAgreeOnRandomModels<storm::models::sparse::Ctmc<ValueType>>(12, 500);
    testRefinementStrategiesAgreeOnRandomModels<storm::models::sparse::Ctmc<ValueType>>(60, 200);
}

}  // namespace
