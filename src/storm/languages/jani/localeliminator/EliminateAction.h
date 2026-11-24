#pragma once

#include "JaniLocalEliminator.h"

// EliminateAction removes the given location from the model. For this, the location must not be initial, satisfy the property, or have a loop. This action
// assumes that these properties already hold.
// Since it is not cheap to delete edges, their guards are instead only set to false. It is recommended to execute a RebuildWithoutUnreachableAction after
// eliminating locations to actually remove edges and the now-unreachable locations.

namespace storm {
namespace jani {
namespace elimination_actions {
class EliminateAction : public JaniLocalEliminator::Action {
   public:
    explicit EliminateAction(const std::string &automatonName, const std::string &locationName);

    std::string getDescription() override;
    void doAction(JaniLocalEliminator::Session &session) override;

   private:
    void eliminateDestination(JaniLocalEliminator::Session &session, Automaton &automaton, Edge &edge, uint64_t destIndex, detail::Edges &outgoing);

    std::string automatonName;
    std::string locationName;
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
