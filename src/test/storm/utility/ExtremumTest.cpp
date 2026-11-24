#include "storm-config.h"
#include "test/storm_gtest.h"

#include <iostream>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"

namespace {
using storm::ExtendedRationalNumber;

storm::RationalNumber rational(double value) {
    return storm::utility::convertNumber<storm::RationalNumber>(value);
}
}  // namespace

TEST(ExtremumTest, emptyExtremumIsTheInfinityItStandsFor) {
    // The extremum over an empty set is +infinity if we minimize and -infinity if we maximize. That is a value like
    // any other, so reading it needs no special case.
    storm::utility::Minimum<storm::RationalNumber> minimum;
    EXPECT_TRUE(minimum.empty());
    EXPECT_TRUE(storm::utility::isInfinity(minimum.getExtendedValue()));

    storm::utility::Maximum<storm::RationalNumber> maximum;
    EXPECT_TRUE(maximum.empty());
    EXPECT_TRUE(storm::utility::isNegativeInfinity(maximum.getExtendedValue()));

    // A value type that brings its own infinity has always behaved this way.
    storm::utility::Minimum<double> doubleMinimum;
    EXPECT_TRUE(doubleMinimum.empty());
    EXPECT_TRUE(storm::utility::isInfinity(doubleMinimum.getExtendedValue()));
}

TEST(ExtremumTest, holdsAnInfiniteValue) {
    // A minimum that has been given +infinity and one that has been given nothing are the same thing: the minimum over
    // {+infinity} is +infinity, and so is the minimum over the empty set. What matters is that the value is right.
    storm::utility::Minimum<storm::RationalNumber> minimum;
    EXPECT_FALSE(minimum &= ExtendedRationalNumber::infinity());
    EXPECT_TRUE(storm::utility::isInfinity(minimum.getExtendedValue()));

    // The infinity that is not the base value is stored like any other value.
    storm::utility::Minimum<storm::RationalNumber> other;
    EXPECT_TRUE(other &= ExtendedRationalNumber::negativeInfinity());
    EXPECT_FALSE(other.empty());
    EXPECT_TRUE(storm::utility::isNegativeInfinity(other.getExtendedValue()));

    // Maximizing, +infinity is an improvement over anything and is kept.
    storm::utility::Maximum<storm::RationalNumber> maximum;
    EXPECT_TRUE(maximum &= rational(2));
    EXPECT_TRUE(maximum &= ExtendedRationalNumber::infinity());
    EXPECT_FALSE(maximum.empty());
    EXPECT_TRUE(storm::utility::isInfinity(maximum.getExtendedValue()));
    // Nothing finite improves upon it any more.
    EXPECT_FALSE(maximum &= rational(1000000));
}

TEST(ExtremumTest, ordinaryUseIsUnchanged) {
    storm::utility::Minimum<storm::RationalNumber> minimum;
    EXPECT_TRUE(minimum &= rational(5));
    EXPECT_FALSE(minimum.empty());
    EXPECT_EQ(rational(5), *minimum);
    EXPECT_TRUE(minimum.better(rational(3)));
    EXPECT_FALSE(minimum.better(rational(7)));
    EXPECT_TRUE(minimum &= rational(3));
    EXPECT_FALSE(minimum &= rational(7));
    EXPECT_EQ(rational(3), *minimum);
    ASSERT_TRUE(minimum.getOptionalValue().has_value());
    EXPECT_EQ(rational(3), minimum.getOptionalValue().value());

    minimum.reset();
    EXPECT_TRUE(minimum.empty());
    EXPECT_FALSE(minimum.getOptionalValue().has_value());

    // Combining two extrema takes the better of the two.
    storm::utility::Maximum<storm::RationalNumber> first(rational(2)), second(rational(4));
    EXPECT_TRUE(first &= second);
    EXPECT_EQ(rational(4), *first);
    EXPECT_FALSE(second &= storm::utility::Maximum<storm::RationalNumber>());
    EXPECT_EQ(rational(4), *second);
}

TEST(ExtremumTest, derefHasNoPlainValueForAnInfiniteOne) {
    // The plain value type has no infinity, so there is nothing for operator* to hand out. The extended value is the
    // way to read an empty extremum.
    storm::utility::Minimum<storm::RationalNumber> minimum;
    EXPECT_TRUE(storm::utility::isInfinity(minimum.getExtendedValue()));
}

TEST(ExtremumDeathTest, derefRefusesAnEmptyExtremum) {
    // Being non-empty is a precondition of operator*, so a violation is caught by an assertion rather than an
    // exception. It must not return the zero that an infinite value carries as its payload.
    storm::utility::Minimum<storm::RationalNumber> minimum;

#ifndef NDEBUG
    EXPECT_DEATH_IF_SUPPORTED(*minimum, "");
#else
    std::cerr << "WARNING: Not testing the operator* assertion, as it is disabled in release mode.\n";
    SUCCEED();
#endif
}
