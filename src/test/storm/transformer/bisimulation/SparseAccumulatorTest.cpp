#include "storm/transformer/bisimulation/SparseAccumulator.h"

#include "test/storm_gtest.h"

namespace {

TEST(WrappingSetAccumulatorTest, AddWrapAndClear) {
    storm::bisimulation::WrappingSetAccumulator accumulator(4);
    ASSERT_TRUE(accumulator.getNonDefaultStates().empty());
    EXPECT_EQ(0ull, accumulator.getValues()[0]);

    accumulator.addValue(0, 3);
    accumulator.addValue(0, 67);  // 67 % 64 == 3, i.e. the same index as above
    accumulator.addValue(2, 3);
    EXPECT_EQ(std::vector<uint64_t>({0, 2}), accumulator.getNonDefaultStates());
    EXPECT_EQ(1ull << 3, accumulator.getValues()[0]);
    EXPECT_EQ(accumulator.getValues()[0], accumulator.getValues()[2]);  // Both states received the same indices.
    EXPECT_EQ(0ull, accumulator.getValues()[1]);

    accumulator.addValue(1, 4);
    accumulator.addValue(1, 63);
    EXPECT_EQ((1ull << 4) | (1ull << 63), accumulator.getValues()[1]);
    EXPECT_LT(accumulator.getValues()[0], accumulator.getValues()[1]);

    accumulator.clear();
    EXPECT_TRUE(accumulator.getNonDefaultStates().empty());
    for (uint64_t state = 0; state < 4; ++state) {
        EXPECT_EQ(0ull, accumulator.getValues()[state]) << "state " << state;
    }
}

}  // namespace
