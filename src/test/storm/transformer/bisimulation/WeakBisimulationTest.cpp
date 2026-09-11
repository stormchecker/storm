#include "BisimulationTestHelper.h"

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

using storm::test::bisimulation::buildFromPrism;
using storm::test::bisimulation::buildModel;
using storm::test::bisimulation::BuildOptions;
using storm::test::bisimulation::checkFormula;
using storm::test::bisimulation::Options;
using storm::test::bisimulation::strongOptions;
using storm::test::bisimulation::weakOptions;

using ValueType = double;

// ------------------------------------------------------------
// Silent states and divergence
// ------------------------------------------------------------

/*!
 * A chain of silent states (0 -> 1 -> 2) that eventually branches into two absorbing states, one of which is labeled "goal".
 * Weak bisimulation collapses the whole chain, strong bisimulation cannot.
 */
TEST(WeakBisimulationTest, SilentChain) {
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5);
    builder.addNextValue(0, 1, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 3, 0.5);
    builder.addNextValue(2, 4, 0.5);
    builder.addNextValue(3, 3, 1.0);
    builder.addNextValue(4, 4, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {3}}});

    auto const strongQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(5ull, strongQuotient->getNumberOfStates());

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    // {0,1,2}, {3} and {4}. The two absorbing blocks are divergent, so they keep their self-loop; the chain moves on with probability 1/2 each.
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    EXPECT_EQ(4ull, quotient->getNumberOfTransitions());
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F \"goal\"]"), 0.5, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 0.5, 1e-12);
}

/*!
 * Two silent states (0 and 1) that reach different sets of non-silent states, which in turn have different conditional distributions. The silent states
 * therefore must not be merged, even though neither of them has a transition leaving the block.
 */
TEST(WeakBisimulationTest, SilentStatesReachingDifferentClasses) {
    // 0 -> {2, 3}, 1 -> 2, 2 -> {4, 5}, 3 -> 5, and 4, 5 are absorbing and labeled.
    storm::storage::SparseMatrixBuilder<ValueType> builder(6, 6);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(0, 3, 0.5);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 4, 0.5);
    builder.addNextValue(2, 5, 0.5);
    builder.addNextValue(3, 5, 1.0);
    builder.addNextValue(4, 4, 1.0);
    builder.addNextValue(5, 5, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"c", {4}}, {"d", {5}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    // States 1 and 2 both reach "c" with probability 1/2, state 0 with probability 1/4 and state 3 with probability 0.
    EXPECT_EQ(5ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F \"c\"]"), 0.25, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"c\"]"), 0.25, 1e-12);
}

/*!
 * A weak bisimulation class whose entire escape mass goes into *other* classes of the same block: every one of its states is silent with respect to the block
 * although none of them is silent with respect to the class. 0 and 1 are weakly bisimilar (both eventually reach "g" and "h" with probability 1/2 each), but
 * 1 reaches the non-silent states 2 and 3 only through 0.
 */
TEST(WeakBisimulationTest, ClassSilentOnlyWithinBlock) {
    storm::storage::SparseMatrixBuilder<ValueType> builder(6, 6);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(0, 3, 0.5);
    builder.addNextValue(1, 0, 1.0);
    builder.addNextValue(2, 4, 1.0);
    builder.addNextValue(3, 5, 1.0);
    builder.addNextValue(4, 4, 1.0);
    builder.addNextValue(5, 5, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"g", {4}}, {"h", {5}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(5ull, quotient->getNumberOfStates());  // {0,1}, {2}, {3}, {4}, {5}
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F \"g\"]"), 0.5, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"g\"]"), 0.5, 1e-12);
}

/*!
 * A block whose states can never leave it. Those states are all weakly bisimilar and the quotient state is absorbing.
 */
