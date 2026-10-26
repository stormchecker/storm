#include "test/storm_gtest.h"

#include <optional>
#include <type_traits>

#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/storage/BeliefCollector.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/OutOfRangeException.h"

namespace {
using namespace storm::pomdp::beliefs;

template<typename ValueType>
Belief<ValueType> makeBelief(BeliefStateType state = 0, BeliefObservationType observation = 0, double firstProbability = 0.5) {
    BeliefBuilder<Belief<ValueType>> builder;
    builder.setObservation(observation);
    builder.addValue(state, storm::utility::convertNumber<ValueType>(firstProbability));
    builder.addValue(state + 1, storm::utility::convertNumber<ValueType>(1.0 - firstProbability));
    return builder.build();
}

template<typename ValueType>
ValueType const* firstValueAddress(Belief<ValueType> const& belief) {
    ValueType const* address = nullptr;
    belief.forEach([&address](auto const&, auto const& value) {
        if (!address) {
            address = &value;
        }
    });
    return address;
}

template<typename ValueType>
class BeliefCollectorTest : public ::testing::Test {
   public:
    using Collector = BeliefCollector<Belief<ValueType>>;

    static_assert(std::is_nothrow_move_constructible_v<Collector>);
    static_assert(std::is_nothrow_move_assignable_v<Collector>);
    static_assert(std::is_nothrow_move_constructible_v<Belief<ValueType>>);
    static_assert(!std::is_copy_assignable_v<Belief<ValueType>>);
    static_assert(!std::is_move_assignable_v<Belief<ValueType>>);

    static void fill(Collector& collector, uint64_t count) {
        for (uint64_t i = 0; i < count; ++i) {
            ASSERT_EQ(i, collector.addBelief(makeBelief<ValueType>(2 * i, 2 * (i % 7))));
        }
    }

