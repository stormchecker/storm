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
}  // namespace storm
