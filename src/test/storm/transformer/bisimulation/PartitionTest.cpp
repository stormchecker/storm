#include <sstream>
#include <vector>
#include "storm/transformer/bisimulation/Partition.h"

#include "test/storm_gtest.h"

namespace {

std::string blockToString(auto const& block) {
    std::ostringstream oss;
    oss << "{";
    for (bool first{true}; auto const e : block) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << e;
    }
    oss << "}";
    return oss.str();
}

bool equalBlocks(storm::bisimulation::Partition const& partition, std::initializer_list<uint64_t> const& expected,
                 storm::bisimulation::Partition::Block const& actual) {
    bool const expectedContainsActual =
        std::all_of(actual.begin(), actual.end(), [&expected](auto const& e) { return std::find(expected.begin(), expected.end(), e) != expected.end(); });
    EXPECT_TRUE(expectedContainsActual) << "Content mismatch: " << blockToString(expected) << " vs. " << blockToString(actual);
    bool const actualContainsExpected =
        std::all_of(expected.begin(), expected.end(), [&actual](auto const& e) { return std::find(actual.begin(), actual.end(), e) != actual.end(); });
    EXPECT_TRUE(actualContainsExpected) << "Content missmatch: " << blockToString(expected) << " vs. " << blockToString(actual);

    // Also test the contains method
    for (auto const& e : expected) {
        EXPECT_TRUE(partition.contains(actual, e)) << "Element " << e << " not in block " << blockToString(actual);
    }

    return expectedContainsActual && actualContainsExpected;
}

TEST(PartitionTest, Basic) {
    storm::bisimulation::Partition partition(10);

    // Check initial partition
    EXPECT_EQ(10ul, partition.getNumberOfElements());
    EXPECT_EQ(1ul, partition.getNumberOfBlocks());
    for (auto i = 0ul; i < 10; ++i) {
        auto block = partition.getBlockOfElement(i);
        ASSERT_TRUE(partition.checkBlockValidity(block)) << "Block " << blockToString(block) << " is not valid.";
        EXPECT_TRUE(equalBlocks(partition, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, block));
    }
    auto initBlock = partition.getBlockOfElement(0);
    ASSERT_FALSE(partition.isProperSuperBlock(initBlock));

    // Split the partition into two blocks (odd and even)
    partition.forEachBlock([&partition](auto const& block) {
        auto [odd, even] = partition.splitBlockByPredicate(block, [](auto const& e) { return e % 2 == 0; });
        ASSERT_TRUE(partition.checkBlockValidity(odd)) << "Block " << blockToString(odd) << " is not valid.";
        ASSERT_TRUE(partition.checkBlockValidity(even)) << "Block " << blockToString(even) << " is not valid.";
        EXPECT_TRUE(equalBlocks(partition, {1, 3, 5, 7, 9}, odd));
        EXPECT_TRUE(equalBlocks(partition, {0, 2, 4, 6, 8}, even));
    });
    ASSERT_TRUE(partition.checkBlockValidity(initBlock)) << "Block " << blockToString(initBlock) << " is not valid.";
    EXPECT_EQ(2ul, partition.getNumberOfBlocks());
    ASSERT_TRUE(partition.isProperSuperBlock(initBlock));
    auto oddBlock = partition.getBlockOfElement(1);
    auto evenBlock = partition.getBlockOfElement(0);

    // Split the odd block w.r.t. their  remainder when divided by 3
    EXPECT_TRUE(partition.splitBlockByOrder(oddBlock, [](auto const& a, auto const& b) { return a % 3 < b % 3; }));
    uint64_t seenSubBlocks = 0;  // very "smart" (aka lazy) encoding of the seen subsets
    partition.forEachSubBlock(oddBlock, [&partition, &seenSubBlocks](auto const& block) {
        ASSERT_TRUE(partition.checkBlockValidity(block)) << "Block " << blockToString(block) << " is not valid.";
        if (partition.contains(block, 1)) {
            EXPECT_TRUE(equalBlocks(partition, {1, 7}, block));
            seenSubBlocks += 1;
        } else if (partition.contains(block, 3)) {
            EXPECT_TRUE(equalBlocks(partition, {3, 9}, block));
            seenSubBlocks += 10;
        } else if (partition.contains(block, 5)) {
            EXPECT_TRUE(equalBlocks(partition, {5}, block));
            seenSubBlocks += 100;
        } else {
            FAIL() << "Block " << blockToString(block) << " does not contain any expected elements.";
        }
    });
    EXPECT_EQ(111ul, seenSubBlocks) << "Not all sub blocks have been seen.";
    EXPECT_EQ(4ul, partition.getNumberOfBlocks());

    // Check if the contains method works as expected
    for (auto i = 0ul; i < 10; ++i) {
        EXPECT_EQ(partition.contains(evenBlock, i), i % 2 == 0) << "Element " << i << " not in block " << blockToString(i % 2 == 0 ? evenBlock : oddBlock);
        EXPECT_EQ(partition.contains(oddBlock, i), i % 2 != 0) << "Element " << i << " in block " << blockToString(i % 2 != 0 ? oddBlock : evenBlock);
    }

    EXPECT_TRUE(partition.isProperSuperBlock(oddBlock));
    EXPECT_TRUE(equalBlocks(partition, {1, 3, 5, 7, 9}, oddBlock));

    // Perform splits that have no effect, i.e., do not change the number of blocks
    auto [evenAndOddBlock, evenAndEvenBlock] = partition.splitBlockByPredicate(evenBlock, [](auto const& e) { return e % 2 == 0; });
    EXPECT_TRUE(evenAndOddBlock.empty());
    EXPECT_TRUE(equalBlocks(partition, {0, 2, 4, 6, 8}, evenAndEvenBlock));
    EXPECT_FALSE(partition.isProperSuperBlock(evenBlock));

    auto [less10, geq10] = partition.splitBlockByPredicate(evenBlock, [](auto const& e) { return e >= 10; });
    EXPECT_TRUE(geq10.empty());
    EXPECT_TRUE(equalBlocks(partition, {0, 2, 4, 6, 8}, less10));
    EXPECT_FALSE(partition.isProperSuperBlock(evenBlock));

    EXPECT_FALSE(partition.splitBlockByOrder(evenBlock, [](auto const& a, auto const& b) { return a % 2 < b % 2; }));
    EXPECT_FALSE(partition.isProperSuperBlock(evenBlock));

    typename storm::bisimulation::Partition::OrderedBlockSet blockSet;
    blockSet.insert(evenBlock);
    blockSet.insert(oddBlock);
    EXPECT_EQ(blockSet.size(), 2);
    blockSet.insert(evenAndEvenBlock);  // (re-)inserts the even block
    EXPECT_EQ(blockSet.size(), 2);
    blockSet.insert(partition.getBlockOfElement(0));  // (re-)inserts the even block
    EXPECT_EQ(blockSet.size(), 2);
    EXPECT_EQ(blockSet.size(), 2);
    blockSet.insert(partition.getBlockOfElement(1));  // inserts a sub-block of oddBlock
    EXPECT_EQ(blockSet.size(), 3);
};