TEST(WeakBisimulationTest, DivergentStates) {
    // 0 -> {1, 3}; 1 <-> 2 forever; 3 absorbing and labeled "goal".
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 4);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 3, 0.5);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 1, 1.0);
    builder.addNextValue(3, 3, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {3}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    // {0}, {1,2} and {3}: states 1 and 2 are divergent, state 0 is not (it can leave the block).
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F \"goal\"]"), 0.5, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 0.5, 1e-12);
}

/*!
 * A positive tolerance must not group a state that can move to a block with a state that cannot, no matter how small the probability is.
 */
TEST(WeakBisimulationTest, ZeroProbabilityIsNeverGroupedWithNonZeroProbability) {
    // 0 moves to 1 and 2; 1 reaches "goal" (3) with a tiny probability and "sink" (4) otherwise; 2 only moves to "sink".
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(1, 3, 1e-10);
    builder.addNextValue(1, 4, 1.0 - 1e-10);
    builder.addNextValue(2, 4, 1.0);
    builder.addNextValue(3, 3, 1.0);
    builder.addNextValue(4, 4, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {3}}, {"sink", {4}}});

    Options options = weakOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-9);
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
    EXPECT_EQ(5ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]") / 5e-11, 1.0, 1e-6);
}

// ------------------------------------------------------------
// Continuous-time models
// ------------------------------------------------------------

/*!
 * On a CTMC, the rate into the own block is unobservable: by memorylessness it does not change the distribution of the time until the block is left.
 */
TEST(WeakBisimulationTest, CtmcInternalRate) {
    // 0 -> 1 with rate 100 and 0 -> 2 with rate 1; 1 -> 2 with rate 1; 2 absorbing (self-loop) and labeled "goal".
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 100.0);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}});

    auto const strongQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, strongOptions()).quotient;
    EXPECT_EQ(3ull, strongQuotient->getNumberOfStates());

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(2ull, quotient->getNumberOfStates());
    // Both states of the merged block reach "goal" after an Exp(1)-distributed amount of time.
    ValueType const expected = 1.0 - std::exp(-1.0);
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F<=1 \"goal\"]"), expected, 1e-6);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F<=1 \"goal\"]"), expected, 1e-6);
}

/*!
 * A silent state without a transition into the splitter loses its silence because its successor ends up in the other sub-block. In contrast to the
 * discrete-time case this needs no rewards, since every block of a CTMC is refined with respect to strong bisimulation.
 */
TEST(WeakBisimulationTest, CtmcSplitChangesSilence) {
    // 0 -> 1 (silent, not a predecessor of the goal); 1 -> 2 (non-silent predecessor); 2 is the absorbing goal.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    ValueType const expected = checkFormula<ValueType>(model, "P=? [F<=1 \"goal\"]");
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F<=1 \"goal\"]"), expected, 1e-9);
}

/*!
 * The state rewards of a CTMC are rate rewards. Weak bisimulation preserves the distribution of the time spent within a block, so it preserves the
 * accumulated reward as well and does not have to treat reward states specially.
 */
TEST(WeakBisimulationTest, CtmcRateRewards) {
    // Same model as CtmcInternalRate, but states 0 and 1 earn reward at rate one until "goal" is reached.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 100.0);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}}, {1.0, 1.0, 0.0});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    // The two states are merged despite their different internal rates, and the reward does not prevent that.
    EXPECT_EQ(2ull, quotient->getNumberOfStates());
    // "goal" is reached after an Exp(1)-distributed amount of time, during which reward accumulates at rate one.
    EXPECT_NEAR(checkFormula<ValueType>(model, "R{\"rew\"}=? [F \"goal\"]"), 1.0, 1e-9);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "R{\"rew\"}=? [F \"goal\"]"), 1.0, 1e-9);
}

/*!
 * Weak bisimulation on a CTMC preserves the distribution of the time spent within a block, and thus expected times.
 */
