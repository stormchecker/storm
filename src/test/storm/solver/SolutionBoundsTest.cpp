#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/solver/SolutionBounds.h"

TEST(SolutionBoundsTest, PrecisionNeedsBothSides) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{0.5};
    EXPECT_FALSE(bounds.isWithinPrecision(1.0, false));
    EXPECT_FALSE(bounds.isWithinPrecision(1.0, true));
}

TEST(SolutionBoundsTest, AbsolutePrecisionComparesTheWidth) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{0.0, 1.0};
    bounds.upper = std::vector<double>{0.1, 1.05};
    EXPECT_TRUE(bounds.isWithinPrecision(0.1, false));
    EXPECT_FALSE(bounds.isWithinPrecision(0.04, false));
}

TEST(SolutionBoundsTest, RelativePrecisionScalesWithTheValue) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{10.0};
    bounds.upper = std::vector<double>{10.1};
    EXPECT_TRUE(bounds.isWithinPrecision(0.01, true));
    EXPECT_FALSE(bounds.isWithinPrecision(0.009, true));
    // The same width is far too wide to be a relative statement about a value this small.
    bounds.lower = std::vector<double>{0.001};
    bounds.upper = std::vector<double>{0.101};
    EXPECT_FALSE(bounds.isWithinPrecision(0.01, true));
}

TEST(SolutionBoundsTest, RelativePrecisionRejectsBoundsAroundZero) {
    storm::solver::SolutionBounds<double> bounds;
    bounds.lower = std::vector<double>{-0.001};
    bounds.upper = std::vector<double>{0.001};
    // The enclosed value may be zero, which no width but zero is within a relative precision of.
    EXPECT_FALSE(bounds.isWithinPrecision(0.5, true));
    EXPECT_TRUE(bounds.isWithinPrecision(0.01, false));
    bounds.setExact(std::vector<double>{0.0});
    EXPECT_TRUE(bounds.isWithinPrecision(0.5, true));
}
