#include "VariablesToConstantsTransformer.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/jani/Constant.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Variable.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/traverser/AssignmentsFinder.h"

namespace storm {

namespace jani {
namespace detail {
storm::jani::Constant createConstantFromVariable(storm::jani::Variable const& variable) {
    STORM_LOG_ASSERT(variable.hasInitExpression(),
                     "Trying to convert variable " << variable.getName() << " to a constant, but the variable does not have an initial value.");
    // For the name we use the name of the expression variable to assert uniqueness.
    return storm::jani::Constant(variable.getExpressionVariable().getName(), variable.getExpressionVariable(), variable.getInitExpression());
}

template<typename JaniStructureType>
bool canTransformVariable(storm::jani::Variable const& variable, JaniStructureType const& janiStructure) {
    // It does not make sense to do this for transient variables.
    if (variable.isTransient()) {
        return false;
    }
    // The jani specification only allows these two types of constants.
    if (!(variable.getType().isBasicType() || variable.getType().isBoundedType())) {
        return false;
    }
    // The variable should have a known initial value.
    if (!variable.hasInitExpression()) {
        return false;
    }
    // Finally, there should be no assignment that assignes a value to this variable.
    auto assignmentTypes = storm::jani::AssignmentsFinder().find(janiStructure, variable);
    return !assignmentTypes.hasEdgeAssignment && !assignmentTypes.hasEdgeDestinationAssignment && !assignmentTypes.hasLocationAssignment;
}
}  // namespace detail

void VariablesToConstantsTransformer::transform(Model& model) {
    // It is dangerous to erase a variable from a set of variables while iterating over the set.
    // Therefore, we store the variables and erase them later.
    std::vector<storm::expressions::Variable> variablesToErase;
    for (auto const& globalVar : model.getGlobalVariables()) {
        if (detail::canTransformVariable(globalVar, model)) {
            variablesToErase.push_back(globalVar.getExpressionVariable());
        }
    }
    for (auto const& varToErase : variablesToErase) {
        STORM_LOG_INFO("Transformed Variable " << varToErase.getName() << " to a constant since it is not assigned any value.");
        auto erasedJaniVariable = model.getGlobalVariables().eraseVariable(varToErase);
        auto createdConstant = detail::createConstantFromVariable(*erasedJaniVariable);
        model.addConstant(createdConstant);
    }
    for (auto& aut : model.getAutomata()) {
        variablesToErase.clear();
        for (auto const& localVar : aut.getVariables()) {
            if (detail::canTransformVariable(localVar, aut)) {
                variablesToErase.push_back(localVar.getExpressionVariable());
            }
        }
        for (auto const& varToErase : variablesToErase) {
            STORM_LOG_INFO("Transformed Variable " << varToErase.getName() << " to a constant since it is not assigned any value.");
            auto erasedJaniVariable = aut.getVariables().eraseVariable(varToErase);
            auto createdConstant = detail::createConstantFromVariable(*erasedJaniVariable);
            model.addConstant(createdConstant);
        }
    }
}
}  // namespace jani
}  // namespace storm
