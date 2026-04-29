#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/RationalApproximation.h"
#include "storm/utility/constants.h"

namespace {

storm::RationalNumber rn(double doubleValue) {
    return storm::utility::convertNumber<storm::RationalNumber>(doubleValue);
}

storm::RationalNumber rn(std::string const& str) {
    return storm::utility::convertNumber<storm::RationalNumber>(str);
}
TEST(RationalApproximationTest, inclusive_bounds) {
    EXPECT_EQ(rn("0"), storm::utility::findRational(rn("0"), true, rn("0"), true));
    EXPECT_EQ(rn("1"), storm::utility::findRational(rn("1"), true, rn("1"), true));
    EXPECT_EQ(rn("0"), storm::utility::findRational(rn("0"), true, rn("1"), true));
    EXPECT_EQ(rn("1/2"), storm::utility::findRational(rn("1/3"), true, rn("2/3"), true));
    EXPECT_EQ(rn("1/2"), storm::utility::findRational(rn("1/10"), true, rn("9/10"), true));
    EXPECT_EQ(rn("1"), storm::utility::findRational(rn("1/2"), true, rn("1"), true));
    EXPECT_EQ(rn("1"), storm::utility::findRational(rn("2/3"), true, rn("1"), true));
    EXPECT_EQ(rn("2/3"), storm::utility::findRational(rn("2/3"), true, rn("3/4"), true));
    EXPECT_EQ(rn("2/3"), storm::utility::findRational(rn("3/5"), true, rn("3/4"), true));
    EXPECT_EQ(rn("1"), storm::utility::findRational(rn("2/3"), true, rn("123456"), true));
    EXPECT_EQ(rn("23/3"), storm::utility::findRational(rn("23/3"), true, rn("31/4"), true));
    EXPECT_EQ(rn("23/3"), storm::utility::findRational(rn("38/5"), true, rn("31/4"), true));
    EXPECT_EQ(rn("75/7"), storm::utility::findRational(rn(10.71), true, rn(10.72), true));
    EXPECT_EQ(rn(0.123456), storm::utility::findRational(rn(0.123456), true, rn(0.123456), true));
    EXPECT_EQ(rn(987.123456), storm::utility::findRational(rn(987.123456), true, rn(987.123456), true));
}

TEST(RationalApproximationTest, exclusive_bounds) {
    EXPECT_EQ(rn("1/2"), storm::utility::findRational(rn("0"), false, rn("1"), false));
    EXPECT_EQ(rn("0"), storm::utility::findRational(rn("0"), true, rn("1"), false));
    EXPECT_EQ(rn("1"), storm::utility::findRational(rn("0"), false, rn("1"), true));
    EXPECT_EQ(rn("1/3"), storm::utility::findRational(rn("0"), false, rn("1/2"), false));
    EXPECT_EQ(rn("2/3"), storm::utility::findRational(rn("1/2"), false, rn("1"), false));
    EXPECT_EQ(rn("3/4"), storm::utility::findRational(rn("2/3"), false, rn("1"), false));
    EXPECT_EQ(rn("5/7"), storm::utility::findRational(rn("2/3"), false, rn("3/4"), false));
    EXPECT_EQ(rn("3/2"), storm::utility::findRational(rn("1"), false, rn("2"), false));
    EXPECT_EQ(rn("30/19"), storm::utility::findRational(rn("11/7"), false, rn("19/12"), false));
    EXPECT_EQ(rn("11/7"), storm::utility::findRational(rn("11/7"), true, rn("19/12"), false));
    EXPECT_EQ(rn("19/12"), storm::utility::findRational(rn("11/7"), false, rn("19/12"), true));
    EXPECT_EQ(rn("1000/1001"), storm::utility::findRational(rn("999/1000"), false, rn("1"), false));
    EXPECT_EQ(rn("999/1000"), storm::utility::findRational(rn("999/1000"), true, rn("1"), false));
    EXPECT_EQ(rn("333/334"), storm::utility::findRational(rn("997/1000"), true, rn("1"), false));
    EXPECT_EQ(rn("1001/1000"), storm::utility::findRational(rn("1"), false, rn("1001/1000"), true));
    EXPECT_EQ(rn("1002/1001"), storm::utility::findRational(rn("1"), false, rn("1001/1000"), false));
    EXPECT_EQ(rn("335/334"), storm::utility::findRational(rn("1"), false, rn("1003/1000"), true));
    EXPECT_EQ(rn("500/1001"), storm::utility::findRational(rn("999/2000"), false, rn("1/2"), false));
    EXPECT_EQ(rn("167/335"), storm::utility::findRational(rn("997/2000"), true, rn("1/2"), false));
    EXPECT_EQ(rn("500/1001"), storm::utility::findRational(rn("500/1001"), true, rn("1/2"), false));
    EXPECT_EQ(rn("501/1001"), storm::utility::findRational(rn("1/2"), false, rn("501/1001"), true));
    EXPECT_EQ(rn("502/1003"), storm::utility::findRational(rn("1/2"), false, rn("501/1001"), false));
    EXPECT_EQ(rn("168/335"), storm::utility::findRational(rn("1/2"), false, rn("502/1001"), true));
}

TEST(RationalApproximationTest, negative) {
    EXPECT_EQ(rn("0"), storm::utility::findRational(rn("-1"), false, rn("1"), false));
    EXPECT_EQ(rn("0"), storm::utility::findRational(rn("-1"), true, rn("0"), true));
    EXPECT_EQ(rn("-1"), storm::utility::findRational(rn("-1"), true, rn("0"), false));
    EXPECT_EQ(rn("-30/19"), storm::utility::findRational(rn("-19/12"), false, rn("-11/7"), false));
    EXPECT_EQ(rn("-11/7"), storm::utility::findRational(rn("-19/12"), false, rn("-11/7"), true));
    EXPECT_EQ(rn("-19/12"), storm::utility::findRational(rn("-19/12"), true, rn("-11/7"), false));
}

}  // namespace
