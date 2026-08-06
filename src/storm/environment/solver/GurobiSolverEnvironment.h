#pragma once

#include <cstdint>

namespace storm {
namespace solver {
enum class GurobiSolverMethod;
}

class GurobiSolverEnvironment {
   public:
    GurobiSolverEnvironment();
    ~GurobiSolverEnvironment();

    storm::solver::GurobiSolverMethod const& getMethod() const;
    void setMethod(storm::solver::GurobiSolverMethod value);

    uint64_t getNumberOfThreads() const;
    void setNumberOfThreads(uint64_t value);

    uint64_t getMIPFocus() const;
    void setMIPFocus(uint64_t value);

    uint64_t getNumberOfConcurrentMipThreads() const;
    void setNumberOfConcurrentMipThreads(uint64_t value);

    double getIntegerTolerance() const;
    void setIntegerTolerance(double value);

    bool isOutputSet() const;
    void setOutput(bool value);

   private:
    storm::solver::GurobiSolverMethod method;
    uint64_t numberOfThreads;
    uint64_t mipFocus;
    uint64_t numberOfConcurrentMipThreads;
    double integerTolerance;
    bool output;
};
}  // namespace storm
