#include "BisimulationTestHelper.h"

#include <cstdint>
#include <random>

#include "storm/adapters/RationalNumberAdapter.h"

namespace {

using storm::test::bisimulation::buildFromPrism;
using storm::test::bisimulation::buildModel;
using storm::test::bisimulation::checkFormula;
using storm::test::bisimulation::Options;
using storm::test::bisimulation::strongOptions;

using ValueType = double;

/*!
 * Checks the number of states, transitions and choices of the bisimulation quotient of the model built from the given PRISM file, plus that the quotient
 * preserves the value of the formula.
 *
 * If `options.preserveAllStateLabels` is set, the full state space is built with all labels of the program. Otherwise, the formula may restrict the
 * exploration, e.g. by making the target states of a reachability formula absorbing.
 */
void testQuotient(std::string const& prismFile, std::string const& formulaString, uint64_t expectedModelStates, uint64_t expectedStates,
                  uint64_t expectedTransitions, uint64_t expectedChoices, Options const options = strongOptions()) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto const input = buildFromPrism<ValueType>(prismFile, formulaString, options.preserveAllStateLabels.value_or(false));
    ASSERT_EQ(expectedModelStates, input.model->getNumberOfStates());

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*input.model, input.formulas, options).quotient;
    EXPECT_EQ(input.model->getType(), quotient->getType());
    EXPECT_EQ(expectedStates, quotient->getNumberOfStates());
    EXPECT_EQ(expectedTransitions, quotient->getNumberOfTransitions());
    EXPECT_EQ(expectedChoices, quotient->getNumberOfChoices());

    EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), checkFormula<ValueType>(input.model, formulaString), 1e-9);
}

/*!
 * @return options that make the quotient preserve every state label of the model.
 */
Options allLabelOptions() {
    Options options = strongOptions();
    options.preserveAllStateLabels = true;
    return options;
}

/*!
 * @return options with the tolerance that the command line uses for inexact value types. Besides grouping almost-equal values, a positive tolerance also
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

TEST(StrongBisimulationTest, Crowds) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 7403ull, 65ull, 105ull, 65ull);
}

TEST(StrongBisimulationTest, CrowdsAllLabels) {
    // The model is larger than above because the target states of the formula are no longer made absorbing.
    testQuotient(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 8607ull, 2149ull, 3912ull, 2149ull, allLabelOptions());
}

TEST(StrongBisimulationTest, CtmcEmbedded) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 2076ull, 634ull, 3576ull, 634ull);
}

TEST(StrongBisimulationTest, CtmcCluster) {
    testQuotient(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", "P=? [F<=100 !\"minimum\"]", 276ull, 147ull, 569ull, 147ull);
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
 * rounding alone then decides some of the splits. In exact arithmetic that effect is gone, cf. BisimulationIssueTest.Issue833EmbeddedCtmcQuotient.
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
