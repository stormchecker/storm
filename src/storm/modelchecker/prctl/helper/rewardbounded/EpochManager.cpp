#include "storm/modelchecker/prctl/helper/rewardbounded/EpochManager.h"

#include "storm/utility/macros.h"

#include "storm/exceptions/IllegalArgumentException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

EpochManager::EpochManager() : dimensionCount(0) {
    // Intentionally left empty
}

EpochManager::EpochManager(uint64_t dimensionCount) : dimensionCount(dimensionCount) {
    STORM_LOG_THROW(dimensionCount > 0, storm::exceptions::IllegalArgumentException, "Invoked EpochManager with zero dimension count.");
    STORM_LOG_THROW(dimensionCount <= 64, storm::exceptions::IllegalArgumentException, "Invoked EpochManager with too many dimensions.");
    bitsPerDimension = 64 / dimensionCount;
    if (dimensionCount == 1) {
        dimensionBitMask = -1ull;
    } else {
        dimensionBitMask = (1ull << bitsPerDimension) - 1;
    }

    if (dimensionCount * bitsPerDimension == 64ull) {
        relevantBitsMask = -1ull;
    } else {
        relevantBitsMask = (1ull << (dimensionCount * bitsPerDimension)) - 1;
    }
}

typename EpochManager::Epoch EpochManager::getBottomEpoch() const {
    return relevantBitsMask;
}

typename EpochManager::Epoch EpochManager::getZeroEpoch() const {
    return 0;
}

uint64_t const& EpochManager::getDimensionCount() const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return dimensionCount;
}

bool EpochManager::compareEpochClass(Epoch const& epoch1, Epoch const& epoch2) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    uint64_t mask = dimensionBitMask;
    for (uint64_t d = 0; d < dimensionCount; ++d) {
        if (((epoch1 & mask) == mask) != ((epoch2 & mask) == mask)) {
            STORM_LOG_ASSERT(getEpochClass(epoch1) != getEpochClass(epoch2), "Epoch classes should differ.");
            return false;
        }
        mask = mask << bitsPerDimension;
    }
    STORM_LOG_ASSERT(getEpochClass(epoch1) == getEpochClass(epoch2), "Epoch classes should match.");
    return true;
}

typename EpochManager::EpochClass EpochManager::getEpochClass(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    EpochClass result = 0;
    uint64_t dimensionMask = dimensionBitMask;
    uint64_t classMask = 1;
    for (uint64_t d = 0; d < dimensionCount; ++d) {
        if ((epoch & dimensionMask) == dimensionMask) {
            result |= classMask;
        }
        classMask = classMask << 1;
        dimensionMask = dimensionMask << bitsPerDimension;
    }
    return result;
}

typename EpochManager::Epoch EpochManager::getSuccessorEpoch(Epoch const& epoch, Epoch const& step) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    STORM_LOG_ASSERT(!hasBottomDimension(step), "The given step has at least one bottom dimension.");

    // Start with dimension zero
    uint64_t mask = dimensionBitMask;
    uint64_t e_d = epoch & mask;
    uint64_t s_d = step & mask;
    uint64_t result = (e_d < s_d || e_d == mask) ? mask : e_d - s_d;

    // Consider the remaining dimensions
    for (uint64_t d = 1; d < dimensionCount; ++d) {
        mask = mask << bitsPerDimension;
        e_d = epoch & mask;
        s_d = step & mask;
        result |= ((e_d < s_d || e_d == mask) ? mask : e_d - s_d);
    }
    return result;
}

std::vector<typename EpochManager::Epoch> EpochManager::getPredecessorEpochs(Epoch const& epoch, Epoch const& step) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    STORM_LOG_ASSERT(!hasBottomDimension(step), "The given step has at least one bottom dimension.");
    std::set<Epoch> resultAsSet;
    gatherPredecessorEpochs(resultAsSet, epoch, step);
    return std::vector<Epoch>(resultAsSet.begin(), resultAsSet.end());
}

