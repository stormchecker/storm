#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/generator/StateBehavior.h"

TEST(StateBehaviorTest, StartNewChoiceAndClear) {
    storm::generator::StateBehavior<double> behavior;
    EXPECT_TRUE(behavior.empty());
    EXPECT_FALSE(behavior.wasExpanded());

    auto& c0 = behavior.startNewChoice(3, true);
    c0.addProbability(1, 0.25);
    c0.addProbability(2, 0.75);
    c0.getRewards().push_back(5.0);
    auto& c1 = behavior.startNewChoice(4);
    c1.addProbability(0, 1.0);
    behavior.setExpanded();
    behavior.addStateReward(2.0);

    ASSERT_EQ(2u, behavior.getNumberOfChoices());
    ASSERT_EQ(2u, behavior.getChoices().size());
    EXPECT_EQ(3u, behavior.getChoices()[0].getActionIndex());
    EXPECT_TRUE(behavior.getChoices()[0].isMarkovian());
    EXPECT_EQ(2u, behavior.getChoices()[0].size());
    EXPECT_EQ(4u, behavior.getChoices()[1].getActionIndex());
    EXPECT_EQ(2, std::distance(behavior.begin(), behavior.end()));

    behavior.clear();
    EXPECT_TRUE(behavior.empty());
    EXPECT_FALSE(behavior.wasExpanded());
    EXPECT_EQ(0u, behavior.getNumberOfChoices());
    EXPECT_EQ(0, std::distance(behavior.begin(), behavior.end()));
    EXPECT_TRUE(behavior.getStateRewards().empty());

    // The reused choice must be completely reset
    auto& reused = behavior.startNewChoice(7);
    EXPECT_EQ(7u, reused.getActionIndex());
    EXPECT_FALSE(reused.isMarkovian());
    EXPECT_EQ(0u, reused.size());
    EXPECT_TRUE(reused.getRewards().empty());
    EXPECT_FALSE(reused.hasLabels());
    EXPECT_FALSE(reused.hasOriginData());
    EXPECT_DOUBLE_EQ(0.0, reused.getTotalMass());
    EXPECT_EQ(1u, behavior.getNumberOfChoices());
}

TEST(StateBehaviorTest, AddChoiceAndRemove) {
    storm::generator::StateBehavior<double> behavior;
    for (uint64_t i = 0; i < 4; ++i) {
        storm::generator::Choice<double> choice(i);
        choice.addProbability(i, 1.0);
        behavior.addChoice(std::move(choice));
    }
    ASSERT_EQ(4u, behavior.getNumberOfChoices());
    behavior.removeLastChoices(2);
    ASSERT_EQ(2u, behavior.getNumberOfChoices());
    EXPECT_EQ(1u, behavior.getChoices().back().getActionIndex());
    // Adding after removal reuses the slot
    storm::generator::Choice<double> choice(9);
    behavior.addChoice(std::move(choice));
    ASSERT_EQ(3u, behavior.getNumberOfChoices());
    EXPECT_EQ(9u, behavior.getChoices().back().getActionIndex());
    behavior.clearChoices();
    EXPECT_TRUE(behavior.empty());
}

TEST(StateBehaviorTest, CopyOnlyContainsActiveChoices) {
    storm::generator::StateBehavior<double> behavior;
    behavior.startNewChoice(1).addProbability(0, 1.0);
    behavior.startNewChoice(2).addProbability(1, 1.0);
    behavior.startNewChoice(3).addProbability(2, 1.0);
    behavior.addStateReward(1.5);
    behavior.setExpanded();
    behavior.removeLastChoices(2);  // Only the first choice is active

    storm::generator::StateBehavior<double> copy(behavior);
    EXPECT_EQ(1u, copy.getNumberOfChoices());
    EXPECT_EQ(1, std::distance(copy.begin(), copy.end()));
    EXPECT_TRUE(copy.wasExpanded());
    ASSERT_EQ(1u, copy.getStateRewards().size());

    storm::generator::StateBehavior<double> assigned;
    assigned.startNewChoice(5);
    assigned.startNewChoice(6);
    assigned = behavior;
    EXPECT_EQ(1u, assigned.getNumberOfChoices());
    EXPECT_EQ(1u, assigned.getChoices()[0].getActionIndex());
}