TEST(WeakBisimulationTest, CtmcExpectedTime) {
    // 0 -> 1 with rate 100 and 0 -> 2 with rate 1; 1 -> 2 with rate 1; 2 absorbing and labeled "goal". Both 0 and 1 leave {0, 1} with rate 1.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 100.0);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Ctmc<ValueType>>(builder.build(), {{"goal", {2}}});

    storm::parser::FormulaParser formulaParser;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas{formulaParser.parseSingleFormulaFromString("T=? [F \"goal\"]")};
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, weakOptions()).quotient;
    EXPECT_EQ(2ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(model, "T=? [F \"goal\"]"), 1.0, 1e-9);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "T=? [F \"goal\"]"), 1.0, 1e-9);
}

// ------------------------------------------------------------
// Rewards and step sensitive states
// ------------------------------------------------------------

/*!
 * Two states with the same conditional distribution but different probabilities of staying in their own block. Without rewards they are weakly bisimilar;
 * with rewards they are not, because they accumulate a different amount of reward before leaving the block.
 */
TEST(WeakBisimulationTest, Rewards) {
    // 0 -> {0, 2}, 1 -> 2, 2 absorbing and labeled "goal". States 0 and 1 have reward 1.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 0, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {2}}}, {1.0, 1.0, 0.0});

    Options optionsWithoutRewards = weakOptions();
    optionsWithoutRewards.preserveAllRewards = false;
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, optionsWithoutRewards).quotient;
    EXPECT_EQ(2ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 1.0, 1e-12);

    auto const rewardQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(3ull, rewardQuotient->getNumberOfStates());
    // From state 0 we expect two steps (each earning reward 1) until "goal" is reached.
    EXPECT_NEAR(checkFormula<ValueType>(model, "R{\"rew\"}=? [F \"goal\"]"), 2.0, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(rewardQuotient, "R{\"rew\"}=? [F \"goal\"]"), 2.0, 1e-12);
}

/*!
 * The states with a non-zero state-action reward have to be treated as step sensitive as well, just like those with a non-zero state reward.
 */
TEST(WeakBisimulationTest, StateActionRewards) {
    // 0 -> {0, 2}, 1 -> 2, 2 absorbing and labeled "goal". Taking a transition out of 0 or 1 earns a reward of one.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 0, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {2}}}, {}, {1.0, 1.0, 0.0});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    // From state 0 we expect two transitions (each earning reward one) until "goal" is reached.
    EXPECT_NEAR(checkFormula<ValueType>(model, "R{\"rew\"}=? [F \"goal\"]"), 2.0, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "R{\"rew\"}=? [F \"goal\"]"), 2.0, 1e-12);
}

/*!
 * A positive tolerance must not group a zero reward with a non-zero one, however small the latter is: whether a state carries a reward decides whether weak
 * bisimulation may stutter over it at all.
 */
TEST(WeakBisimulationTest, ZeroRewardIsNeverGroupedWithNonZeroReward) {
    // States 0 and 1 behave identically and differ only in their reward, which is zero for 0 and tiny but non-zero for 1.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 2, 1.0);
    builder.addNextValue(1, 2, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {2}}}, {0.0, 1e-9, 0.0});

    Options options = weakOptions();
    options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-6);
    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());

    // Without any reward to preserve, the tolerance does merge the two states.
    options.preserveAllRewards = false;
    auto const rewardlessQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
    EXPECT_EQ(2ull, rewardlessQuotient->getNumberOfStates());
}

/*!
 * A state can be divergent and step sensitive at the same time. Divergence takes precedence: the states of such a block can never leave it, so they behave
 * alike however much reward they accumulate, and the block must not be refined with respect to strong bisimulation.
 */
