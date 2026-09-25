#pragma once

#include "storm/environment/Environment.h"

namespace storm {
namespace settings {

/*!
 * Builds a storm::Environment based on the currently registered settings.
 *
 * The environment itself does not know about settings anymore; this class is the only place
 * where (a subset of) the settings are translated into an environment.
 */
class EnvironmentBuilder {
   public:
    EnvironmentBuilder() = delete;

    static storm::Environment buildEnvironment();

   private:
    static void setSolverEnvironment(storm::SolverEnvironment& solver);
    static void setModelcheckerEnvironment(storm::ModelCheckerEnvironment& modelchecker);
    static void setDdEnvironment(storm::DdEnvironment& dd);
    static void setExplorationEnvironment(storm::ExplorationEnvironment& exploration);
};

}  // namespace settings
}  // namespace storm