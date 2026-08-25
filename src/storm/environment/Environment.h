#pragma once

#include "storm/environment/SubEnvironment.h"

namespace storm {

// Forward declare sub-environments
class DdEnvironment;
class ExplorationEnvironment;
class ModelCheckerEnvironment;
class SolverEnvironment;

// Avoid implementing ugly copy constructors for environment by using an internal environment.
struct InternalEnvironment {
    SubEnvironment<DdEnvironment> ddEnvironment;
    SubEnvironment<ExplorationEnvironment> explorationEnvironment;
    SubEnvironment<ModelCheckerEnvironment> modelcheckerEnvironment;
    SubEnvironment<SolverEnvironment> solverEnvironment;
};

class Environment {
   public:
    Environment();
    virtual ~Environment();
    Environment(Environment const& other);
    Environment& operator=(Environment const& other);

    DdEnvironment& dd();
    DdEnvironment const& dd() const;
    ExplorationEnvironment& exploration();
    ExplorationEnvironment const& exploration() const;
    ModelCheckerEnvironment& modelchecker();
    ModelCheckerEnvironment const& modelchecker() const;
    SolverEnvironment& solver();
    SolverEnvironment const& solver() const;

    double modelTolerance() const;
    void setModelTolerance(double value);

   private:
    SubEnvironment<InternalEnvironment> internalEnv;
    double modelToleranceValue;
};

// Explicitly instantiated once in CoreEnvironments.cpp
// Avoids redundant re-instantiation elsewhere
extern template class SubEnvironment<Environment>;  // Not needed in core but needed for other libraries
extern template class SubEnvironment<InternalEnvironment>;
extern template class SubEnvironment<DdEnvironment>;
extern template class SubEnvironment<ExplorationEnvironment>;
extern template class SubEnvironment<ModelCheckerEnvironment>;
extern template class SubEnvironment<SolverEnvironment>;

}  // namespace storm