TEST(WeakBisimulationTest, DivergentStepSensitiveBlock) {
    // 0 -> {1, 3}; 1 and 2 carry a reward and can only ever reach each other; 3 is the absorbing goal.
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 4);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 3, 0.5);
    builder.addNextValue(1, 1, 0.5);
    builder.addNextValue(1, 2, 0.5);
    builder.addNextValue(2, 2, 1.0);
    builder.addNextValue(3, 3, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"d", {1, 2}}, {"goal", {3}}}, {0.0, 1.0, 1.0, 0.0});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());  // {0}, {1,2} and {3}
    EXPECT_NEAR(checkFormula<ValueType>(model, "P=? [F \"goal\"]"), 0.5, 1e-12);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"goal\"]"), 0.5, 1e-12);
    // "goal" is not reached almost surely, so the expected reward is infinite either way.
    EXPECT_TRUE(std::isinf(checkFormula<ValueType>(model, "R{\"rew\"}=? [F \"goal\"]")));
    EXPECT_TRUE(std::isinf(checkFormula<ValueType>(quotient, "R{\"rew\"}=? [F \"goal\"]")));
}

/*!
 * A step sensitive block is refined with respect to strong bisimulation, which can change the silence of its states: a state without a transition into the
 * splitter can lose its silence because one of its successors ended up in the other sub-block.
 */
TEST(WeakBisimulationTest, StepSensitiveSplitChangesSilence) {
    // 0 -> 1 (silent, not a predecessor of the goal); 1 -> {1, 2} (non-silent predecessor). Both carry a reward, so they are refined strongly.
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3);
    builder.addNextValue(0, 1, 1.0);
    builder.addNextValue(1, 1, 0.5);
    builder.addNextValue(1, 2, 0.5);
    builder.addNextValue(2, 2, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"goal", {2}}}, {1.0, 1.0, 0.0});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    EXPECT_EQ(3ull, quotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(model, "R{\"rew\"}=? [F \"goal\"]"), 3.0, 1e-9);
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "R{\"rew\"}=? [F \"goal\"]"), 3.0, 1e-9);
}

/*!
 * The block C={3,4} carries a reward and is its own predecessor, so it is not skipped when it acts as its own splitter and gets split during that very round.
 * A predecessor block that is refined weakly afterwards must still recognize the (by then already split) splitter, which is why the membership test asks
 * whether a successor is among the states the splitter had at the start of the round (Partition::contains) rather than comparing the blocks.
 */
TEST(WeakBisimulationTest, SplitterSplitDuringItsOwnRound) {
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 9);
    for (uint64_t b = 0; b < 3; ++b) {
        builder.addNextValue(b, 4, 0.5);
        builder.addNextValue(b, 8, 0.5);
    }
    builder.addNextValue(3, 3, 0.5);
    builder.addNextValue(3, 5, 0.5);
    builder.addNextValue(4, 4, 0.25);
    builder.addNextValue(4, 5, 0.75);
    for (uint64_t e = 5; e < 8; ++e) {
        builder.addNextValue(e, 8, 1.0);
    }
    builder.addNextValue(8, 8, 1.0);
    auto const model = buildModel<storm::models::sparse::Dtmc<ValueType>>(builder.build(), {{"b", {0, 1, 2}}, {"c", {3, 4}}, {"e", {5, 6, 7}}, {"t", {8}}},
                                                                          {0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0});

    auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, weakOptions()).quotient;
    auto const requotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*quotient, {}, weakOptions()).quotient;
    EXPECT_EQ(quotient->getNumberOfStates(), requotient->getNumberOfStates());
    EXPECT_NEAR(checkFormula<ValueType>(quotient, "P=? [F \"t\"]"), checkFormula<ValueType>(model, "P=? [F \"t\"]"), 1e-9);
}

// ------------------------------------------------------------
// Randomized self-checks
// ------------------------------------------------------------

/*!
 * Builds a pseudo-random deterministic model in which every state has between one and three successors and carries a random subset of the labels "a" and
 * "b". For a DTMC the outgoing probabilities are normalized; for a CTMC they are used as rates.
 */
