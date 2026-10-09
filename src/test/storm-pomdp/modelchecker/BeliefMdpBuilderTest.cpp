#include "test/storm_gtest.h"

#include <type_traits>

#include "storm-pomdp/beliefs/exploration/BeliefMdpBuilder.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm/adapters/RationalNumberAdapter.h"

namespace {
using namespace storm::pomdp::beliefs;

template<typename ValueType>
class BeliefMdpBuilderTest : public ::testing::Test {
   public:
    using BeliefType = Belief<double>;
    using StandardInfo = StandardExplorationInformation<ValueType, BeliefType>;
    using RewardInfo = RewardAwareExplorationInformation<ValueType, BeliefType>;
    using ClippingInfo = ClippingExplorationInformation<ValueType, BeliefType>;

    static ValueType value(double number) {
        return storm::utility::convertNumber<ValueType>(number);
    }

    template<typename Info>
    static Info makeInfo(uint64_t beliefs) {
        Info info;
        info.initialBeliefId = 1;
        info.nrObservationsInPomdp = beliefs;
        for (uint64_t i = 0; i < beliefs; ++i) {
            BeliefBuilder<BeliefType> builder;
            builder.setObservation(i);
            builder.addValue(i, 1.0);
            info.discoveredBeliefs.addBelief(builder.build());
        }
        return info;
    }

    static PropertyInformation property(PropertyInformation::Kind kind = PropertyInformation::Kind::ReachabilityProbability) {
        return {kind, {}, std::nullopt, storm::OptimizationDirection::Maximize, {}};
    }

    template<typename Info>
    static auto build(Info const& info, PropertyInformation const& propertyInfo = property()) {
        std::function<std::unordered_map<std::string, ValueType>(BeliefType const&)> cutOff = [](BeliefType const&) {
            return std::unordered_map<std::string, ValueType>{{"cutoff", value(0.75)}};
        };
        return buildBeliefMdp(info, propertyInfo, cutOff);
    }

    static void checkRow(storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t row, std::initializer_list<std::pair<uint64_t, ValueType>> expected) {
        auto const actual = matrix.getRow(row);
        ASSERT_EQ(expected.size(), actual.getNumberOfEntries());
        auto it = actual.begin();
        for (auto const& [column, probability] : expected) {
            EXPECT_EQ(column, it->getColumn());
            if constexpr (std::is_same_v<ValueType, double>) {
                EXPECT_NEAR(probability, it->getValue(), 1e-14);
            } else {
                EXPECT_EQ(probability, it->getValue());
            }
            ++it;
        }
    }
};

using MatrixValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(BeliefMdpBuilderTest, MatrixValueTypes, );

TYPED_TEST(BeliefMdpBuilderTest, MapsSuccessorsInEveryOrderAndPreservesZeros) {
    auto info = TestFixture::template makeInfo<typename TestFixture::StandardInfo>(3);
    info.exploredBeliefs = {{0, 2}, {1, 0}, {2, 1}};
    // These belief IDs map to ascending, descending, and mixed state IDs respectively.
    for (auto const& order : {std::vector<BeliefId>{1, 2, 0}, std::vector<BeliefId>{0, 2, 1}, std::vector<BeliefId>{2, 0, 1}}) {
        for (auto id : order) {
            info.matrix.transitions.push_back({TestFixture::value(id == 2 ? 0.0 : 0.5), id});
        }
        info.matrix.endCurrentRow();
    }
    info.matrix.endCurrentRowGroup();
    info.matrix.transitions.push_back({TestFixture::value(1), 2});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.matrix.transitions.push_back({TestFixture::value(1), 0});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();

    auto const [mdp, mapping] = TestFixture::build(info);
    auto const& matrix = mdp->getTransitionMatrix();
    for (uint64_t row = 0; row < 3; ++row) {
        TestFixture::checkRow(matrix, row, {{0, TestFixture::value(0.5)}, {1, TestFixture::value(0)}, {2, TestFixture::value(0.5)}});
    }
    TestFixture::checkRow(matrix, 3, {{1, TestFixture::value(1)}});
    TestFixture::checkRow(matrix, 4, {{2, TestFixture::value(1)}});
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 4, 5, 6, 7}), matrix.getRowGroupIndices());
    EXPECT_EQ((std::unordered_map<uint64_t, BeliefId>{{0, 1}, {1, 2}, {2, 0}}), mapping);
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("init", 0));
}

