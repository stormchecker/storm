#pragma once

#include <boost/random.hpp>
#include <cstdint>
#include <random>

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace utility {
template<typename ValueType>
class RandomProbabilityGenerator {
   public:
    RandomProbabilityGenerator();
    RandomProbabilityGenerator(uint64_t seed);
    ValueType random() const;
    uint64_t random_uint(uint64_t min, uint64_t max);
};

template<>
class RandomProbabilityGenerator<double> {
   public:
    RandomProbabilityGenerator();
    RandomProbabilityGenerator(uint64_t seed);
    double random();
    uint64_t random_uint(uint64_t min, uint64_t max);

   private:
    std::uniform_real_distribution<double> distribution;
    std::mt19937 engine;
};

template<>
class RandomProbabilityGenerator<storm::RationalNumber> {
   public:
    RandomProbabilityGenerator();
    RandomProbabilityGenerator(uint64_t seed);
    RationalNumber random();
    uint64_t random_uint(uint64_t min, uint64_t max);

   private:
    std::uniform_int_distribution<uint64_t> distribution;
    std::mt19937 engine;
};

class BernoulliDistributionGenerator {
   public:
    BernoulliDistributionGenerator(double prob);
    bool random(boost::mt19937& engine);

   private:
    boost::random::bernoulli_distribution<> distribution;
};

class ExponentialDistributionGenerator {
   public:
    ExponentialDistributionGenerator(double rate);
    double random(boost::mt19937& engine);

   private:
    boost::random::exponential_distribution<> distribution;
};

}  // namespace utility
}  // namespace storm