TEST(PartitionTest, SplitByRange) {
    storm::bisimulation::Partition partition(10);

    // Split into "not in range" and "in range" parts
    auto [no, yes] = partition.splitBlockByRange(partition.getUniversalBlock(), std::vector<uint64_t>{1, 3, 5, 7, 9});
    ASSERT_TRUE(partition.checkBlockValidity(no)) << "Block " << blockToString(no) << " is not valid.";
    ASSERT_TRUE(partition.checkBlockValidity(yes)) << "Block " << blockToString(yes) << " is not valid.";
    EXPECT_TRUE(equalBlocks(partition, {0, 2, 4, 6, 8}, no));
    EXPECT_TRUE(equalBlocks(partition, {1, 3, 5, 7, 9}, yes));
    EXPECT_EQ(2ul, partition.getNumberOfBlocks());

    // Duplicates in the range as well as elements that are not part of the block being split (but are valid elements of the partition) must be handled
    // gracefully
    auto [notInRange, inRange] = partition.splitBlockByRange(no, std::vector<uint64_t>{0, 4, 4, 8, 1, 3});  // 1 and 3 do not belong to `no`
    ASSERT_TRUE(partition.checkBlockValidity(notInRange)) << "Block " << blockToString(notInRange) << " is not valid.";
    ASSERT_TRUE(partition.checkBlockValidity(inRange)) << "Block " << blockToString(inRange) << " is not valid.";
    EXPECT_TRUE(equalBlocks(partition, {2, 6}, notInRange));
    EXPECT_TRUE(equalBlocks(partition, {0, 4, 8}, inRange));
    EXPECT_EQ(3ul, partition.getNumberOfBlocks());

    // Splitting with an empty range has no effect
    auto [allOfYes, emptyBlock] = partition.splitBlockByRange(yes, std::vector<uint64_t>{});
    EXPECT_TRUE(emptyBlock.empty());
    EXPECT_TRUE(equalBlocks(partition, {1, 3, 5, 7, 9}, allOfYes));
    EXPECT_FALSE(partition.isProperSuperBlock(yes));

    // Splitting with a range that contains all elements of the block has no effect
    auto [emptyBlock2, allOfYes2] = partition.splitBlockByRange(yes, std::vector<uint64_t>{1, 3, 5, 7, 9});
    EXPECT_TRUE(emptyBlock2.empty());
    EXPECT_TRUE(equalBlocks(partition, {1, 3, 5, 7, 9}, allOfYes2));
    EXPECT_FALSE(partition.isProperSuperBlock(yes));
    EXPECT_EQ(3ul, partition.getNumberOfBlocks());
}

TEST(PartitionTest, SplitByClustering) {
    // Values are deliberately interleaved so that same-valued elements are not adjacent; splitBlockByClustering must not rely on any pre-existing order.
    std::vector<int> const value = {0, 5, 0, 5, 0, 9};
    storm::bisimulation::Partition partition(value.size());
    auto const condition = [&value](uint64_t const a, uint64_t const b) { return value[a] != value[b]; };

    EXPECT_TRUE(partition.splitBlockByClustering(partition.getUniversalBlock(), condition));
    EXPECT_EQ(3ul, partition.getNumberOfBlocks());
    partition.forEachBlock([&partition](auto const& block) {
        ASSERT_TRUE(partition.checkBlockValidity(block)) << "Block " << blockToString(block) << " is not valid.";
        if (partition.contains(block, 0)) {
            EXPECT_TRUE(equalBlocks(partition, {0, 2, 4}, block));
        } else if (partition.contains(block, 1)) {
            EXPECT_TRUE(equalBlocks(partition, {1, 3}, block));
        } else if (partition.contains(block, 5)) {
            EXPECT_TRUE(equalBlocks(partition, {5}, block));
        } else {
            FAIL() << "Block " << blockToString(block) << " does not contain any expected elements.";
        }
    });

    // A condition that never holds must not split anything.
    {
        storm::bisimulation::Partition trivialPartition(4);
        EXPECT_FALSE(trivialPartition.splitBlockByClustering(trivialPartition.getUniversalBlock(), [](uint64_t const, uint64_t const) { return false; }));
        EXPECT_EQ(1ul, trivialPartition.getNumberOfBlocks());
    }

    // Unlike a transitive-closure-based clustering, splitBlockByClustering only compares every element against the (fixed) anchor of its group, not
    // against each other: with values 0, 4, 8 and a "must split" threshold of 5, element 1 (value 4) is within reach of anchor 0 (|4-0|=4) and gets
    // grouped with it, even though it would also be within reach of element 2 (value 8, |8-4|=4) - once claimed by the first anchor, it is not
    // reconsidered, so element 2 (|8-0|=8, exceeds the threshold) ends up alone rather than transitively merged with 0 via 1.
    {
        std::vector<int> const chainValue = {0, 4, 8};
        storm::bisimulation::Partition chainPartition(chainValue.size());
        auto const chainCondition = [&chainValue](uint64_t const a, uint64_t const b) { return std::abs(chainValue[a] - chainValue[b]) > 5; };
        EXPECT_TRUE(chainPartition.splitBlockByClustering(chainPartition.getUniversalBlock(), chainCondition));
        EXPECT_EQ(2ul, chainPartition.getNumberOfBlocks());
        EXPECT_TRUE(equalBlocks(chainPartition, {0, 1}, chainPartition.getBlockOfElement(0)));
        EXPECT_TRUE(equalBlocks(chainPartition, {2}, chainPartition.getBlockOfElement(2)));
    }
}

TEST(PartitionTest, UniversalBlockAndSubBlockRelation) {
    storm::bisimulation::Partition partition(8);
    auto universal = partition.getUniversalBlock();
    EXPECT_TRUE(equalBlocks(partition, {0, 1, 2, 3, 4, 5, 6, 7}, universal));
    EXPECT_TRUE(partition.isSubBlockOf(universal, universal));  // a block is a sub-block of itself
    EXPECT_TRUE(partition.isBlockOfElement(universal, 3));

    auto [odd, even] = partition.splitBlockByPredicate(universal, [](auto const& e) { return e % 2 == 0; });

    // getUniversalBlock always spans all elements, even though the partition now consists of multiple blocks
    auto stillUniversal = partition.getUniversalBlock();
    EXPECT_TRUE(equalBlocks(partition, {0, 1, 2, 3, 4, 5, 6, 7}, stillUniversal));
    EXPECT_TRUE(partition.isProperSuperBlock(stillUniversal));

    EXPECT_TRUE(partition.isSubBlockOf(odd, stillUniversal));
    EXPECT_TRUE(partition.isSubBlockOf(even, stillUniversal));
    EXPECT_FALSE(partition.isSubBlockOf(stillUniversal, odd));
    EXPECT_FALSE(partition.isSubBlockOf(odd, even));

    // `odd` is the finest currently known block for element 1, but the (now proper super-) universal block is not
    EXPECT_TRUE(partition.isBlockOfElement(odd, 1));
    EXPECT_FALSE(partition.isBlockOfElement(stillUniversal, 1));
    EXPECT_FALSE(partition.isBlockOfElement(even, 1));
}

TEST(PartitionTest, SingletonBlockSplits) {
    // A partition consisting of a single element: the sole block always starts at index 0.
    {
        storm::bisimulation::Partition partition(1);
        auto block = partition.getBlockOfElement(0);
        EXPECT_FALSE(partition.splitBlockByOrder(block, [](auto const& a, auto const& b) { return a < b; }));

        auto [emptyFalsePart, allTrue] = partition.splitBlockByPredicate(block, [](auto const&) { return true; });
        EXPECT_TRUE(emptyFalsePart.empty());
        EXPECT_TRUE(equalBlocks(partition, {0}, allTrue));

        auto [allFalse, emptyTruePart] = partition.splitBlockByPredicate(block, [](auto const&) { return false; });
        EXPECT_TRUE(emptyTruePart.empty());
        EXPECT_TRUE(equalBlocks(partition, {0}, allFalse));

        EXPECT_FALSE(partition.isProperSuperBlock(block));
        ASSERT_TRUE(partition.checkBlockValidity(block));
    }

    // A singleton block resulting from splitting a larger partition, i.e., one that does not start at index 0
    {
        storm::bisimulation::Partition partition(5);
        auto [odd, even] = partition.splitBlockByPredicate(partition.getUniversalBlock(), [](auto const& e) { return e % 2 == 0; });
        auto [rest, isolated] = partition.splitBlockByPredicate(even, [](auto const& e) { return e == 0; });
        EXPECT_TRUE(equalBlocks(partition, {0}, isolated));

        EXPECT_FALSE(partition.splitBlockByOrder(isolated, [](auto const& a, auto const& b) { return a < b; }));

        auto [notInRange, inRange] = partition.splitBlockByRange(isolated, std::vector<uint64_t>{0});
        EXPECT_TRUE(notInRange.empty());
        EXPECT_TRUE(equalBlocks(partition, {0}, inRange));

        auto [notInRange2, inRange2] = partition.splitBlockByRange(isolated, std::vector<uint64_t>{});
        EXPECT_TRUE(inRange2.empty());
        EXPECT_TRUE(equalBlocks(partition, {0}, notInRange2));

        ASSERT_TRUE(partition.checkBlockValidity(isolated));
    }
}

TEST(PartitionTest, NonSuperBlockSet) {
    storm::bisimulation::Partition partition(6);
    storm::bisimulation::Partition::NonSuperBlockSet nonSuperBlockSet(partition);

    EXPECT_TRUE(nonSuperBlockSet.empty());
    EXPECT_EQ(0ul, nonSuperBlockSet.size());

    auto universalBlock = partition.getUniversalBlock();
    nonSuperBlockSet.insert(universalBlock);
    EXPECT_FALSE(nonSuperBlockSet.empty());
    EXPECT_EQ(1ul, nonSuperBlockSet.size());
    EXPECT_TRUE(nonSuperBlockSet.contains(universalBlock));

    // Inserting the same (leaf) block again has no effect
    nonSuperBlockSet.insert(partition.getBlockOfElement(3));
    EXPECT_EQ(1ul, nonSuperBlockSet.size());

    auto poppedBlock = nonSuperBlockSet.pop();
    EXPECT_TRUE(equalBlocks(partition, {0, 1, 2, 3, 4, 5}, poppedBlock));
    EXPECT_TRUE(nonSuperBlockSet.empty());

    auto [odd, even] = partition.splitBlockByPredicate(poppedBlock, [](auto const& e) { return e % 2 == 0; });
    nonSuperBlockSet.insert(odd);
    nonSuperBlockSet.insert(even);
    EXPECT_EQ(2ul, nonSuperBlockSet.size());
    EXPECT_TRUE(nonSuperBlockSet.contains(odd));
    EXPECT_TRUE(nonSuperBlockSet.contains(even));

    // Popping removes an arbitrary (but existing) block from the set; here we don't rely on a particular order
    auto b0 = nonSuperBlockSet.pop();
    auto b1 = nonSuperBlockSet.pop();
    EXPECT_TRUE(nonSuperBlockSet.empty());
    if (partition.contains(b0, 1)) {
        EXPECT_TRUE(equalBlocks(partition, {1, 3, 5}, b0));
        EXPECT_TRUE(equalBlocks(partition, {0, 2, 4}, b1));
    } else {
        EXPECT_TRUE(equalBlocks(partition, {0, 2, 4}, b0));
        EXPECT_TRUE(equalBlocks(partition, {1, 3, 5}, b1));
    }
    EXPECT_FALSE(nonSuperBlockSet.contains(odd));
    EXPECT_FALSE(nonSuperBlockSet.contains(even));
}
}  // namespace
