#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/storage/BitVector.h"

TEST(BitVectorTest, InitToZero) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_FALSE(vector.get(i));
    }

    ASSERT_TRUE(vector.empty());
    ASSERT_FALSE(vector.full());
}

TEST(BitVectorTest, InitToOne) {
    storm::storage::BitVector vector(32, true);

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_TRUE(vector.get(i));
    }
    ASSERT_FALSE(vector.empty());
    ASSERT_TRUE(vector.full());
}

TEST(BitVectorTest, InitFromIterator) {
    std::vector<uint64_t> valueVector = {0, 4, 10};
    storm::storage::BitVector vector(32, valueVector.begin(), valueVector.end());

    ASSERT_EQ(32ul, vector.size());

    for (uint64_t i = 0; i < 32; ++i) {
        if (i == 0 || i == 4 || i == 10) {
            ASSERT_TRUE(vector.get(i));
        } else {
            ASSERT_FALSE(vector.get(i));
        }
    }
}

TEST(BitVectorTest, InitFromIntVector) {
    std::vector<uint64_t> valueVector = {0, 4, 10};
    storm::storage::BitVector vector(32, valueVector);

    ASSERT_EQ(32ul, vector.size());

    for (uint64_t i = 0; i < 32; ++i) {
        if (i == 0 || i == 4 || i == 10) {
            ASSERT_TRUE(vector.get(i));
        } else {
            ASSERT_FALSE(vector.get(i));
        }
    }
}

TEST(BitVectorTest, GetSet) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector.set(i, i % 2 == 0);
    }

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_EQ(i % 2 == 0, vector.get(i));
    }
}

TEST(BitVectorTest, GetAsInt) {
    storm::storage::BitVector vector(77);

    vector.set(62);
    vector.set(63);
    vector.set(64);
    vector.set(65);

    EXPECT_EQ(3ul, vector.getAsInt(0, 64));
    EXPECT_EQ(1ul, vector.getAsInt(62, 1));
    EXPECT_EQ(3ul, vector.getAsInt(62, 2));
    EXPECT_EQ(7ul, vector.getAsInt(62, 3));
    EXPECT_EQ(15ul, vector.getAsInt(62, 4));

    vector.set(64, false);

    EXPECT_EQ(1ul, vector.getAsInt(62, 1));
    EXPECT_EQ(3ul, vector.getAsInt(62, 2));
    EXPECT_EQ(6ul, vector.getAsInt(62, 3));
    EXPECT_EQ(13ul, vector.getAsInt(62, 4));

    vector.set(61);
    vector.set(62, false);
    EXPECT_EQ(2ul, vector.getAsInt(61, 2));
}

TEST(BitVectorTest, SetFromInt) {
    storm::storage::BitVector vector(77);

    vector.setFromInt(62, 1, 1);

    EXPECT_TRUE(vector.get(62));
    EXPECT_FALSE(vector.get(63));
    EXPECT_FALSE(vector.get(64));
    EXPECT_FALSE(vector.get(65));

    vector.setFromInt(61, 2, 2);

    EXPECT_TRUE(vector.get(61));
    EXPECT_FALSE(vector.get(62));
    EXPECT_FALSE(vector.get(63));

    vector.setFromInt(61, 3, 5);

    EXPECT_TRUE(vector.get(61));
    EXPECT_FALSE(vector.get(62));
    EXPECT_TRUE(vector.get(63));

    vector = storm::storage::BitVector(77);
    vector.setFromInt(62, 4, 15);

    EXPECT_TRUE(vector.get(62));
    EXPECT_TRUE(vector.get(63));
    EXPECT_TRUE(vector.get(64));
    EXPECT_TRUE(vector.get(65));

    vector.setFromInt(62, 5, 17);
}

TEST(BitVectorTest, GetSetInt) {
    storm::storage::BitVector vector(77);

    vector.setFromInt(63, 3, 2);
    EXPECT_EQ(2ul, vector.getAsInt(63, 3));
}

TEST(BitVectorDeathTest, GetSetAssertion) {
    storm::storage::BitVector vector(32);

#ifndef NDEBUG
    EXPECT_DEATH_IF_SUPPORTED(vector.get(32), "");
    EXPECT_DEATH_IF_SUPPORTED(vector.set(32), "");
#else
    std::cerr << "WARNING: Not testing GetSetAssertions, as they are disabled in release mode.\n";
    SUCCEED();
#endif
}