template<typename ModelType>
std::shared_ptr<ModelType> buildRandomModel(uint64_t const numStates, uint64_t const seed) {
    std::mt19937_64 rng(seed);
    storm::storage::SparseMatrixBuilder<ValueType> builder(numStates, numStates);
    for (uint64_t state = 0; state < numStates; ++state) {
        uint64_t const numSuccessors = 1 + rng() % 3;
        std::map<uint64_t, ValueType> row;
        for (uint64_t i = 0; i < numSuccessors; ++i) {
            // Small values keep the rows short and make coinciding distributions (and thus actual merges) reasonably likely.
            row[rng() % numStates] += static_cast<ValueType>(1 + rng() % 4);
        }
        if constexpr (std::is_same_v<ModelType, storm::models::sparse::Dtmc<ValueType>>) {
            ValueType sum = storm::utility::zero<ValueType>();
            for (auto const& [_, value] : row) {
                sum += value;
            }
            for (auto& [_, value] : row) {
                value /= sum;
            }
        }
        for (auto const& [column, value] : row) {
            builder.addNextValue(state, column, value);
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
    return buildModel<ModelType>(builder.build(), labels);
}

/*!
 * Minimizing an already minimal model must not change it any further. This is a strong self-check: if the refinement split a block that it should not have,
 * the second round has a chance to merge it again, and if it stopped too early, so does the first.
 *
 * We deliberately run this with a positive tolerance, which is also what the command line uses for inexact value types. With tolerance zero the conditional
 * probabilities are compared exactly, and since they are quotients of accumulated sums, rounding alone can make the refinement split a block further than
 * necessary - see the note on that in refineBlockWeak. The check has been run over 8000 models per model type without a failure.
 */
template<typename ModelType>
void testIdempotence(uint64_t const numStates, uint64_t const numSeeds, std::string const& formulaString) {
    for (uint64_t seed = 0; seed < numSeeds; ++seed) {
        auto const model = buildRandomModel<ModelType>(numStates, seed);
        Options options = weakOptions();
        options.tolerance = storm::utility::convertNumber<storm::RationalNumber>(1e-6);
        auto const quotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, {}, options).quotient;
        ASSERT_LE(quotient->getNumberOfStates(), model->getNumberOfStates()) << "Seed " << seed << ".";
        auto const quotientOfQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*quotient, {}, options).quotient;
        EXPECT_EQ(quotient->getNumberOfStates(), quotientOfQuotient->getNumberOfStates()) << "Weak bisimulation is not idempotent for seed " << seed << ".";
        EXPECT_EQ(quotient->getNumberOfTransitions(), quotientOfQuotient->getNumberOfTransitions())
            << "Weak bisimulation is not idempotent for seed " << seed << ".";
        // The comparison is against the default precision of the underlying equation solver, not against the exactness of the quotient.
        EXPECT_NEAR(checkFormula<ValueType>(quotient, formulaString), checkFormula<ValueType>(model, formulaString), 1e-6) << "Seed " << seed << ".";
    }
}

TEST(WeakBisimulationTest, RandomDtmcs) {
    testIdempotence<storm::models::sparse::Dtmc<ValueType>>(12, 500, "P=? [F \"a\"]");
    testIdempotence<storm::models::sparse::Dtmc<ValueType>>(60, 200, "P=? [(!\"b\") U \"a\"]");
}

TEST(WeakBisimulationTest, RandomCtmcs) {
    testIdempotence<storm::models::sparse::Ctmc<ValueType>>(12, 500, "P=? [F \"a\"]");
    testIdempotence<storm::models::sparse::Ctmc<ValueType>>(60, 200, "P=? [(!\"b\") U \"a\"]");
}

// ------------------------------------------------------------
// Benchmark models
// ------------------------------------------------------------

/*!
 * Checks on a real model that the weak quotient is at most as large as the strong one and that both preserve the value of the given formula.
 *
 * If `buildOptions.allLabels` is set, the full state space is built with all labels of the program and the quotient has to preserve all of them. Otherwise, the
 * formula may restrict the exploration, e.g. by making the target states of a reachability formula absorbing, and only the labels occurring in the formula
 * are preserved.
 */
