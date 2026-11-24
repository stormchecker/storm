#pragma once

#include <cstdint>

namespace storm {

class CuddDdManagerEnvironment {
   public:
    CuddDdManagerEnvironment();
    ~CuddDdManagerEnvironment();

    /*!
     * Retrieves the precision up to which constants are considered to be different.
     */
    double getConstantPrecision() const;
    void setConstantPrecision(double value);

    /*!
     * Retrieves the maximal amount of memory (in megabytes) that CUDD can occupy.
     */
    uint64_t getMaximalMemory() const;
    void setMaximalMemory(uint64_t value);

   private:
    double constantPrecision;
    uint64_t maximalMemory;
};
}  // namespace storm
