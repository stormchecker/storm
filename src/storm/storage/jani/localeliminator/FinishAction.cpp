#include "FinishAction.h"

namespace storm {
namespace jani {
namespace elimination_actions {
FinishAction::FinishAction() {}

std::string FinishAction::getDescription() {
    return "FinishAction";
}

void FinishAction::doAction(JaniLocalEliminator::Session &session) {
    session.setFinished(true);
}
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
