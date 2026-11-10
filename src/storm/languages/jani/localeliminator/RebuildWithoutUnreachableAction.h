#pragma once

#include "JaniLocalEliminator.h"

// RebuildWithoutUnreachable constructs a new model that is equivalent to the old model, but does not contain edges with guard "false" and unreachable
// locations. Removing unsatisfiable edges is necessary because elimination actions only set guards to "false" instead of removing edges.

namespace storm {
namespace jani {
namespace elimination_actions {
class RebuildWithoutUnreachableAction : public JaniLocalEliminator::Action {
   public:
    explicit RebuildWithoutUnreachableAction();
    std::string getDescription() override;
    void doAction(JaniLocalEliminator::Session &session) override;
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