void EpochManager::gatherPredecessorEpochs(std::set<Epoch>& gatheredPredecessorEpochs, Epoch const& epoch, Epoch const& step) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    STORM_LOG_ASSERT(!hasBottomDimension(step), "The given step has at least one bottom dimension.");
    Epoch currStep = step;
    uint64_t d = 0;
    while (d < dimensionCount) {
        Epoch predecessor = epoch;
        for (uint64_t dPrime = 0; dPrime < dimensionCount; ++dPrime) {
            uint64_t step_dPrime = getDimensionOfEpoch(currStep, dPrime);
            if (isBottomDimension(predecessor, dPrime)) {
                if (step_dPrime != 0) {
                    setDimensionOfEpoch(predecessor, dPrime, step_dPrime - 1);
                }
            } else {
                setDimensionOfEpoch(predecessor, dPrime, getDimensionOfEpoch(predecessor, dPrime) + step_dPrime);
            }
        }
        STORM_LOG_ASSERT(epoch == getSuccessorEpoch(predecessor, step), "Epoch does not match successor.");
        gatheredPredecessorEpochs.insert(predecessor);

        do {
            if (isBottomDimension(epoch, d)) {
                uint64_t step_d = getDimensionOfEpoch(currStep, d);
                if (step_d == 0) {
                    setDimensionOfEpoch(currStep, d, getDimensionOfEpoch(step, d));
                } else {
                    setDimensionOfEpoch(currStep, d, step_d - 1);
                    d = 0;
                    break;
                }
            }
            ++d;
        } while (d < dimensionCount);
    }
}

bool EpochManager::isValidDimensionValue(uint64_t const& value) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return ((value & dimensionBitMask) == value) && value != dimensionBitMask;
}

bool EpochManager::isZeroEpoch(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return (epoch & relevantBitsMask) == 0;
}

bool EpochManager::isBottomEpoch(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return (epoch & relevantBitsMask) == relevantBitsMask;
}

bool EpochManager::hasBottomDimension(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    uint64_t mask = dimensionBitMask;
    for (uint64_t d = 0; d < dimensionCount; ++d) {
        if ((epoch | mask) == epoch) {
            return true;
        }
        mask = mask << bitsPerDimension;
    }
    return false;
}

bool EpochManager::hasBottomDimensionEpochClass(EpochClass const& epochClass) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    uint64_t mask = (1 << dimensionCount) - 1;
    return (epochClass & mask) != 0;
}

bool EpochManager::isPredecessorEpochClass(EpochClass const& epochClass1, EpochClass const& epochClass2) const {
    return (epochClass1 & epochClass2) == epochClass1;
}

void EpochManager::setBottomDimension(Epoch& epoch, uint64_t const& dimension) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    epoch |= (dimensionBitMask << (dimension * bitsPerDimension));
}

void EpochManager::setDimensionOfEpoch(Epoch& epoch, uint64_t const& dimension, uint64_t const& value) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    STORM_LOG_ASSERT(isValidDimensionValue(value), "The dimension value " << value << " is too high.");
    epoch &= ~(dimensionBitMask << (dimension * bitsPerDimension));
    epoch |= (value << (dimension * bitsPerDimension));
}

void EpochManager::setDimensionOfEpochClass(EpochClass& epochClass, uint64_t const& dimension, bool const& setToBottom) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    if (setToBottom) {
        epochClass |= (1 << dimension);
    } else {
        epochClass &= ~(1 << dimension);
    }
}

bool EpochManager::isBottomDimension(Epoch const& epoch, uint64_t const& dimension) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return (epoch | (dimensionBitMask << (dimension * bitsPerDimension))) == epoch;
}

bool EpochManager::isBottomDimensionEpochClass(EpochClass const& epochClass, uint64_t const& dimension) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return (epochClass | (1 << dimension)) == epochClass;
}

