#pragma once

#include <cstdint>

namespace storm {

class SylvanDdManagerEnvironment {
   public:
    SylvanDdManagerEnvironment();
    ~SylvanDdManagerEnvironment();

    /*!
     * Retrieves the maximal amount of memory (in megabytes) that Sylvan can occupy.
     */
    uint64_t getMaximalMemory() const;
    void setMaximalMemory(uint64_t value);

    /*!
     * Retrieves the number of threads used by Sylvan. Note that a value of zero means that the number of threads
     * is auto-detected to fit the current machine.
     */
    uint64_t getNumberOfThreads() const;
    void setNumberOfThreads(uint64_t value);

   private:
    uint64_t maximalMemory;
    uint64_t numberOfThreads;
};
}  // namespace storm
