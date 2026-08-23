#include "storm-config.h"
#include "test/storm_gtest.h"

#include <boost/math/distributions/weibull.hpp>

#include "storm-dft/storage/elements/BEConst.h"
#include "storm-dft/storage/elements/BEErlang.h"
#include "storm-dft/storage/elements/BEExponential.h"
#include "storm-dft/storage/elements/BELogNormal.h"
#include "storm-dft/storage/elements/BEProbability.h"
#include "storm-dft/storage/elements/BESamples.h"
#include "storm-dft/storage/elements/BEWeibull.h"

namespace {

TEST(DftBETest, FailureConstant) {
    storm::dft::storage::elements::BEConst<double> be(0, "TestBE", true);
    EXPECT_TRUE(be.failed());
    EXPECT_TRUE(be.canFail());

    EXPECT_EQ(1, be.getUnreliability(0));
    EXPECT_EQ(1, be.getUnreliability(10));

    storm::dft::storage::elements::BEConst<double> be2(0, "TestBE", false);
    EXPECT_FALSE(be2.failed());
    EXPECT_FALSE(be2.canFail());

    EXPECT_EQ(0, be2.getUnreliability(0));
    EXPECT_EQ(0, be2.getUnreliability(8));
}

TEST(DftBETest, FailureProbability) {
    storm::dft::storage::elements::BEProbability<double> be(0, "TestBE", 0.4, 0.5);

    EXPECT_TRUE(be.canFail());
    EXPECT_EQ(0.2, be.passiveFailureProbability());

    EXPECT_EQ(0.4, be.getUnreliability(0.4));
    EXPECT_EQ(0.4, be.getUnreliability(0.5));
    EXPECT_EQ(0.4, be.getUnreliability(1));
    EXPECT_EQ(0.4, be.getUnreliability(5));
}

TEST(DftBETest, FailureExponential) {
    storm::dft::storage::elements::BEExponential<double> be(0, "TestBE", 3, 0.5);

    EXPECT_TRUE(be.canFail());
    EXPECT_EQ(1.5, be.passiveFailureRate());

    EXPECT_EQ(0, be.getUnreliability(0));
    EXPECT_NEAR(0.7768698399, be.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.9502129316, be.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.9975212478, be.getUnreliability(2), 1e-10);
}

TEST(DftBETest, FailureErlang) {
    // Single phase
    storm::dft::storage::elements::BEErlang<double> be(0, "TestBE", 3, 1, 1);

    EXPECT_TRUE(be.canFail());
    EXPECT_EQ(3, be.passiveFailureRate());

    EXPECT_EQ(0, be.getUnreliability(0));
    EXPECT_NEAR(0.7768698399, be.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.9502129316, be.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.9975212478, be.getUnreliability(2), 1e-10);

    // Multiple phases
    storm::dft::storage::elements::BEErlang<double> be2(0, "TestBE", 3, 4, 1);

    EXPECT_TRUE(be2.canFail());
    EXPECT_EQ(3, be2.passiveFailureRate());

    EXPECT_EQ(0, be2.getUnreliability(0));
    EXPECT_NEAR(0.0656424544, be2.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.3527681112, be2.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.6577040442, be2.getUnreliability(1.5), 1e-10);
    EXPECT_NEAR(0.8487961172, be2.getUnreliability(2), 1e-10);
    EXPECT_NEAR(0.9997886215, be2.getUnreliability(5), 1e-10);
}

TEST(DftBETest, FailureWeibullExponential) {
    // Reduces to exponential distribution
    storm::dft::storage::elements::BEWeibull<double> be(0, "TestBE", 1, 1.0 / 3.0);

    EXPECT_TRUE(be.canFail());

    EXPECT_EQ(0, be.getUnreliability(0));
    EXPECT_NEAR(0.7768698399, be.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.9502129316, be.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.9975212478, be.getUnreliability(2), 1e-10);

    // Compare with boost results
    boost::math::weibull_distribution<double> dist(1, 1.0 / 3.0);
    for (double t = 0; t <= 5.0; t += 0.25) {
        EXPECT_NEAR(boost::math::cdf(dist, t), be.getUnreliability(t), 1e-10);
    }

    // Increasing failure rate
    storm::dft::storage::elements::BEWeibull<double> be2(0, "TestBE", 2, 2);

    EXPECT_TRUE(be2.canFail());

    EXPECT_EQ(0, be2.getUnreliability(0));
    EXPECT_NEAR(0.0605869372, be2.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.2211992169, be2.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.6321205588, be2.getUnreliability(2), 1e-10);
    EXPECT_NEAR(0.9980695458, be2.getUnreliability(5), 1e-10);

    // Compare with boost results
    boost::math::weibull_distribution<double> dist2(2, 2);
    for (double t = 0; t <= 5.0; t += 0.25) {
        EXPECT_NEAR(boost::math::cdf(dist2, t), be2.getUnreliability(t), 1e-10);
    }

    // Decreasing failure rate
    storm::dft::storage::elements::BEWeibull<double> be3(0, "TestBE", 0.4, 2);

    EXPECT_TRUE(be3.canFail());

    EXPECT_EQ(0, be3.getUnreliability(0));
    EXPECT_NEAR(0.4369287910, be3.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.5313308906, be3.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.6321205588, be3.getUnreliability(2), 1e-10);
    EXPECT_NEAR(0.7637110612, be3.getUnreliability(5), 1e-10);

    // Compare with boost results
    boost::math::weibull_distribution<double> dist3(0.4, 2);
    for (double t = 0; t <= 5.0; t += 0.25) {
        EXPECT_NEAR(boost::math::cdf(dist3, t), be3.getUnreliability(t), 1e-10);
    }
}

TEST(DftBETest, FailureLogNormal) {
    // First distribution
    storm::dft::storage::elements::BELogNormal<double> be(0, "TestBE", 0, 0.5);

    EXPECT_TRUE(be.canFail());

    EXPECT_EQ(0, be.getUnreliability(0));
    EXPECT_NEAR(0.0828285190, be.getUnreliability(0.5), 1e-10);
    EXPECT_NEAR(0.5, be.getUnreliability(1), 1e-10);
    EXPECT_NEAR(0.9171714810, be.getUnreliability(2), 1e-10);
    EXPECT_NEAR(0.9993565290, be.getUnreliability(5), 1e-10);

    // Second distribution
    storm::dft::storage::elements::BELogNormal<double> be2(0, "TestBE", 1, 0.25);

    EXPECT_TRUE(be2.canFail());

    EXPECT_EQ(0, be2.getUnreliability(0));
    EXPECT_NEAR(6.32491e-12, be2.getUnreliability(0.5), 1e-17);
    EXPECT_NEAR(3.167124183e-5, be2.getUnreliability(1), 1e-14);
    EXPECT_NEAR(0.1098340249, be2.getUnreliability(2), 1e-10);
    EXPECT_NEAR(0.9926105382, be2.getUnreliability(5), 1e-10);
}

TEST(DftBETest, FailureSamples) {
    // Weibull distribution with shape 5 and scale 1
    std::map<double, double> samples = {{0.0, 0.0},
                                        {0.25, 0.0009760858180243304},
                                        {0.5, 0.03076676552365587},
                                        {0.75, 0.21124907114638225},
                                        {1.0, 0.6321205588285577},
                                        {1.25, 0.9527242505937095},
                                        {1.5, 0.9994964109502631},
                                        {1.75, 0.999999925546118},
                                        {2.0, 0.9999999999999873}};
    storm::dft::storage::elements::BESamples<double> be(0, "TestBE", samples);

    EXPECT_TRUE(be.canFail());

    // Compare with boost results
    boost::math::weibull_distribution<double> dist(5, 1);
    for (double t = 0; t <= 2.0; t += 0.25) {
        EXPECT_NEAR(boost::math::cdf(dist, t), be.getUnreliability(t), 1e-10);
    }
}

}  // namespace
