#include "storm-config.h"
#include "test/storm_gtest.h"

#include <iostream>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/constants.h"

namespace {
using storm::ExtendedRationalNumber;

storm::RationalNumber rational(double value) {
    return storm::utility::convertNumber<storm::RationalNumber>(value);
}
}  // namespace

TEST(ExtendedNumberTest, extendedValueTypeOnlyWrapsWhereNeeded) {
    // double has its own infinity, so it must not be wrapped: the floating point path pays nothing for this.
    EXPECT_TRUE((std::is_same_v<storm::utility::ExtendedValueType<double>, double>));
    // A rational number has none, so it is wrapped, and that is what the ExtendedRationalNumber alias names.
    EXPECT_TRUE((std::is_same_v<storm::ExtendedRationalNumber, storm::utility::ExtendedNumber<storm::RationalNumber>>));
    // The same holds for a rational function, which the ExtendedRationalFunction alias names.
    EXPECT_TRUE((std::is_same_v<storm::ExtendedRationalFunction, storm::utility::ExtendedNumber<storm::RationalFunction>>));
}

TEST(ExtendedNumberTest, kinds) {
    ExtendedRationalNumber const two(rational(2));
    EXPECT_TRUE(two.isFinite());
    EXPECT_FALSE(two.isInfinite());
    EXPECT_EQ(rational(2), two.getFinite());

    EXPECT_TRUE(ExtendedRationalNumber::infinity().isPositiveInfinity());
    EXPECT_TRUE(ExtendedRationalNumber::infinity().isInfinite());
    EXPECT_TRUE(ExtendedRationalNumber::negativeInfinity().isNegativeInfinity());
    EXPECT_FALSE(ExtendedRationalNumber::negativeInfinity().isPositiveInfinity());

    // The default value is a finite zero.
    EXPECT_TRUE(ExtendedRationalNumber().isFinite());
    EXPECT_TRUE(storm::utility::isZero(ExtendedRationalNumber().getFinite()));
}

TEST(ExtendedNumberTest, ordering) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();
    ExtendedRationalNumber const two(rational(2));

    EXPECT_LT(negInf, two);
    EXPECT_LT(two, inf);
    EXPECT_LT(negInf, inf);
    EXPECT_LE(inf, inf);
    EXPECT_GT(inf, two);

    // The previous representation of infinity was the literal 100000000000, so any larger value compared greater than
    // "infinity". Every finite value must now be below it.
    EXPECT_LT(ExtendedRationalNumber(rational(1e12)), inf);
    EXPECT_LT(ExtendedRationalNumber(rational(1e11)), inf);
}

TEST(ExtendedNumberTest, equality) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    EXPECT_EQ(inf, ExtendedRationalNumber::infinity());
    EXPECT_NE(inf, ExtendedRationalNumber::negativeInfinity());
    EXPECT_NE(inf, ExtendedRationalNumber(rational(1e11)));
    EXPECT_EQ(ExtendedRationalNumber(rational(2)), ExtendedRationalNumber(rational(2)));
}

TEST(ExtendedNumberTest, arithmeticIsAbsorbing) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();
    ExtendedRationalNumber const two(rational(2));
    ExtendedRationalNumber const zero;

    EXPECT_EQ(inf, inf + two);
    EXPECT_EQ(inf, two + inf);
    EXPECT_EQ(inf, inf - two);
    EXPECT_EQ(inf, inf + inf);
    EXPECT_EQ(inf, inf * two);
    EXPECT_EQ(negInf, inf * -two);
    EXPECT_EQ(inf, inf / two);
    EXPECT_EQ(negInf, inf / -two);
    EXPECT_EQ(zero, two / inf);
    EXPECT_EQ(negInf, -inf);
    EXPECT_EQ(inf, -negInf);

    EXPECT_EQ(ExtendedRationalNumber(rational(6)), ExtendedRationalNumber(rational(2)) * ExtendedRationalNumber(rational(3)));
}

