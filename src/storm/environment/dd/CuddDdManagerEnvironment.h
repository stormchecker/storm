#pragma once

#include <cstdint>

#include "storm/storage/dd/cudd/CuddReorderingTechnique.h"

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

    bool isReorderingEnabled() const;
    void setReorderingEnabled(bool value);

    storm::dd::CuddReorderingTechnique getReorderingTechnique() const;
    void setReorderingTechnique(storm::dd::CuddReorderingTechnique value);

   private:
    double constantPrecision;
    uint64_t maximalMemory;
    bool reorderingEnabled;
    storm::dd::CuddReorderingTechnique reorderingTechnique;
};
}  // namespace storm
