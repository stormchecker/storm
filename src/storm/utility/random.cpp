#include "random.h"

#include <limits>

namespace storm {
namespace utility {
RandomProbabilityGenerator<double>::RandomProbabilityGenerator() : distribution(0.0, 1.0) {
    std::random_device rd;
    engine = std::mt19937(rd());
}

RandomProbabilityGenerator<double>::RandomProbabilityGenerator(uint64_t seed) : distribution(0.0, 1.0), engine(seed) {}

double RandomProbabilityGenerator<double>::random() {
    return distribution(engine);
}

uint64_t RandomProbabilityGenerator<double>::random_uint(uint64_t min, uint64_t max) {
    return std::uniform_int_distribution<uint64_t>(min, max)(engine);
}

RandomProbabilityGenerator<RationalNumber>::RandomProbabilityGenerator() : distribution(0, std::numeric_limits<uint64_t>::max()) {
    std::random_device rd;
    engine = std::mt19937(rd());
}

RandomProbabilityGenerator<RationalNumber>::RandomProbabilityGenerator(uint64_t seed) : distribution(0, std::numeric_limits<uint64_t>::max()), engine(seed) {}

RationalNumber RandomProbabilityGenerator<RationalNumber>::random() {
    return carl::rationalize<RationalNumber>(distribution(engine)) / carl::rationalize<RationalNumber>(std::numeric_limits<uint64_t>::max());
}

uint64_t RandomProbabilityGenerator<RationalNumber>::random_uint(uint64_t min, uint64_t max) {
    return std::uniform_int_distribution<uint64_t>(min, max)(engine);
}

BernoulliDistributionGenerator::BernoulliDistributionGenerator(double prob) : distribution(prob) {}

bool BernoulliDistributionGenerator::random(boost::mt19937& engine) {
    return distribution(engine);
}

ExponentialDistributionGenerator::ExponentialDistributionGenerator(double rate) : distribution(rate) {}

double ExponentialDistributionGenerator::random(boost::mt19937& engine) {
    return distribution(engine);
}
}  // namespace utility
}  // namespace storm