TEST(ExtendedNumberTest, undefinedFormsThrow) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();
    ExtendedRationalNumber const zero;

    // The exact value types have no NaN, so these throw rather than producing a quiet junk value.
    STORM_SILENT_EXPECT_THROW(inf - inf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(inf + negInf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(inf * zero, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(inf / inf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(inf / zero, storm::exceptions::InvalidOperationException);
}

TEST(ExtendedNumberTest, conversionAcrossValueTypes) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();

    // Into a type that has its own infinity.
    EXPECT_EQ(storm::utility::infinity<double>(), storm::utility::convertNumber<double>(inf));
    EXPECT_EQ(-storm::utility::infinity<double>(), storm::utility::convertNumber<double>(negInf));
    EXPECT_EQ(2.0, storm::utility::convertNumber<double>(ExtendedRationalNumber(rational(2))));

    // Into another extended type. This is what the parameter lifting model checker needs, where infinity has to travel
    // from the constant type to the coefficient type.
    auto const asFunction = storm::utility::convertNumber<storm::ExtendedRationalFunction>(inf);
    EXPECT_TRUE(asFunction.isPositiveInfinity());
    auto const finiteAsFunction = storm::utility::convertNumber<storm::ExtendedRationalFunction>(ExtendedRationalNumber(rational(2)));
    EXPECT_TRUE(finiteAsFunction.isFinite());
}

TEST(ExtendedNumberTest, output) {
    std::stringstream stream;
    stream << ExtendedRationalNumber::infinity() << " " << ExtendedRationalNumber::negativeInfinity() << " " << ExtendedRationalNumber(rational(2));
    EXPECT_EQ("inf -inf 2", stream.str());
}

TEST(ExtendedNumberTest, mixesWithFiniteValues) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    storm::RationalNumber const two = rational(2);

    // The operators are hidden friends, so a finite value converts on either side of them.
    EXPECT_TRUE(two < inf);
    EXPECT_TRUE(inf > two);
    EXPECT_EQ(ExtendedRationalNumber(rational(4)), two + ExtendedRationalNumber(two));
    EXPECT_EQ(ExtendedRationalNumber(rational(4)), ExtendedRationalNumber(two) + two);
    EXPECT_EQ(inf, two + inf);
    EXPECT_EQ(ExtendedRationalNumber(two), two);
}

TEST(ExtendedNumberTest, reportsItsInfinityToTheGenericUtilities) {
    // NumberTraits and numeric_limits both have to say that this type does have an infinity, otherwise the generic
    // storm::utility::infinity and isInfinity would not reach it.
    static_assert(storm::NumberTraits<ExtendedRationalNumber>::HasInfinity);
    static_assert(storm::NumberTraits<ExtendedRationalNumber>::IsExact == storm::NumberTraits<storm::RationalNumber>::IsExact);
    EXPECT_TRUE(std::numeric_limits<ExtendedRationalNumber>::has_infinity);
    EXPECT_EQ(ExtendedRationalNumber::infinity(), std::numeric_limits<ExtendedRationalNumber>::infinity());

    // Extending an already extended type is idempotent.
    EXPECT_TRUE((std::is_same_v<storm::utility::ExtendedValueType<ExtendedRationalNumber>, ExtendedRationalNumber>));
    EXPECT_TRUE((std::is_same_v<storm::utility::FiniteValueType<ExtendedRationalNumber>, storm::RationalNumber>));
    EXPECT_TRUE((std::is_same_v<storm::utility::FiniteValueType<double>, double>));
}

TEST(ExtendedNumberTest, conversionIntoAnExtendedType) {
    auto const widened = storm::utility::convertNumber<ExtendedRationalNumber>(2.0);
    EXPECT_TRUE(widened.isFinite());
    EXPECT_EQ(rational(2), widened.getFinite());
}

TEST(ExtendedNumberTest, infinityOfTheExtendedType) {
    EXPECT_EQ(ExtendedRationalNumber::infinity(), storm::utility::positiveInfinity<storm::RationalNumber>());
    EXPECT_EQ(ExtendedRationalNumber::negativeInfinity(), storm::utility::negativeInfinity<storm::RationalNumber>());

    // For a type that has its own infinity nothing is wrapped, so this stays the IEEE infinity.
    EXPECT_EQ(storm::utility::infinity<double>(), storm::utility::positiveInfinity<double>());
    EXPECT_EQ(-storm::utility::infinity<double>(), storm::utility::negativeInfinity<double>());
}

TEST(ExtendedNumberTest, comparesAgainstAPlainValue) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();
    ExtendedRationalNumber const two(rational(2));
    storm::RationalNumber const plainTwo = rational(2);
    storm::RationalNumber const plainThree = rational(3);

    // A finite extended value compares as the value it holds, in both argument orders.
    EXPECT_TRUE(two == plainTwo);
    EXPECT_TRUE(plainTwo == two);
    EXPECT_FALSE(two != plainTwo);
    EXPECT_TRUE(two < plainThree);
    EXPECT_TRUE(plainTwo < ExtendedRationalNumber(plainThree));
    EXPECT_TRUE(two <= plainTwo);
    EXPECT_TRUE(two >= plainTwo);
    EXPECT_TRUE(ExtendedRationalNumber(plainThree) > plainTwo);
    EXPECT_TRUE(plainThree > two);

    // Every plain value lies strictly between the two infinities.
    EXPECT_TRUE(negInf < plainTwo);
    EXPECT_TRUE(plainTwo < inf);
    EXPECT_FALSE(inf < plainTwo);
    EXPECT_FALSE(plainTwo < negInf);
    EXPECT_TRUE(inf > plainTwo);
    EXPECT_TRUE(plainTwo > negInf);
    EXPECT_TRUE(negInf <= plainTwo);
    EXPECT_TRUE(inf >= plainTwo);

    // An infinity is equal to no plain value at all.
    EXPECT_FALSE(inf == plainTwo);
    EXPECT_FALSE(negInf == plainTwo);
    EXPECT_TRUE(inf != plainTwo);
    EXPECT_TRUE(plainTwo != negInf);

    // The mixed comparisons must agree with the ones that go through the conversion, which is what they replace.
    for (auto const& plain : {rational(-1), rational(0), rational(2), rational(1000)}) {
        for (auto const& extended : {inf, negInf, two}) {
            EXPECT_EQ(extended < ExtendedRationalNumber(plain), extended < plain);
            EXPECT_EQ(ExtendedRationalNumber(plain) < extended, plain < extended);
            EXPECT_EQ(extended == ExtendedRationalNumber(plain), extended == plain);
            EXPECT_EQ(extended <= ExtendedRationalNumber(plain), extended <= plain);
            EXPECT_EQ(extended > ExtendedRationalNumber(plain), extended > plain);
            EXPECT_EQ(extended >= ExtendedRationalNumber(plain), extended >= plain);
        }
    }
}