TEST(BitVectorTest, Resize) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector.set(i);
    }

    vector.resize(70);

    ASSERT_EQ(70ul, vector.size());
    ASSERT_EQ(32ul, vector.getNumberOfSetBits());

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_TRUE(vector.get(i));
    }
    bool result;
    for (uint64_t i = 32; i < 70; ++i) {
        result = true;
        ASSERT_NO_THROW(result = vector.get(i));
        ASSERT_FALSE(result);
    }

    vector.resize(72, true);

    std::cout << vector << std::endl;
    ASSERT_EQ(72ul, vector.size());
    ASSERT_EQ(34ul, vector.getNumberOfSetBits());

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_TRUE(vector.get(i));
    }
    for (uint64_t i = 32; i < 70; ++i) {
        result = true;
        ASSERT_NO_THROW(result = vector.get(i));
        ASSERT_FALSE(result);
    }
    for (uint64_t i = 70; i < 72; ++i) {
        ASSERT_TRUE(vector.get(i));
    }

    vector.resize(16, false);
    ASSERT_EQ(16ul, vector.size());
    ASSERT_EQ(16ul, vector.getNumberOfSetBits());

    for (uint64_t i = 0; i < 16; ++i) {
        ASSERT_TRUE(vector.get(i));
    }

    vector.resize(65, true);
    ASSERT_EQ(65ul, vector.size());
    ASSERT_TRUE(vector.full());
}

TEST(BitVectorTest, OperatorAnd) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (int i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }
    vector1.set(31);
    vector2.set(31);

    storm::storage::BitVector andResult = vector1 & vector2;
    for (uint64_t i = 0; i < 31; ++i) {
        ASSERT_FALSE(andResult.get(i));
    }
    ASSERT_TRUE(andResult.get(31));
}

TEST(BitVectorTest, OperatorAndEqual) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (int i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }
    vector1.set(31);
    vector2.set(31);

    vector1 &= vector2;

    for (uint64_t i = 0; i < 31; ++i) {
        ASSERT_FALSE(vector1.get(i));
    }
    ASSERT_TRUE(vector1.get(31));
}

TEST(BitVectorTest, OperatorOr) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }
    vector1.set(31, false);
    vector2.set(31, false);

    storm::storage::BitVector orResult = vector1 | vector2;

    for (uint64_t i = 0; i < 31; ++i) {
        ASSERT_TRUE(orResult.get(i));
    }
    ASSERT_FALSE(orResult.get(31));
}

TEST(BitVectorTest, OperatorOrEqual) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }
    vector1.set(31, false);
    vector2.set(31, false);

    vector1 |= vector2;

    for (uint64_t i = 0; i < 31; ++i) {
        ASSERT_TRUE(vector1.get(i));
    }
    ASSERT_FALSE(vector1.get(31));
}

TEST(BitVectorTest, OperatorXor) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i);
        vector2.set(i, i % 2 == 1);
    }

    storm::storage::BitVector vector3 = vector1 ^ vector2;
    storm::storage::BitVector vector4 = ~vector2;
    storm::storage::BitVector vector5 = vector1 ^ vector1;

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_EQ(vector3.get(i), vector4.get(i));
        ASSERT_FALSE(vector5.get(i));
    }
}

TEST(BitVectorTest, OperatorModulo) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 15; ++i) {
        vector2.set(i, i % 2 == 0);
    }

    vector1.set(2);
    vector1.set(5);
    vector1.set(6);

    storm::storage::BitVector moduloResult = vector1 % vector2;

    ASSERT_EQ(8ul, moduloResult.size());
    ASSERT_EQ(2ul, moduloResult.getNumberOfSetBits());

    for (uint64_t i = 0; i < 8; ++i) {
        if (i == 1 || i == 3) {
            ASSERT_TRUE(moduloResult.get(i));
        } else {
            ASSERT_FALSE(moduloResult.get(i));
        }
    }
}

TEST(BitVectorTest, OperatorNot) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }

    storm::storage::BitVector notResult = ~vector2;

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_EQ(vector1.get(i), notResult.get(i));
    }
}

TEST(BitVectorTest, Complement) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }

    vector2.complement();

    for (uint64_t i = 0; i < 32; ++i) {
        ASSERT_EQ(vector1.get(i), vector2.get(i));
    }
}