void testAgainstOriginal(std::string const& prismFile, std::string const& formulaString, uint64_t expectedModelStates, uint64_t expectedStrongStates,
                         uint64_t expectedWeakStates, uint64_t expectedWeakTransitions, BuildOptions const& buildOptions = {}, double const tolerance = 0.0) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto const [model, formulas] = buildFromPrism<ValueType>(prismFile, formulaString, buildOptions);
    ASSERT_EQ(expectedModelStates, model->getNumberOfStates());

    auto strong = strongOptions();
    auto weak = weakOptions();
    strong.tolerance = storm::utility::convertNumber<storm::RationalNumber>(tolerance);
    weak.tolerance = strong.tolerance;
    if (buildOptions.allLabels) {
        strong.preserveAllStateLabels = true;
        weak.preserveAllStateLabels = true;
    }

    auto const strongQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, strong).quotient;
    EXPECT_EQ(model->getType(), strongQuotient->getType());
    EXPECT_EQ(expectedStrongStates, strongQuotient->getNumberOfStates());
    auto const weakQuotient = storm::bisimulation::performBisimulationMinimization<ValueType>(*model, formulas, weak).quotient;
    EXPECT_EQ(model->getType(), weakQuotient->getType());
    EXPECT_EQ(expectedWeakStates, weakQuotient->getNumberOfStates());
    EXPECT_EQ(expectedWeakTransitions, weakQuotient->getNumberOfTransitions());

    ValueType const expected = checkFormula<ValueType>(model, formulaString);
    EXPECT_NEAR(checkFormula<ValueType>(strongQuotient, formulaString), expected, 1e-9);
    EXPECT_NEAR(checkFormula<ValueType>(weakQuotient, formulaString), expected, 1e-9);
}

TEST(WeakBisimulationTest, Die) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]", 13ull, 5ull, 5ull, 8ull);
}

TEST(WeakBisimulationTest, DieAllLabels) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [F \"one\"]", 13ull, 11ull, 9ull, 13ull, {.allLabels = true});
}

TEST(WeakBisimulationTest, Crowds) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 7403ull, 65ull, 43ull, 83ull);
}

TEST(WeakBisimulationTest, CrowdsAllLabels) {
    // The model is larger than above because the target states of the formula are no longer made absorbing.
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm", "P=? [F \"observe0Greater1\"]", 8607ull, 2149ull, 1556ull, 3287ull, {.allLabels = true});
}

/*!
 * A CTMC, checked against a time-bounded formula (which weak bisimulation does preserve on continuous-time models).
 * The rates of this model are not exactly representable as doubles. With tolerance zero, the strong quotient therefore depends on how the platform rounds,
 * so we use the tolerance of the command line, with which the strong quotient coincides with the exact one.
 */
TEST(WeakBisimulationTest, CtmcEmbedded) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 2076ull, 310ull, 158ull, 898ull, {}, 1e-9);
}

TEST(WeakBisimulationTest, CtmcEmbeddedAllLabels) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "P=? [F<=10000 \"down\"]", 3478ull, 1127ull, 659ull, 3392ull, {.allLabels = true}, 1e-9);
}

/*!
 * The choice labels and choice origins of a PRISM program are preserved, too. For cluster2 this rules out any reduction, since the actions of the left and the
 * right cluster have different names and stem from different modules.
 */
TEST(WeakBisimulationTest, CtmcClusterChoiceLabelsAndOrigins) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", "P=? [F<=100 !\"minimum\"]", 276ull, 276ull, 276ull, 1120ull,
                        {.choiceLabels = true, .choiceOrigins = true});
}

TEST(WeakBisimulationTest, DtmcBrpChoiceLabelsAndOrigins) {
    testAgainstOriginal(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [F \"target\"]", 613ull, 412ull, 412ull, 572ull,
                        {.choiceLabels = true, .choiceOrigins = true});
}
}  // namespace