TEST(StateBehaviorTest, Move) {
    storm::generator::StateBehavior<double> behavior;
    behavior.startNewChoice(1).addProbability(0, 1.0);
    behavior.startNewChoice(2).addProbability(1, 1.0);
    behavior.addStateReward(1.5);
    behavior.setExpanded();

    storm::generator::StateBehavior<double> moved(std::move(behavior));
    EXPECT_EQ(2u, moved.getNumberOfChoices());
    EXPECT_EQ(2u, moved.getChoices().size());
    EXPECT_EQ(2, std::distance(moved.begin(), moved.end()));
    EXPECT_TRUE(moved.wasExpanded());
    EXPECT_EQ(1u, moved.getStateRewards().size());

    // The source of the move is a valid empty behavior that can be reused
    // NOLINTBEGIN(bugprone-use-after-move): We explicitly test the state of moved-from objects
    EXPECT_TRUE(behavior.empty());
    EXPECT_EQ(0u, behavior.getNumberOfChoices());
    EXPECT_EQ(0u, behavior.getChoices().size());
    EXPECT_EQ(0, std::distance(behavior.begin(), behavior.end()));
    EXPECT_FALSE(behavior.wasExpanded());
    EXPECT_TRUE(behavior.getStateRewards().empty());
    behavior.startNewChoice(5);
    EXPECT_EQ(1u, behavior.getNumberOfChoices());
    // NOLINTEND(bugprone-use-after-move)

    storm::generator::StateBehavior<double> assigned;
    assigned.startNewChoice(9);
    assigned = std::move(moved);
    EXPECT_EQ(2u, assigned.getNumberOfChoices());
    EXPECT_EQ(2u, assigned.getChoices()[1].getActionIndex());
    // NOLINTBEGIN(bugprone-use-after-move): We explicitly test the state of moved-from objects
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(0, std::distance(moved.begin(), moved.end()));
    // NOLINTEND(bugprone-use-after-move)
}

TEST(StateBehaviorTest, AddChoicesSumsAllRewards) {
    storm::generator::Choice<double> a(0, true);
    a.addProbability(0, 1.0);
    a.getRewards() = {1.0, 10.0, 100.0};
    storm::generator::Choice<double> b(0, true);
    b.addProbability(1, 2.0);
    b.getRewards() = {2.0, 20.0, 200.0};
    a.add(b);
    ASSERT_EQ(3u, a.getRewards().size());
    EXPECT_DOUBLE_EQ(3.0, a.getRewards()[0]);
    EXPECT_DOUBLE_EQ(30.0, a.getRewards()[1]);
    EXPECT_DOUBLE_EQ(300.0, a.getRewards()[2]);
    EXPECT_DOUBLE_EQ(3.0, a.getTotalMass());
}

TEST(StateBehaviorTest, CopyAssignmentReusesAndGrows) {
    storm::generator::StateBehavior<double> source;
    for (uint64_t i = 0; i < 3; ++i) {
        auto& choice = source.startNewChoice(i + 1, i == 1);
        choice.addProbability(i, 0.5);
        choice.addProbability(i + 10, 0.5);
        choice.getRewards() = {static_cast<double>(i)};
    }
    source.addStateReward(4.0);
    source.setExpanded();

    // Target with fewer choices than the source (the choices vector has to grow)
    storm::generator::StateBehavior<double> small;
    small.startNewChoice(42).addProbability(7, 1.0);
    small = source;
    ASSERT_EQ(3u, small.getNumberOfChoices());
    for (uint64_t i = 0; i < 3; ++i) {
        EXPECT_EQ(i + 1, small.getChoices()[i].getActionIndex());
        EXPECT_EQ(i == 1, small.getChoices()[i].isMarkovian());
        EXPECT_EQ(2u, small.getChoices()[i].size());
        EXPECT_DOUBLE_EQ(static_cast<double>(i), small.getChoices()[i].getRewards().at(0));
    }
    EXPECT_TRUE(small.wasExpanded());
    ASSERT_EQ(1u, small.getStateRewards().size());

    // Target with more (stale) choices than the source: only the active ones are visible afterwards
    storm::generator::StateBehavior<double> large;
    for (uint64_t i = 0; i < 5; ++i) {
        large.startNewChoice(100 + i).addProbability(i, 1.0);
    }
    storm::generator::StateBehavior<double> twoChoices;
    twoChoices.startNewChoice(1).addProbability(0, 1.0);
    twoChoices.startNewChoice(2).addProbability(1, 1.0);
    large = twoChoices;
    ASSERT_EQ(2u, large.getNumberOfChoices());
    EXPECT_EQ(2, std::distance(large.begin(), large.end()));
    EXPECT_EQ(1u, large.getChoices()[0].getActionIndex());
    EXPECT_EQ(2u, large.getChoices()[1].getActionIndex());
    EXPECT_EQ(1u, large.getChoices()[1].size());
    EXPECT_FALSE(large.wasExpanded());
}
