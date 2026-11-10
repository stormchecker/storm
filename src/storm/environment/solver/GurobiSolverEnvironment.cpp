#include "storm/environment/solver/GurobiSolverEnvironment.h"

#include "storm/solver/GurobiLpSolver.h"

namespace storm {

GurobiSolverEnvironment::GurobiSolverEnvironment()
    : method(storm::solver::GurobiSolverMethod::AUTOMATIC),
      numberOfThreads(1),
      mipFocus(0),
      numberOfConcurrentMipThreads(1),
      integerTolerance(1e-06),
      output(false) {
    // Intentionally left empty.
}

GurobiSolverEnvironment::~GurobiSolverEnvironment() {
    // Intentionally left empty.
}

storm::solver::GurobiSolverMethod const& GurobiSolverEnvironment::getMethod() const {
    return method;
}

void GurobiSolverEnvironment::setMethod(storm::solver::GurobiSolverMethod value) {
    method = value;
}

uint64_t GurobiSolverEnvironment::getNumberOfThreads() const {
    return numberOfThreads;
}

void GurobiSolverEnvironment::setNumberOfThreads(uint64_t value) {
    numberOfThreads = value;
}

uint64_t GurobiSolverEnvironment::getMIPFocus() const {
    return mipFocus;
}

void GurobiSolverEnvironment::setMIPFocus(uint64_t value) {
    mipFocus = value;
}

uint64_t GurobiSolverEnvironment::getNumberOfConcurrentMipThreads() const {
    return numberOfConcurrentMipThreads;
}

void GurobiSolverEnvironment::setNumberOfConcurrentMipThreads(uint64_t value) {
    numberOfConcurrentMipThreads = value;
}

double GurobiSolverEnvironment::getIntegerTolerance() const {
    return integerTolerance;
}

void GurobiSolverEnvironment::setIntegerTolerance(double value) {
    integerTolerance = value;
}

bool GurobiSolverEnvironment::isOutputSet() const {
    return output;
}

void GurobiSolverEnvironment::setOutput(bool value) {
    output = value;
}

}  // namespace storm
