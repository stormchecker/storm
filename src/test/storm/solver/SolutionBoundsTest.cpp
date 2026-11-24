#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/SolutionBounds.h"
#include "storm/utility/ExtendedNumber.h"

TEST(SolutionBoundsTest, PrecisionNeedsBothSides) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{0.5};
    EXPECT_FALSE(bounds.isWithinPrecision({0.5}, 1.0, false));
    EXPECT_FALSE(bounds.isWithinPrecision({0.5}, 1.0, true));
}

TEST(SolutionBoundsTest, PrecisionRejectsMismatchingSizes) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.setExact(std::vector<double>{0.5});
    STORM_SILENT_EXPECT_THROW(bounds.isWithinPrecision({0.5, 0.5}, 1.0, false), storm::exceptions::InvalidArgumentException);
}

TEST(SolutionBoundsTest, AbsolutePrecisionComparesTheDistanceToTheValue) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{0.0, 1.0};
    bounds.upper = std::vector<double>{0.1, 1.05};
    // Both values sit at the midpoint, so the distance is half of the width.
    EXPECT_TRUE(bounds.isWithinPrecision({0.05, 1.025}, 0.05, false));
    EXPECT_FALSE(bounds.isWithinPrecision({0.05, 1.025}, 0.04, false));
    // A value off center is only as precise as its farther side allows.
    EXPECT_FALSE(bounds.isWithinPrecision({0.1, 1.025}, 0.05, false));
}

TEST(SolutionBoundsTest, RelativePrecisionScalesWithTheValue) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{10.0};
    bounds.upper = std::vector<double>{10.1};
    EXPECT_TRUE(bounds.isWithinPrecision({10.05}, 0.01, true));
    EXPECT_FALSE(bounds.isWithinPrecision({10.05}, 0.004, true));
    // The same distance is far too large to be a relative statement about a value this small.
    bounds.lower = std::vector<double>{0.001};
    bounds.upper = std::vector<double>{0.101};
    EXPECT_FALSE(bounds.isWithinPrecision({0.051}, 0.005, true));
}

TEST(SolutionBoundsTest, RelativePrecisionRejectsBoundsAroundZero) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{-0.001};
    bounds.upper = std::vector<double>{0.001};
    // The enclosed value may be zero, which no distance but zero is within a relative precision of.
    EXPECT_FALSE(bounds.isWithinPrecision({0.0}, 0.5, true));
    EXPECT_TRUE(bounds.isWithinPrecision({0.0}, 0.01, false));
    bounds.setExact(std::vector<double>{0.0});
    EXPECT_TRUE(bounds.isWithinPrecision({0.0}, 0.5, true));
}

TEST(SolutionBoundsTest, RelativePrecisionWithExtendedRationalNumbers) {
    using storm::ExtendedRationalNumber;
    auto const rational = [](int numerator, int denominator) {
        return ExtendedRationalNumber(storm::utility::convertNumber<storm::RationalNumber>(numerator) /
                                      storm::utility::convertNumber<storm::RationalNumber>(denominator));
    };
    storm::solver::SolutionBounds<ExtendedRationalNumber> bounds;
    bounds.lower = std::vector<ExtendedRationalNumber>{rational(10, 1), rational(-101, 10)};
    bounds.upper = std::vector<ExtendedRationalNumber>{rational(101, 10), rational(-10, 1)};
    // Negative bounds are scaled by their absolute value just like positive ones.
    EXPECT_TRUE(bounds.isWithinPrecision({rational(201, 20), rational(-201, 20)}, rational(1, 100), true));
    EXPECT_FALSE(bounds.isWithinPrecision({rational(201, 20), rational(-201, 20)}, rational(1, 250), true));
    // An infinite bound is never within a finite distance of the value.
    bounds.upper = std::vector<ExtendedRationalNumber>{ExtendedRationalNumber::infinity(), rational(-10, 1)};
    EXPECT_FALSE(bounds.isWithinPrecision({rational(201, 20), rational(-201, 20)}, rational(1, 100), true));
}
