#pragma once

#include "storm/languages/jani/localeliminator/JaniLocalEliminator.h"

// When a FinishAction is executed, it stops any further actions from being executed.

namespace storm {
namespace jani {
namespace elimination_actions {
class FinishAction : public JaniLocalEliminator::Action {
   public:
    explicit FinishAction();
    std::string getDescription() override;
    void doAction(JaniLocalEliminator::Session &session) override;
};
}  // namespace elimination_actions

}  // namespace jani
}  // namespace storm
