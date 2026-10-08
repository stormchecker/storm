#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <random>
#include <vector>

#include "storm/generator/CompressedState.h"
#include "storm/generator/VariableInformation.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

// Creates variables with a random layout: Variables are placed consecutively (so many of them cross bucket boundaries)
storm::generator::VariableInformation createRandomLayout(storm::expressions::ExpressionManager& manager, std::mt19937_64& rng, uint64_t targetNumberOfBits) {
    storm::generator::VariableInformation info;
    uint64_t offset = 0;
    uint64_t index = 0;
    while (offset < targetNumberOfBits) {
        auto const name = "v" + std::to_string(index++);
        switch (rng() % 3) {
            case 0: {
                info.booleanVariables.emplace_back(manager.declareBooleanVariable(name), offset, false, true);
                offset += 1;
                break;
            }
            case 1: {
                // Width 0 is possible for locations with a single value.
                uint64_t const width = rng() % 9;
                info.locationVariables.emplace_back(manager.declareIntegerVariable(name), (uint64_t(1) << width) - 1, offset, width, true);
                offset += width;
                break;
            }
            default: {
                // Widths up to 52 bits are exactly representable by the (double based) evaluator
                uint64_t const width = 1 + rng() % 52;
                int64_t const lowerBound = static_cast<int64_t>(rng() % 20) - 10;
                info.integerVariables.emplace_back(manager.declareIntegerVariable(name), lowerBound,
                                                   lowerBound + static_cast<int64_t>((uint64_t(1) << width) - 1), offset, width);
                offset += width;
                break;
            }
        }
    }
    info.totalBitOffset = offset;
    return info;
}

storm::generator::CompressedState randomState(std::mt19937_64& rng, uint64_t numberOfBits) {
    storm::generator::CompressedState state(numberOfBits);
    for (uint64_t i = 0; i < numberOfBits; ++i) {
        state.set(i, rng() % 2 == 0);
    }
    return state;
}

// Makes a state that agrees with the given one except for a few random bits
storm::generator::CompressedState perturbState(storm::generator::CompressedState state, std::mt19937_64& rng, uint64_t numberOfFlips) {
    for (uint64_t i = 0; i < numberOfFlips; ++i) {
        uint64_t const bit = rng() % state.size();
        state.set(bit, !state.get(bit));
    }
    return state;
}

void expectSameValues(storm::generator::VariableInformation const& info, storm::expressions::ExpressionEvaluator<double> const& expected,
                      storm::expressions::ExpressionEvaluator<double> const& actual) {
    for (auto const& v : info.locationVariables) {
        EXPECT_EQ(expected.asInt(v.variable.getExpression()), actual.asInt(v.variable.getExpression())) << v.variable.getName();
    }
    for (auto const& v : info.booleanVariables) {
        EXPECT_EQ(expected.asBool(v.variable.getExpression()), actual.asBool(v.variable.getExpression())) << v.variable.getName();
    }
    for (auto const& v : info.integerVariables) {
        EXPECT_EQ(expected.asInt(v.variable.getExpression()), actual.asInt(v.variable.getExpression())) << v.variable.getName();
    }
}

}  // namespace

TEST(CompressedStateTest, UnpackStateDifferenceIntoEvaluator) {
    std::mt19937_64 rng(1234);
    // The bit sizes cover a single bucket, a few buckets and more than 16 buckets (which requires heap memory).
    for (uint64_t targetBits : {10ul, 64ul, 100ul, 300ul, 1200ul}) {
        for (int layoutIteration = 0; layoutIteration < 20; ++layoutIteration) {
            auto manager = std::make_shared<storm::expressions::ExpressionManager>();
            auto const info = createRandomLayout(*manager, rng, targetBits);
            // The evaluators must be created after declaring all variables
            storm::expressions::ExpressionEvaluator<double> fullyUnpacked(*manager);
            storm::expressions::ExpressionEvaluator<double> incremental(*manager);

            auto current = randomState(rng, info.totalBitOffset);
            storm::generator::unpackStateIntoEvaluator<double>(current, info, incremental);
            for (int step = 0; step < 30; ++step) {
                storm::generator::CompressedState next;
                switch (step % 4) {
                    case 0:
                        next = current;  // no difference at all
                        break;
                    case 1:
                        next = perturbState(current, rng, 1);
                        break;
                    case 2:
                        next = perturbState(current, rng, 5);
                        break;
                    default:
                        next = randomState(rng, info.totalBitOffset);
                        break;
                }
                storm::generator::unpackStateDifferenceIntoEvaluator<double>(next, current, info, incremental);
                storm::generator::unpackStateIntoEvaluator<double>(next, info, fullyUnpacked);
                expectSameValues(info, fullyUnpacked, incremental);
                if (::testing::Test::HasFailure()) {
                    return;
                }
                current = next;
            }
        }
    }
}