TEST(BitVectorTest, Increment) {
    storm::storage::BitVector vector1(130, false);
    storm::storage::BitVector vector2(130, false);

    vector1.increment();
    EXPECT_EQ(1ull, vector1.getNumberOfSetBits());
    vector2.set(0, true);
    EXPECT_EQ(vector1, vector2);

    vector1.increment();
    EXPECT_EQ(1ull, vector1.getNumberOfSetBits());
    vector2.clear();
    vector2.set(1, true);
    EXPECT_EQ(vector1, vector2);

    vector1.increment();
    EXPECT_EQ(2ull, vector1.getNumberOfSetBits());
    vector2.set(0, true);
    EXPECT_EQ(vector1, vector2);

    vector1.clear();
    for (uint64_t i = 0; i < 66; ++i) {
        vector1.set(i, true);
    }
    vector1.increment();
    EXPECT_EQ(1ull, vector1.getNumberOfSetBits());
    vector2.clear();
    vector2.set(66, true);
    EXPECT_EQ(vector1, vector2);

    vector1.increment();
    EXPECT_EQ(2ull, vector1.getNumberOfSetBits());
    vector2.set(0, true);
    EXPECT_EQ(vector1, vector2);

    vector1.clear();
    vector1.complement();
    EXPECT_TRUE(vector1.full());
    vector1.increment();
    EXPECT_TRUE(vector1.empty());
}

TEST(BitVectorTest, permute) {
    storm::storage::BitVector vector1(9, {3, 5});
    std::vector<uint64_t> inversePermutation = {0, 1, 3, 2, 4, 6, 5, 8, 7};
    storm::storage::BitVector vector2 = vector1.permute(inversePermutation);
    EXPECT_EQ(vector1.getNumberOfSetBits(), vector2.getNumberOfSetBits());
    EXPECT_TRUE(vector2.get(2));
    EXPECT_TRUE(vector2.get(6));
}

TEST(BitVectorTest, permuteGrouped) {
    storm::storage::BitVector vector1(6, {0, 2, 5});
    std::vector<uint64_t> inversePermutation = {1, 0, 2};
    std::vector<uint64_t> groupIndices = {0, 3, 5, 6};
    storm::storage::BitVector permuted = vector1.permuteGroupedVector(inversePermutation, groupIndices);
    storm::storage::BitVector expected(6, {2, 4, 5});
    EXPECT_EQ(expected, permuted);
}

TEST(BitVectorTest, Implies) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32, true);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
    }
    vector2.set(31, false);
    vector2.set(30, false);

    storm::storage::BitVector impliesResult = vector1.implies(vector2);

    for (uint64_t i = 0; i < 30; ++i) {
        ASSERT_TRUE(impliesResult.get(i));
    }
    ASSERT_FALSE(impliesResult.get(30));
    ASSERT_TRUE(impliesResult.get(31));
}

TEST(BitVectorTest, Subset) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32, true);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
    }

    ASSERT_TRUE(vector1.isSubsetOf(vector2));

    vector2.set(16, false);

    ASSERT_FALSE(vector1.isSubsetOf(vector2));
}

TEST(BitVectorTest, Disjoint) {
    storm::storage::BitVector vector1(32);
    storm::storage::BitVector vector2(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector1.set(i, i % 2 == 0);
        vector2.set(i, i % 2 == 1);
    }

    ASSERT_TRUE(vector1.isDisjointFrom(vector2));

    vector2.set(16, true);

    ASSERT_FALSE(vector1.isDisjointFrom(vector2));
}

TEST(BitVectorTest, Empty) {
    storm::storage::BitVector vector(32);

    ASSERT_TRUE(vector.empty());

    vector.set(17, true);

    ASSERT_FALSE(vector.empty());

    vector.set(17, false);
    vector.set(18, false);

    ASSERT_TRUE(vector.empty());
}

TEST(BitVectorTest, Full) {
    storm::storage::BitVector vector(32, true);

    ASSERT_TRUE(vector.full());

    vector.set(17, false);

    ASSERT_FALSE(vector.full());

    vector.set(17, true);
    vector.set(18, true);

    ASSERT_TRUE(vector.full());
}

TEST(BitVectorTest, NumberOfSetBits) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector.set(i, i % 2 == 0);
    }

    ASSERT_EQ(16ul, vector.getNumberOfSetBits());
}

TEST(BitVectorTest, NumberOfSetBitsBeforeIndex) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector.set(i, i % 2 == 0);
    }

    ASSERT_EQ(7ul, vector.getNumberOfSetBitsBeforeIndex(14));
}

TEST(BitVectorTest, BeginEnd) {
    storm::storage::BitVector vector(32);

    ASSERT_TRUE(vector.begin() == vector.end());

    vector.set(17);

    ASSERT_FALSE(vector.begin() == vector.end());

    vector.set(17, false);

    ASSERT_TRUE(vector.begin() == vector.end());
}

TEST(BitVectorTest, NextSetIndex) {
    storm::storage::BitVector vector(32);

    vector.set(14);
    vector.set(17);

    ASSERT_EQ(14ul, vector.getNextSetIndex(14));
    ASSERT_EQ(17ul, vector.getNextSetIndex(15));
    ASSERT_EQ(17ul, vector.getNextSetIndex(16));
    ASSERT_EQ(17ul, vector.getNextSetIndex(17));
    ASSERT_EQ(vector.size(), vector.getNextSetIndex(18));
}

