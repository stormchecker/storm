#pragma once

#include "JaniLocalEliminator.h"

// UnfoldAction unfolds a variable into state space. For example, if a model has locations l1 and l2 and we unfold a variable x with domain {1, 2, 3}, the new
// model will have locations l1_x_1, l1_x_2, …, l_2_x_3. The new locations will have a transient assignment that indicates the value of the unfolded variable.

namespace storm {
namespace jani {
namespace elimination_actions {
class UnfoldAction : public JaniLocalEliminator::Action {
   public:
    explicit UnfoldAction(const std::string &automatonName, const std::string &variableName);
    explicit UnfoldAction(const std::string &automatonName, const std::string &janiVariableName, const std::string &expressionVariableName);

    std::string getDescription() override;
    void doAction(JaniLocalEliminator::Session &session) override;

    std::string automatonName;
    std::string variableName;
    std::string expressionVariableName;
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
