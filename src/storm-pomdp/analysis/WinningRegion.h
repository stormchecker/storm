#pragma once

#include <vector>

#include "storm/adapters/RationalNumberForward.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
class Expression;
}
namespace pomdp {
class WinningRegion {
   public:
    WinningRegion(std::vector<uint64_t> const& observationSizes = {});

    bool update(uint64_t observation, storm::storage::BitVector const& winning);
    bool query(uint64_t observation, storm::storage::BitVector const& currently) const;
    bool isWinning(uint64_t observation, uint64_t offset) const {
        STORM_LOG_ASSERT(observation < observationSizes.size(), "Observation index out of range.");
        STORM_LOG_ASSERT(offset < observationSizes[observation], "Offset out of range for observation.");
        storm::storage::BitVector currently(observationSizes[observation]);
        currently.set(offset);
        return query(observation, currently);
    }

    std::vector<storm::storage::BitVector> const& getWinningSetsPerObservation(uint64_t observation) const;

    void addTargetStates(uint64_t observation, storm::storage::BitVector const& offsets);
    void setObservationIsWinning(uint64_t observation);

    bool observationIsWinning(uint64_t observation) const;
    storm::expressions::Expression extensionExpression(uint64_t observation, std::vector<storm::expressions::Expression>& varsForStates) const;

    uint64_t getStorageSize() const;
    storm::RationalNumber beliefSupportStates() const;
    std::pair<storm::RationalNumber, storm::RationalNumber> computeNrWinningBeliefs() const;

    uint64_t getNumberOfObservations() const;
    bool empty() const;
    void print() const;

    void storeToFile(std::string const& path, std::string const& preamble = "", bool append = false) const;
    static std::pair<WinningRegion, std::string> loadFromFile(std::string const& path);

   private:
    std::vector<std::vector<storm::storage::BitVector>> winningRegion;
    std::vector<uint64_t> observationSizes;
};
}  // namespace pomdp
}  // namespace storm