TYPED_TEST(BeliefMdpBuilderTest, MergesDuplicatesAndPreservesFrontierTerminalAndLabels) {
    auto info = TestFixture::template makeInfo<typename TestFixture::StandardInfo>(4);
    info.exploredBeliefs = {{0, 1}, {1, 0}};
    info.terminalBeliefValues = {{3, TestFixture::value(0.5)}};
    info.matrix.transitions = {{TestFixture::value(0.25), 2},
                               {TestFixture::value(0.125), 0},
                               {TestFixture::value(0.25), 3},
                               {TestFixture::value(0.25), 1},
                               {TestFixture::value(0.125), 0}};
    info.matrix.endCurrentRow();
    // No ordinary successors: the scratch buffer from the previous choice must be cleared.
    info.matrix.transitions.push_back({TestFixture::value(1), 3});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.matrix.transitions.push_back({TestFixture::value(1), 0});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.matrix.choiceLabels = {{"mixed"}, {"sinks"}, {"self"}};

    auto const [mdp, mapping] = TestFixture::build(info);
    auto const& matrix = mdp->getTransitionMatrix();
    TestFixture::checkRow(matrix, 0,
                          {{0, TestFixture::value(0.25)},
                           {1, TestFixture::value(0.25)},
                           {2, TestFixture::value(0.25)},
                           {3, TestFixture::value(0.125)},
                           {4, TestFixture::value(0.125)}});
    TestFixture::checkRow(matrix, 1, {{3, TestFixture::value(0.5)}, {4, TestFixture::value(0.5)}});
    TestFixture::checkRow(matrix, 2, {{1, TestFixture::value(1)}});
    TestFixture::checkRow(matrix, 3, {{3, TestFixture::value(0.75)}, {4, TestFixture::value(0.25)}});
    TestFixture::checkRow(matrix, 4, {{3, TestFixture::value(1)}});
    TestFixture::checkRow(matrix, 5, {{4, TestFixture::value(1)}});
    EXPECT_EQ((std::vector<uint64_t>{0, 2, 3, 4, 5, 6}), matrix.getRowGroupIndices());
    EXPECT_EQ((std::unordered_map<uint64_t, BeliefId>{{0, 1}, {1, 0}, {2, 2}}), mapping);
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("truncated", 2));
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("target", 3));
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("bottom", 4));
    ASSERT_TRUE(mdp->hasChoiceLabeling());
    for (auto const& [choice, label] :
         std::vector<std::pair<uint64_t, std::string>>{{0, "mixed"}, {1, "sinks"}, {2, "self"}, {3, "cutoff"}, {4, "__loop__"}, {5, "__loop__"}}) {
        EXPECT_EQ((std::set<std::string>{label}), mdp->getChoiceLabeling().getLabelsOfChoice(choice));
    }
    // Construction must not reorder the exploration information itself.
    EXPECT_EQ(2, info.matrix.transitions.front().targetBelief);
}

TYPED_TEST(BeliefMdpBuilderTest, KeepsMultipleRewardDimensionsAttachedToSuccessors) {
    auto info = TestFixture::template makeInfo<typename TestFixture::RewardInfo>(4);
    info.exploredBeliefs = {{0, 1}, {1, 0}, {2, 2}};
    auto const n = TestFixture::value;
    info.matrix.transitions = {{n(0.25), 3, {n(5), n(0)}}, {n(0.25), 0, {n(0), n(3)}}, {n(0.25), 2, {n(2), n(7)}}, {n(0.25), 1, {n(11), n(0)}}};
    info.matrix.endCurrentRow();
    // Duplicate successors are valid when all their transition rewards are zero.
    info.matrix.transitions.push_back({n(0.5), 2, {n(0), n(0)}});
    info.matrix.transitions.push_back({n(0.25), 1, {n(0), n(0)}});
    info.matrix.transitions.push_back({n(0.25), 2, {n(0), n(0)}});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    for (auto id : {0ul, 2ul}) {
        info.matrix.transitions.push_back({n(1), id, {n(0), n(0)}});
        info.matrix.endCurrentRow();
        info.matrix.endCurrentRowGroup();
    }
    auto property = TestFixture::property(PropertyInformation::Kind::RewardBoundedReachabilityProbability);
    property.rewardBounds = {{"first", std::nullopt, std::nullopt}, {"second", std::nullopt, std::nullopt}};
    property.targetObservations = {2, 3};

    auto const [mdp, mapping] = TestFixture::build(info, property);
    auto const& probabilities = mdp->getTransitionMatrix();
    auto const& first = mdp->getRewardModel("first").getTransitionRewardMatrix();
    auto const& second = mdp->getRewardModel("second").getTransitionRewardMatrix();
    TestFixture::checkRow(probabilities, 0, {{0, n(0.25)}, {1, n(0.25)}, {2, n(0.25)}, {3, n(0.25)}});
    TestFixture::checkRow(probabilities, 1, {{0, n(0.25)}, {2, n(0.75)}});
    TestFixture::checkRow(first, 0, {{0, n(11)}, {2, n(2)}, {3, n(5)}});
    TestFixture::checkRow(second, 0, {{1, n(3)}, {2, n(7)}});
    for (uint64_t row = 1; row < probabilities.getRowCount(); ++row) {
        TestFixture::checkRow(first, row, {});
        TestFixture::checkRow(second, row, {});
    }
    EXPECT_EQ(probabilities.getRowGroupIndices(), first.getRowGroupIndices());
    EXPECT_EQ(probabilities.getRowGroupIndices(), second.getRowGroupIndices());
    EXPECT_EQ((std::unordered_map<uint64_t, BeliefId>{{0, 1}, {1, 0}, {2, 2}, {3, 3}}), mapping);
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("target", 2));
    EXPECT_TRUE(mdp->getStateLabeling().getStateHasLabel("target", 3));
}

TYPED_TEST(BeliefMdpBuilderTest, AccumulatesClippingProbabilitiesForBothDirections) {
    auto info = TestFixture::template makeInfo<typename TestFixture::ClippingInfo>(4);
    info.exploredBeliefs = {{0, 1}, {1, 0}};
    auto const n = TestFixture::value;
    info.terminalBeliefValues = {{3, n(0.5)}};
    info.matrix.transitions = {{n(0.125), 2, {n(0.125), std::nullopt}},
                               {n(0.125), 0, {n(0.125), std::nullopt}},
                               {n(0.25), 3, {n(0.125), std::nullopt}},
                               {n(0.125), 1, {std::nullopt, std::nullopt}}};
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.matrix.transitions.push_back({n(1), 0, {std::nullopt, std::nullopt}});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();

    for (auto direction : {storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize}) {
        auto property = TestFixture::property();
        property.dir = direction;
        auto const [mdp, mapping] = TestFixture::build(info, property);
        auto const target = direction == storm::OptimizationDirection::Minimize ? n(0.5) : n(0.125);
        auto const bottom = direction == storm::OptimizationDirection::Minimize ? n(0.125) : n(0.5);
        TestFixture::checkRow(mdp->getTransitionMatrix(), 0, {{0, n(0.125)}, {1, n(0.125)}, {2, n(0.125)}, {3, target}, {4, bottom}});
    }
}

TYPED_TEST(BeliefMdpBuilderTest, AccumulatesTerminalRewardsAndClippingPenalties) {
    auto info = TestFixture::template makeInfo<typename TestFixture::ClippingInfo>(4);
    info.exploredBeliefs = {{0, 1}, {1, 0}};
    auto const n = TestFixture::value;
    info.terminalBeliefValues = {{3, n(4)}};
    info.matrix.transitions = {{n(0.25), 2, {n(0.125), n(2)}}, {n(0.25), 0, {n(0.125), std::nullopt}}, {n(0.125), 3, {n(0.125), n(3)}}};
    info.matrix.endCurrentRow();
    info.matrix.transitions.push_back({n(1), 3, {std::nullopt, std::nullopt}});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.matrix.transitions.push_back({n(1), 0, {std::nullopt, std::nullopt}});
    info.matrix.endCurrentRow();
    info.matrix.endCurrentRowGroup();
    info.actionRewards = {n(1), n(2), n(3)};
    auto property = TestFixture::property(PropertyInformation::Kind::ExpectedTotalReachabilityReward);
    property.rewardModelName = "reward";

    auto const [mdp, mapping] = TestFixture::build(info, property);
    TestFixture::checkRow(mdp->getTransitionMatrix(), 0, {{1, n(0.25)}, {2, n(0.25)}, {3, n(0.5)}});
    TestFixture::checkRow(mdp->getTransitionMatrix(), 1, {{3, n(1)}});
    EXPECT_EQ((std::vector<TypeParam>{n(6.5), n(6), n(3), n(0.75), n(0), n(0)}), mdp->getRewardModel("reward").getStateActionRewardVector());
}
}  // namespace