    static void checkLookups(Collector const& collector, uint64_t count) {
        EXPECT_EQ(count, collector.getNumberOfBeliefIds());
        for (uint64_t i = 0; i < count; ++i) {
            auto const belief = makeBelief<ValueType>(2 * i, 2 * (i % 7));
            EXPECT_TRUE(collector.containsId(i));
            EXPECT_TRUE(collector.containsBelief(belief));
            EXPECT_EQ(i, collector.getIdFromBelief(belief));
            EXPECT_TRUE(belief == collector.getBeliefFromId(i));
        }
    }
};

using BeliefValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(BeliefCollectorTest, BeliefValueTypes, );

TYPED_TEST(BeliefCollectorTest, InternsBeliefs) {
    typename TestFixture::Collector collector;
    auto const belief = makeBelief<TypeParam>();
    EXPECT_FALSE(collector.containsId(0));
    EXPECT_FALSE(collector.containsBelief(belief));
    EXPECT_EQ(InvalidBeliefId, collector.getIdOptional(belief));

    EXPECT_EQ(0, collector.getIdOrAddBelief(makeBelief<TypeParam>()));
    EXPECT_EQ(0, collector.getIdOrAddBelief(makeBelief<TypeParam>()));
    EXPECT_EQ(1, collector.getNumberOfBeliefIds());
    EXPECT_EQ(0, collector.getIdFromBelief(belief));

    EXPECT_EQ(1, collector.getIdOrAddBelief(makeBelief<TypeParam>(0, 0, 0.25)));
    EXPECT_EQ(2, collector.getIdOrAddBelief(makeBelief<TypeParam>(0, 8)));
    EXPECT_EQ(2, collector.getIdOrAddBelief(makeBelief<TypeParam>(0, 8)));
    EXPECT_FALSE(collector.isEqual(0, 1));
    EXPECT_FALSE(collector.isEqual(0, 2));
    EXPECT_TRUE(collector.isEqual(0, 0));
    EXPECT_EQ(3, collector.getNumberOfBeliefIds());
    EXPECT_FALSE(collector.containsId(3));
    EXPECT_EQ(InvalidBeliefId, collector.getIdOptional(makeBelief<TypeParam>(20, 0)));
    EXPECT_EQ(InvalidBeliefId, collector.getIdOptional(makeBelief<TypeParam>(0, 3)));
    EXPECT_FALSE(collector.containsBelief(makeBelief<TypeParam>(0, 99)));
}

TYPED_TEST(BeliefCollectorTest, MissingRequiredLookupThrowsStormException) {
    typename TestFixture::Collector collector;
    ASSERT_EQ(0, collector.addBelief(makeBelief<TypeParam>()));
    auto const missingBelief = makeBelief<TypeParam>(20);
    EXPECT_EQ(InvalidBeliefId, collector.getIdOptional(missingBelief));
    EXPECT_THROW(collector.getIdFromBelief(missingBelief), storm::exceptions::OutOfRangeException);
    EXPECT_EQ(0, collector.getIdFromBelief(makeBelief<TypeParam>()));
}

TYPED_TEST(BeliefCollectorTest, PreservesDistributionStorageThroughGrowth) {
    typename TestFixture::Collector collector;
    auto firstBelief = makeBelief<TypeParam>();
    auto const* firstValue = firstValueAddress(firstBelief);
    ASSERT_EQ(0, collector.addBelief(std::move(firstBelief)));
    EXPECT_EQ(firstValue, firstValueAddress(collector.getBeliefFromId(0)));

    constexpr uint64_t count = 1024;
    for (uint64_t i = 1; i < count; ++i) {
        ASSERT_EQ(i, collector.addBelief(makeBelief<TypeParam>(2 * i, 2 * (i % 7))));
    }
    EXPECT_EQ(firstValue, firstValueAddress(collector.getBeliefFromId(0)));
    TestFixture::checkLookups(collector, count);
    EXPECT_EQ(0, collector.getIdOrAddBelief(makeBelief<TypeParam>()));
    EXPECT_EQ(count, collector.getNumberOfBeliefIds());
}

TYPED_TEST(BeliefCollectorTest, CopiesUseIndependentStorage) {
    using Collector = typename TestFixture::Collector;
    std::optional<Collector> copied;
    constexpr uint64_t count = 64;
    {
        Collector original;
        TestFixture::fill(original, count);
        copied.emplace(original);
        EXPECT_NE(firstValueAddress(original.getBeliefFromId(0)), firstValueAddress(copied->getBeliefFromId(0)));
        original.addBelief(makeBelief<TypeParam>(10000, 20));
        EXPECT_FALSE(copied->containsBelief(makeBelief<TypeParam>(10000, 20)));
    }
    TestFixture::checkLookups(*copied, count);
    EXPECT_EQ(count, copied->addBelief(makeBelief<TypeParam>(10000, 20)));
    EXPECT_EQ(count, copied->getIdFromBelief(makeBelief<TypeParam>(10000, 20)));
}

TYPED_TEST(BeliefCollectorTest, MovesPreserveIndex) {
    using Collector = typename TestFixture::Collector;
    Collector assigned;
    assigned.addBelief(makeBelief<TypeParam>(10000, 20));
    constexpr uint64_t count = 64;
    {
        Collector original;
        TestFixture::fill(original, count);
        auto const* firstValue = firstValueAddress(original.getBeliefFromId(0));
        Collector moved(std::move(original));
        EXPECT_EQ(firstValue, firstValueAddress(moved.getBeliefFromId(0)));
        TestFixture::checkLookups(moved, count);
        assigned = std::move(moved);
        EXPECT_EQ(firstValue, firstValueAddress(assigned.getBeliefFromId(0)));
        TestFixture::checkLookups(assigned, count);
    }
    TestFixture::checkLookups(assigned, count);
    EXPECT_EQ(count, assigned.addBelief(makeBelief<TypeParam>(10000, 20)));
    EXPECT_EQ(count, assigned.getIdFromBelief(makeBelief<TypeParam>(10000, 20)));
}

TEST(BeliefCollectorNumericsTest, UsesExistingDoubleEquality) {
    BeliefCollector<Belief<double>> collector;
    ASSERT_EQ(0, collector.addBelief(makeBelief<double>()));
    auto const equivalent = makeBelief<double>(0, 0, 0.5 + 1e-15);
    ASSERT_TRUE(equivalent == collector.getBeliefFromId(0));
    EXPECT_EQ(0, collector.getIdFromBelief(equivalent));
    EXPECT_EQ(0, collector.getIdOrAddBelief(makeBelief<double>(0, 0, 0.5 + 1e-15)));
    EXPECT_EQ(1, collector.getIdOrAddBelief(makeBelief<double>(0, 0, 0.5 + 1e-12)));
    EXPECT_EQ(2, collector.getNumberOfBeliefIds());
}
}  // namespace
