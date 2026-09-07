#include "RewardModelInformation.h"

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/constants.h"

namespace storm {
namespace jani {

RewardModelInformation::RewardModelInformation(bool hasStateRewards, bool hasActionRewards, bool hasTransitionRewards)
    : stateRewards(hasStateRewards), actionRewards(hasActionRewards), transitionRewards(hasTransitionRewards) {
    // Intentionally left empty
}

RewardModelInformation::RewardModelInformation(Model const& model, std::string const& rewardModelNameIdentifier)
    : RewardModelInformation(model, model.getRewardModelExpression(rewardModelNameIdentifier)) {
    // Intentionally left empty
}

RewardModelInformation::RewardModelInformation(Model const& model, storm::expressions::Expression const& rewardModelExpression)
    : stateRewards(false), actionRewards(false), transitionRewards(false) {
    auto variablesInRewardExpression = rewardModelExpression.getVariables();
    std::map<storm::expressions::Variable, storm::expressions::Expression> initialSubstitution;
    bool containsNonTransientVariable = false;
    for (auto const& v : variablesInRewardExpression) {
        if (model.hasConstant(v.getName())) {
            auto const& janiConst = model.getConstant(v.getName());
            if (janiConst.isDefined()) {
                initialSubstitution.emplace(v, janiConst.getExpression());
            }
        } else {
            STORM_LOG_ASSERT(model.hasGlobalVariable(v.getName()), "Unable to find global variable " << v.getName() << " occurring in a reward expression.");
            auto const& janiVar = model.getGlobalVariable(v.getName());
            if (janiVar.hasInitExpression()) {
                initialSubstitution.emplace(v, janiVar.getInitExpression());
            }
            if (!janiVar.isTransient()) {
                containsNonTransientVariable = true;
            }
        }
    }
    auto initExpr = storm::jani::substituteJaniExpression(rewardModelExpression, initialSubstitution, true).simplify();
    if (containsNonTransientVariable || initExpr.containsVariables() || !storm::utility::isZero(initExpr.evaluateAsRational())) {
        stateRewards = true;
        actionRewards = true;
        transitionRewards = true;
    }
    traverse(model, &variablesInRewardExpression);
}

void RewardModelInformation::traverse(Location const& location, boost::any const& data) {
    auto const& vars = *boost::any_cast<std::set<storm::expressions::Variable>*>(data);
    if (!hasStateRewards()) {
        for (auto const& assignment : location.getAssignments().getTransientAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (vars.count(assignedVariable.getExpressionVariable()) > 0) {
                stateRewards = true;
                break;
            }
        }
    }
}

void RewardModelInformation::traverse(TemplateEdge const& templateEdge, boost::any const& data) {
    auto const& vars = *boost::any_cast<std::set<storm::expressions::Variable>*>(data);
    if (!hasActionRewards()) {
        for (auto const& assignment : templateEdge.getAssignments().getTransientAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (vars.count(assignedVariable.getExpressionVariable()) > 0) {
                actionRewards = true;
                break;
            }
        }
    }
    for (auto const& dest : templateEdge.getDestinations()) {
        traverse(dest, data);
    }
}

void RewardModelInformation::traverse(TemplateEdgeDestination const& templateEdgeDestination, boost::any const& data) {
    auto const& vars = *boost::any_cast<std::set<storm::expressions::Variable>*>(data);
    if (!hasTransitionRewards()) {
        for (auto const& assignment : templateEdgeDestination.getOrderedAssignments().getTransientAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (vars.count(assignedVariable.getExpressionVariable()) > 0) {
                transitionRewards = true;
                break;
            }
        }
    }
}

RewardModelInformation RewardModelInformation::join(RewardModelInformation const& other) const {
    return RewardModelInformation(this->hasStateRewards() || other.hasStateRewards(), this->hasActionRewards() || other.hasActionRewards(),
                                  this->hasTransitionRewards() || other.hasTransitionRewards());
}

bool RewardModelInformation::hasStateRewards() const {
    return stateRewards;
}

bool RewardModelInformation::hasActionRewards() const {
    return actionRewards;
}

bool RewardModelInformation::hasTransitionRewards() const {
    return transitionRewards;
}

}  // namespace jani
}  // namespace storm
