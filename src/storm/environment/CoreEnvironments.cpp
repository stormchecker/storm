// Explicit instantiations of core environments

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/environment/dd/AllDdEnvironments.h"
#include "storm/environment/exploration/ExplorationEnvironment.h"
#include "storm/environment/modelchecker/AllModelCheckerEnvironments.h"
#include "storm/environment/solver/AllSolverEnvironments.h"

namespace storm {

// Not used within the core, but needed by other libraries
template class SubEnvironment<Environment>;

template class SubEnvironment<InternalEnvironment>;

template class SubEnvironment<DdEnvironment>;
template class SubEnvironment<CuddDdManagerEnvironment>;
template class SubEnvironment<SylvanDdManagerEnvironment>;

template class SubEnvironment<ExplorationEnvironment>;

template class SubEnvironment<ModelCheckerEnvironment>;
template class SubEnvironment<ConditionalModelCheckerEnvironment>;
template class SubEnvironment<MultiObjectiveModelCheckerEnvironment>;

template class SubEnvironment<SolverEnvironment>;
template class SubEnvironment<EigenSolverEnvironment>;
template class SubEnvironment<EliminationSolverEnvironment>;
template class SubEnvironment<GameSolverEnvironment>;
template class SubEnvironment<GlpkSolverEnvironment>;
template class SubEnvironment<GmmxxSolverEnvironment>;
template class SubEnvironment<GurobiSolverEnvironment>;
template class SubEnvironment<LongRunAverageSolverEnvironment>;
template class SubEnvironment<MinMaxSolverEnvironment>;
template class SubEnvironment<MinMaxLpSolverEnvironment>;
template class SubEnvironment<MultiplierEnvironment>;
template class SubEnvironment<NativeSolverEnvironment>;
template class SubEnvironment<OviSolverEnvironment>;
template class SubEnvironment<TimeBoundedSolverEnvironment>;
template class SubEnvironment<TopologicalSolverEnvironment>;

}  // namespace storm