uint64_t EpochManager::getDimensionOfEpoch(Epoch const& epoch, uint64_t const& dimension) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    return (epoch >> (dimension * bitsPerDimension)) & dimensionBitMask;
}

uint64_t EpochManager::getSumOfDimensions(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    uint64_t sumOfDimensions = 0;
    for (uint64_t dim = 0; dim < getDimensionCount(); ++dim) {
        if (!isBottomDimension(epoch, dim)) {
            sumOfDimensions += getDimensionOfEpoch(epoch, dim) + 1;
        }
    }
    return sumOfDimensions;
}

std::string EpochManager::toString(Epoch const& epoch) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    std::string res = "<" + (isBottomDimension(epoch, 0) ? "_" : std::to_string(getDimensionOfEpoch(epoch, 0)));
    for (uint64_t d = 1; d < dimensionCount; ++d) {
        res += ", ";
        res += (isBottomDimension(epoch, d) ? "_" : std::to_string(getDimensionOfEpoch(epoch, d)));
    }
    res += ">";
    return res;
}

bool EpochManager::epochClassZigZagOrder(Epoch const& epoch1, Epoch const& epoch2) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");

    // Return true iff epoch 1 has to be computed before epoch 2

    if (!compareEpochClass(epoch1, epoch2)) {
        return epochClassOrder(getEpochClass(epoch1), getEpochClass(epoch2));
    }

    // check whether the sum of dimensions is the same
    uint64_t e1Sum = 0;
    uint64_t e2Sum = 0;
    for (uint64_t dim = 0; dim < dimensionCount; ++dim) {
        if (!isBottomDimension(epoch1, dim)) {
            STORM_LOG_ASSERT(!isBottomDimension(epoch2, dim), "Unexpected bottom dimension in epoch2.");
            e1Sum += getDimensionOfEpoch(epoch1, dim);
            e2Sum += getDimensionOfEpoch(epoch2, dim);
        }
    }
    if (e1Sum < e2Sum) {
        return true;
    } else if (e1Sum > e2Sum) {
        return false;
    }

    // find the first dimension where the epochs do not match.
    // if the sum is even, we search from left to right, otherwise from right to left
    bool sumEven = (e1Sum % 2) == 0;
    if (sumEven) {
        for (uint64_t dim = 0; dim < dimensionCount; ++dim) {
            uint64_t e1Value = getDimensionOfEpoch(epoch1, dim);
            uint64_t e2Value = getDimensionOfEpoch(epoch2, dim);
            if (e1Value < e2Value) {
                return true;
            } else if (e1Value > e2Value) {
                return false;
            }
        }
    } else {
        uint64_t dim = dimensionCount;
        while (dim > 0) {
            --dim;
            uint64_t e1Value = getDimensionOfEpoch(epoch1, dim);
            uint64_t e2Value = getDimensionOfEpoch(epoch2, dim);
            if (e1Value < e2Value) {
                return true;
            } else if (e1Value > e2Value) {
                return false;
            }
        }
    }

    // reaching this point means that the epochs are equal
    STORM_LOG_ASSERT(epoch1 == epoch2, "Epochs should be equal.");
    return false;
}

bool EpochManager::epochClassOrder(EpochClass const& epochClass1, EpochClass const& epochClass2) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");

    if (epochClass1 == epochClass2) {
        return false;
    }

    // Return true iff epochClass 1 is a successor class of epochClass 2

    // Check whether the number of bottom dimensions is not equal
    int64_t count;
    EpochClass ec = epochClass1;
    for (count = 0; ec; ++count) {
        ec &= ec - 1;
    }
    ec = epochClass2;
    for (; ec; --count) {
        ec &= ec - 1;
    }
    if (count > 0) {
        return true;
    } else if (count < 0) {
        return false;
    }

    return epochClass1 < epochClass2;
}
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm