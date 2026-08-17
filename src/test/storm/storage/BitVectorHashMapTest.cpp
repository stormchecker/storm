#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>

#include "storm/storage/BitVector.h"
#include "storm/storage/BitVectorHashMap.h"

TEST(BitVectorHashMapTest, FindOrAdd) {
    storm::storage::BitVectorHashMap<uint64_t> map(64, 3);

    storm::storage::BitVector first(64);
    first.set(4);
    first.set(47);
    ASSERT_NO_THROW(map.findOrAdd(first, 1));

    storm::storage::BitVector second(64);
    second.set(8);
    second.set(18);
    ASSERT_NO_THROW(map.findOrAdd(second, 2));

    EXPECT_EQ(1ul, map.findOrAdd(first, 3));
    EXPECT_EQ(2ul, map.findOrAdd(second, 3));

    storm::storage::BitVector third(64);
    third.set(10);
    third.set(63);

    ASSERT_NO_THROW(map.findOrAdd(third, 3));

    EXPECT_EQ(1ul, map.findOrAdd(first, 2));
    EXPECT_EQ(2ul, map.findOrAdd(second, 1));
    EXPECT_EQ(3ul, map.findOrAdd(third, 1));

    storm::storage::BitVector fourth(64);
    fourth.set(12);
    fourth.set(14);

    ASSERT_NO_THROW(map.findOrAdd(fourth, 4));

    EXPECT_EQ(1ul, map.findOrAdd(first, 2));
    EXPECT_EQ(2ul, map.findOrAdd(second, 1));
    EXPECT_EQ(3ul, map.findOrAdd(third, 1));
    EXPECT_EQ(4ul, map.findOrAdd(fourth, 1));

    storm::storage::BitVector fifth(64);
    fifth.set(44);
    fifth.set(55);

    ASSERT_NO_THROW(map.findOrAdd(fifth, 5));

    EXPECT_EQ(1ul, map.findOrAdd(first, 2));
    EXPECT_EQ(2ul, map.findOrAdd(second, 1));
    EXPECT_EQ(3ul, map.findOrAdd(third, 1));
    EXPECT_EQ(4ul, map.findOrAdd(fourth, 1));
    EXPECT_EQ(5ul, map.findOrAdd(fifth, 1));

    storm::storage::BitVector sixth(64);
    sixth.set(45);
    sixth.set(55);

    ASSERT_NO_THROW(map.findOrAdd(sixth, 6));

    EXPECT_EQ(1ul, map.findOrAdd(first, 0));
    EXPECT_EQ(2ul, map.findOrAdd(second, 0));
    EXPECT_EQ(3ul, map.findOrAdd(third, 0));
    EXPECT_EQ(4ul, map.findOrAdd(fourth, 0));
    EXPECT_EQ(5ul, map.findOrAdd(fifth, 0));
    EXPECT_EQ(6ul, map.findOrAdd(sixth, 0));
}
