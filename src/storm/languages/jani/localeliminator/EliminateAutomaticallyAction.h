#pragma once

#include "JaniLocalEliminator.h"

// EliminateAutomaticallyAction determines which locations can be eliminated in the given automaton and automatically eliminates them, until doing so would
// create too many new transitions. The elimination order can be specified, with NewTransitionCount recommended in most cases, since it produces smaller
// models (at increased runtime cost).

namespace storm {
namespace jani {
namespace elimination_actions {
class EliminateAutomaticallyAction : public JaniLocalEliminator::Action {
   public:
    enum class EliminationOrder { Arbitrary, NewTransitionCount };

    explicit EliminateAutomaticallyAction(const std::string &automatonName, EliminationOrder eliminationOrder, uint32_t transitionCountThreshold = 1000,
                                          bool restrictToUnnamedActions = false);
    std::string getDescription() override;
    void doAction(JaniLocalEliminator::Session &session) override;

   private:
    std::string automatonName;
    EliminationOrder eliminationOrder;
    bool restrictToUnnamedActions;
    uint32_t transitionCountThreshold;
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