TEST(BitVectorTest, NextUnsetIndex) {
    storm::storage::BitVector vector(32);

    vector.set(14);
    vector.set(17);

    vector.complement();

    ASSERT_EQ(14ul, vector.getNextUnsetIndex(14));
    ASSERT_EQ(17ul, vector.getNextUnsetIndex(15));
    ASSERT_EQ(17ul, vector.getNextUnsetIndex(16));
    ASSERT_EQ(17ul, vector.getNextUnsetIndex(17));
    ASSERT_EQ(vector.size(), vector.getNextUnsetIndex(18));
}

TEST(BitVectorTest, SequenceBefore) {
    storm::storage::BitVector vector(65);

    vector.set(14);
    vector.set(17);
    vector.set(64);

    auto vector_compl = ~vector;

    for (uint64_t i = 0; i <= 65; ++i) {
        uint64_t expected;
        if (i <= 14) {
            expected = 0ul;
        } else if (i <= 17) {
            expected = 15ul;
        } else if (i <= 64) {
            expected = 18ul;
        } else {
            expected = 65ul;
        }
        ASSERT_EQ(expected, vector.getStartOfZeroSequenceBefore(i)) << " input index is i=" << i;
        ASSERT_EQ(expected, vector_compl.getStartOfOneSequenceBefore(i)) << " input index is i=" << i;
    }
}

TEST(BitVectorTest, Iterator) {
    storm::storage::BitVector vector(32);

    for (uint64_t i = 0; i < 32; ++i) {
        vector.set(i, i % 2 == 0);
    }

    uint64_t i = 0;
    for (uint64_t bit : vector) {
        ASSERT_EQ(i, bit);
        i += 2;
    }
    ASSERT_EQ(i, 32ull);
}

TEST(BitVectorTest, ReverseIterator) {
    storm::storage::BitVector vector(547490);

    uint64_t i = 2;
    for (; i < vector.size(); i += 3) {
        vector.set(i, true);
    }

    for (auto bitIt = vector.rbegin(); bitIt != vector.rend(); ++bitIt) {
        i -= 3;
        ASSERT_EQ(i, *bitIt);
    }
    ASSERT_EQ(i, 2ull);
}

TEST(BitVectorTest, CompareAndSwap) {
    storm::storage::BitVector vector(140);
    vector.setFromInt(0, 64, 2377830234574424100);
    vector.setFromInt(64, 64, 1152921504607379586);
    vector.setFromInt(128, 12, 2080);

    bool result = vector.compareAndSwap(0, 68, 68);
    ASSERT_FALSE(result);

    result = vector.compareAndSwap(68, 0, 68);
    ASSERT_TRUE(result);
}

TEST(BitVectorTest, Concat) {
    storm::storage::BitVector vector1(64, {3, 5});
    storm::storage::BitVector vector2(65, {10, 12});

    vector1.concat(vector2);
    ASSERT_EQ(129ul, vector1.size());
    ASSERT_TRUE(vector1.get(3));
    ASSERT_TRUE(vector1.get(5));
    ASSERT_TRUE(vector1.get(10 + 64));
    ASSERT_TRUE(vector1.get(12 + 64));
    ASSERT_EQ(4ul, vector1.getNumberOfSetBits());
}

TEST(BitVectorTest, Expand) {
    storm::storage::BitVector vector1(64, {3, 5});
    vector1.expandSize();
    ASSERT_EQ(64ul, vector1.size());
    ASSERT_EQ(2ul, vector1.getNumberOfSetBits());
    storm::storage::BitVector vector2(65, {10, 12});
    vector2.expandSize();
    ASSERT_EQ(128ul, vector2.size());
    ASSERT_EQ(2ul, vector2.getNumberOfSetBits());
}

TEST(BitVectorTest, Assignment) {
    storm::storage::BitVector v1(10), v2(100000);
    v1 = v2;
    v1.set(9999);
    ASSERT_TRUE(v1.get(9999));
}

TEST(BitVectorTest, ZeroSized) {
    storm::storage::BitVector v(0);
    EXPECT_EQ(0ul, v.size());
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(v.full());
    EXPECT_EQ(0ul, v.getNumberOfSetBits());
    EXPECT_EQ(0ul, v.getAsInt(0, 0));
    EXPECT_EQ(0ul, v.getNextSetIndex(0));
    EXPECT_EQ(v, v);
    EXPECT_EQ(v, ~v);
    for (uint64_t entry : v) {
        FAIL() << "Should not iterate over an empty bit vector.";
        ASSERT_FALSE(entry);
    }
}
