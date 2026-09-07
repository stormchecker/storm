#pragma once

#include <boost/optional.hpp>
#include <vector>

#include "storm/storage/jani/localeliminator/FinishAction.h"
#include "storm/storage/jani/localeliminator/JaniLocalEliminator.h"
#include "storm/storage/jani/localeliminator/UnfoldDependencyGraph.h"

// AutomaticAction performs location elimination completely automatically. For this, it alternates between unfolding a variable and eliminating as many
// locations as possible from the model. There are two main parameters to control this process:
// * locationLimit: If this number of locations is reached, no further unfolding will be performed
// * newTransitionLimit: Each candidate location will only be removed if doing so creates at most this many new transitions.

namespace storm {
namespace jani {
namespace elimination_actions {
class AutomaticAction : public JaniLocalEliminator::Action {
   public:
    explicit AutomaticAction();
    explicit AutomaticAction(uint64_t locationLimit, uint64_t newTransitionLimit, uint64_t maxDomainSize = 100, bool flatten = true);

    std::string getDescription() override;

    void doAction(JaniLocalEliminator::Session &session) override;

   private:
    uint64_t locationLimit;
    uint64_t newTransitionLimit;
    uint64_t maxDomainSize;
    bool flatten;

    void processAutomaton(JaniLocalEliminator::Session &session, std::string const &autName);

    void unfoldGroupAndDependencies(JaniLocalEliminator::Session &session, const std::string &autName, UnfoldDependencyGraph &dependencyGraph,
                                    uint32_t groupIndex);

    boost::optional<uint32_t> chooseNextUnfold(JaniLocalEliminator::Session &session, std::string const &automatonName, UnfoldDependencyGraph &dependencyGraph,
                                               bool onlyPropertyVariables);
    std::map<std::string, double> getAssignmentCountByVariable(JaniLocalEliminator::Session &session, std::string const &automatonName);
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