TEST(ExtendedNumberTest, negativeInfinityArithmetic) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();
    ExtendedRationalNumber const zero;
    ExtendedRationalNumber const two(rational(2));

    EXPECT_EQ(negInf, negInf + negInf);
    EXPECT_EQ(negInf, negInf + two);
    EXPECT_EQ(negInf, negInf - two);
    EXPECT_EQ(negInf, negInf - inf);
    EXPECT_EQ(inf, negInf * negInf);
    EXPECT_EQ(negInf, negInf * two);
    EXPECT_EQ(negInf, negInf * inf);
    EXPECT_EQ(inf, -negInf);
    EXPECT_EQ(negInf, -inf);
    EXPECT_EQ(zero, two / negInf);

    // The undefined forms are undefined in this direction too.
    STORM_SILENT_EXPECT_THROW(negInf - negInf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(negInf + inf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(negInf * zero, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(negInf / negInf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(negInf / inf, storm::exceptions::InvalidOperationException);
    STORM_SILENT_EXPECT_THROW(negInf / zero, storm::exceptions::InvalidOperationException);
}

TEST(ExtendedNumberTest, reportsNegativeInfinity) {
    EXPECT_TRUE(storm::utility::isNegativeInfinity(ExtendedRationalNumber::negativeInfinity()));
    EXPECT_FALSE(storm::utility::isNegativeInfinity(ExtendedRationalNumber::infinity()));
    EXPECT_FALSE(storm::utility::isNegativeInfinity(ExtendedRationalNumber(rational(-2))));

    // The same question is answerable for a type that brings its own infinity.
    EXPECT_TRUE(storm::utility::isNegativeInfinity(-storm::utility::infinity<double>()));
    EXPECT_FALSE(storm::utility::isNegativeInfinity(storm::utility::infinity<double>()));
    EXPECT_FALSE(storm::utility::isNegativeInfinity(-2.0));
    // A value type without an infinity of its own has no negative infinity to report.
    EXPECT_FALSE(storm::utility::isNegativeInfinity(rational(-2)));
}

TEST(ExtendedNumberTest, isFiniteRejectsNotANumber) {
    EXPECT_TRUE(storm::utility::isFinite(2.0));
    EXPECT_FALSE(storm::utility::isFinite(storm::utility::infinity<double>()));
    EXPECT_FALSE(storm::utility::isFinite(-storm::utility::infinity<double>()));
    // A NaN is not an infinity, but it is not a finite value either. The guards built on this predicate exist to keep
    // a value that is not a number out of a computation.
    EXPECT_FALSE(storm::utility::isFinite(std::nan("")));

    EXPECT_TRUE(storm::utility::isFinite(ExtendedRationalNumber(rational(2))));
    EXPECT_FALSE(storm::utility::isFinite(ExtendedRationalNumber::infinity()));
    EXPECT_FALSE(storm::utility::isFinite(ExtendedRationalNumber::negativeInfinity()));
}

TEST(ExtendedNumberTest, widenDoesNotInterpretTheSentinel) {
    // widen is for values that cannot be infinite. It must take the sentinel at face value: reading it as an infinity
    // would be guesswork, and the sentinel is a perfectly ordinary number that a computation may legitimately produce.
    storm::RationalNumber const sentinel = storm::utility::infinity<storm::RationalNumber>();
    ExtendedRationalNumber const widened(sentinel);
    EXPECT_TRUE(widened.isFinite());
    EXPECT_EQ(sentinel, widened.getFinite());

    // fromSentinel is the one that translates it, and it is the only one that may be used on a vector that came out of
    // a part of Storm that still produces the sentinel.
    EXPECT_TRUE(storm::utility::fromSentinel(sentinel).isPositiveInfinity());

    // For a type that has its own infinity the sentinel is that infinity, so fromSentinel is the identity there.
    EXPECT_TRUE(storm::utility::isInfinity(storm::utility::fromSentinel(storm::utility::infinity<double>())));
    EXPECT_TRUE(storm::utility::isInfinity(storm::utility::widen(std::vector<double>{storm::utility::infinity<double>()}).front()));
}

TEST(ExtendedNumberTest, widenAndNarrowVectors) {
    std::vector<storm::RationalNumber> const finite{rational(1), rational(2)};
    auto const widened = storm::utility::widen(std::vector<storm::RationalNumber>(finite));
    ASSERT_EQ(2ull, widened.size());
    EXPECT_EQ(ExtendedRationalNumber(rational(1)), widened[0]);
    EXPECT_EQ(ExtendedRationalNumber(rational(2)), widened[1]);
    EXPECT_EQ(finite, storm::utility::narrowFinite<storm::RationalNumber>(std::vector<ExtendedRationalNumber>(widened)));

    std::vector<storm::RationalNumber> const withSentinel{rational(1), storm::utility::infinity<storm::RationalNumber>()};
    auto const translated = storm::utility::fromSentinel(std::vector<storm::RationalNumber>(withSentinel));
    ASSERT_EQ(2ull, translated.size());
    EXPECT_TRUE(translated[0].isFinite());
    EXPECT_TRUE(translated[1].isPositiveInfinity());

    // narrowFinite has no number to put in place of an infinity, so it refuses rather than inventing one. This has to
    // hold in a release build too, which is why it throws rather than asserting.
    STORM_SILENT_EXPECT_THROW(storm::utility::narrowFinite<storm::RationalNumber>(std::vector<ExtendedRationalNumber>(translated)),
                              storm::exceptions::InvalidOperationException);
}

TEST(ExtendedNumberTest, narrowsToADefaultValue) {
    std::vector<ExtendedRationalNumber> const values{ExtendedRationalNumber(rational(1)), ExtendedRationalNumber::infinity(),
                                                     ExtendedRationalNumber::negativeInfinity()};
    std::vector<storm::RationalNumber> const narrowed =
        storm::utility::narrowFinite<storm::RationalNumber>(std::vector<ExtendedRationalNumber>(values), rational(7));
    ASSERT_EQ(3ull, narrowed.size());
    EXPECT_EQ(rational(1), narrowed[0]);
    EXPECT_EQ(rational(7), narrowed[1]);
    EXPECT_EQ(rational(7), narrowed[2]);

    // A value type that brings its own infinity keeps the default in the same places.
    std::vector<double> const doubles = storm::utility::narrowFinite<double>(std::vector<double>{1.0, storm::utility::infinity<double>()}, 7.0);
    ASSERT_EQ(2ull, doubles.size());
    EXPECT_EQ(1.0, doubles[0]);
    EXPECT_EQ(7.0, doubles[1]);
}

TEST(ExtendedNumberTest, narrowingRefusesAnInfiniteValue) {
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();
    ExtendedRationalNumber const negInf = ExtendedRationalNumber::negativeInfinity();

    EXPECT_EQ(rational(2), storm::utility::narrow<storm::RationalNumber>(ExtendedRationalNumber(rational(2))));
    STORM_SILENT_EXPECT_THROW(storm::utility::narrow<storm::RationalNumber>(inf), storm::exceptions::NotSupportedException);
    STORM_SILENT_EXPECT_THROW(storm::utility::narrow<storm::RationalNumber>(negInf), storm::exceptions::NotSupportedException);

    EXPECT_EQ(rational(2), storm::utility::getFinite(ExtendedRationalNumber(rational(2))));

    // A type that has its own infinity narrows to itself, infinity included.
    EXPECT_TRUE(storm::utility::isInfinity(storm::utility::narrow<double>(storm::utility::infinity<double>())));
}

TEST(ExtendedNumberDeathTest, getFiniteRefusesAnInfiniteValue) {
    // Being finite is a precondition of getFinite, so a violation is caught by an assertion rather than an exception.
    // It must not quietly hand out the zero that an infinite value happens to carry as its payload.
    ExtendedRationalNumber const inf = ExtendedRationalNumber::infinity();

#ifndef NDEBUG
    EXPECT_DEATH_IF_SUPPORTED(storm::utility::getFinite(inf), "");
    EXPECT_DEATH_IF_SUPPORTED(inf.getFinite(), "");
#else
    std::cerr << "WARNING: Not testing the getFinite assertion, as it is disabled in release mode.\n";
    SUCCEED();
#endif
}

TEST(ExtendedNumberTest, toSentinelHasNoNegativeInfinity) {
    EXPECT_EQ(rational(2), storm::utility::toSentinel<storm::RationalNumber>(ExtendedRationalNumber(rational(2))));
    EXPECT_EQ(storm::utility::infinity<storm::RationalNumber>(), storm::utility::toSentinel<storm::RationalNumber>(ExtendedRationalNumber::infinity()));

    // The sentinel is a single magic number with no negative counterpart, so this direction is rejected rather than
    // silently turned into something else.
    STORM_SILENT_EXPECT_THROW(storm::utility::toSentinel<storm::RationalNumber>(ExtendedRationalNumber::negativeInfinity()),
                              storm::exceptions::NotSupportedException);

    // Round tripping through the sentinel is lossless for the one infinity it can represent.
    EXPECT_TRUE(storm::utility::fromSentinel(storm::utility::toSentinel<storm::RationalNumber>(ExtendedRationalNumber::infinity())).isPositiveInfinity());
}

TEST(ExtendedNumberTest, convertsAnInfinityFromATypeThatHasItsOwn) {
    // This is the path that carries an infinity out of a double computation into an exact coefficient type.
    auto const fromDouble = storm::utility::convertNumber<ExtendedRationalNumber>(storm::utility::infinity<double>());
    EXPECT_TRUE(fromDouble.isPositiveInfinity());
    auto const negFromDouble = storm::utility::convertNumber<ExtendedRationalNumber>(-storm::utility::infinity<double>());
    EXPECT_TRUE(negFromDouble.isNegativeInfinity());
    EXPECT_EQ(rational(0.5), storm::utility::convertNumber<ExtendedRationalNumber>(0.5).getFinite());
}
