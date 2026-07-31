#include "FunctionEliminator.h"

#include <unordered_map>

#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/Variable.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/traverser/FunctionCallExpressionFinder.h"
#include "storm/storage/jani/traverser/JaniTraverser.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace jani {
namespace detail {

class FunctionEliminationExpressionVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;

    typedef std::shared_ptr<storm::expressions::BaseExpression const> BaseExprPtr;

    FunctionEliminationExpressionVisitor(std::unordered_map<std::string, FunctionDefinition> const* globalFunctions,
                                         std::unordered_map<std::string, FunctionDefinition> const* localFunctions = nullptr)
        : globalFunctions(globalFunctions), localFunctions(localFunctions) {}

    virtual ~FunctionEliminationExpressionVisitor() = default;

    FunctionEliminationExpressionVisitor setLocalFunctions(std::unordered_map<std::string, FunctionDefinition> const* localFunctions) {
        return FunctionEliminationExpressionVisitor(this->globalFunctions, localFunctions);
    }

    storm::expressions::Expression eliminate(storm::expressions::Expression const& expression) {
        auto res = storm::expressions::Expression(boost::any_cast<BaseExprPtr>(expression.accept(*this, boost::any())));
        return res.simplify();
    }

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        BaseExprPtr conditionExpression = boost::any_cast<BaseExprPtr>(expression.getCondition()->accept(*this, data));
        BaseExprPtr thenExpression = boost::any_cast<BaseExprPtr>(expression.getThenExpression()->accept(*this, data));
        BaseExprPtr elseExpression = boost::any_cast<BaseExprPtr>(expression.getElseExpression()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (conditionExpression.get() == expression.getCondition().get() && thenExpression.get() == expression.getThenExpression().get() &&
            elseExpression.get() == expression.getElseExpression().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::IfThenElseExpression(
                    expression.getManager(), thenExpression->getType(), conditionExpression, thenExpression, elseExpression)));
        }
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        BaseExprPtr firstExpression = boost::any_cast<BaseExprPtr>(expression.getFirstOperand()->accept(*this, data));
        BaseExprPtr secondExpression = boost::any_cast<BaseExprPtr>(expression.getSecondOperand()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryBooleanFunctionExpression(
                    expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
        }
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        BaseExprPtr firstExpression = boost::any_cast<BaseExprPtr>(expression.getFirstOperand()->accept(*this, data));
        BaseExprPtr secondExpression = boost::any_cast<BaseExprPtr>(expression.getSecondOperand()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryNumericalFunctionExpression(
                    expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getOperatorType())));
        }
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        BaseExprPtr firstExpression = boost::any_cast<BaseExprPtr>(expression.getFirstOperand()->accept(*this, data));
        BaseExprPtr secondExpression = boost::any_cast<BaseExprPtr>(expression.getSecondOperand()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryRelationExpression(
                    expression.getManager(), expression.getType(), firstExpression, secondExpression, expression.getRelationType())));
        }
    }

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const&) override {
        return expression.getSharedPointer();
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        BaseExprPtr operandExpression = boost::any_cast<BaseExprPtr>(expression.getOperand()->accept(*this, data));

        // If the argument did not change, we simply push the expression itself.
        if (operandExpression.get() == expression.getOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::UnaryBooleanFunctionExpression(
                    expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
        }
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        BaseExprPtr operandExpression = boost::any_cast<BaseExprPtr>(expression.getOperand()->accept(*this, data));

        // If the argument did not change, we simply push the expression itself.
        if (operandExpression.get() == expression.getOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::UnaryNumericalFunctionExpression(
                    expression.getManager(), expression.getType(), operandExpression, expression.getOperatorType())));
        }
    }

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) override {
        return expression.getSharedPointer();
    }

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) override {
        return expression.getSharedPointer();
    }

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) override {
        return expression.getSharedPointer();
    }

    virtual boost::any visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(expression.size()->isIntegerLiteralExpression(),
                         "unexpected kind of size expression of ValueArrayExpression (" << expression.size()->toExpression() << ").");
        uint64_t size = expression.size()->evaluateAsInt();
        std::vector<BaseExprPtr> elements;
        bool changed = false;
        for (uint64_t i = 0; i < size; ++i) {
            BaseExprPtr element = boost::any_cast<BaseExprPtr>(expression.at(i)->accept(*this, data));
            if (element.get() != expression.at(i).get()) {
                changed = true;
            }
            elements.push_back(element);
        }
        if (changed) {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(std::shared_ptr<storm::expressions::BaseExpression>(
                new storm::expressions::ValueArrayExpression(expression.getManager(), expression.getType(), elements)));
        } else {
            return expression.getSharedPointer();
        }
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) override {
        BaseExprPtr sizeExpression = boost::any_cast<BaseExprPtr>(expression.size()->accept(*this, data));
        BaseExprPtr elementExpression = boost::any_cast<BaseExprPtr>(expression.getElementExpression()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (sizeExpression.get() == expression.size().get() && elementExpression.get() == expression.getElementExpression().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::ConstructorArrayExpression(
                    expression.getManager(), expression.getType(), sizeExpression, expression.getIndexVar(), elementExpression)));
        }
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) override {
        BaseExprPtr firstExpression = boost::any_cast<BaseExprPtr>(expression.getFirstOperand()->accept(*this, data));
        BaseExprPtr secondExpression = boost::any_cast<BaseExprPtr>(expression.getSecondOperand()->accept(*this, data));

        // If the arguments did not change, we simply push the expression itself.
        if (firstExpression.get() == expression.getFirstOperand().get() && secondExpression.get() == expression.getSecondOperand().get()) {
            return expression.getSharedPointer();
        } else {
            return std::const_pointer_cast<storm::expressions::BaseExpression const>(std::shared_ptr<storm::expressions::BaseExpression>(
                new storm::expressions::ArrayAccessExpression(expression.getManager(), expression.getType(), firstExpression, secondExpression)));
        }
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data) override {
        // Find the associated function definition
        FunctionDefinition const* funDef = nullptr;
        if (localFunctions != nullptr) {
            auto funDefIt = localFunctions->find(expression.getFunctionIdentifier());
            if (funDefIt != localFunctions->end()) {
                funDef = &(funDefIt->second);
            }
        }
        if (globalFunctions != nullptr) {
            auto funDefIt = globalFunctions->find(expression.getFunctionIdentifier());
            if (funDefIt != globalFunctions->end()) {
                funDef = &(funDefIt->second);
            }
        }

        STORM_LOG_THROW(funDef != nullptr, storm::exceptions::UnexpectedException,
                        "Unable to find function definition for function call " << expression << ".");

        return boost::any_cast<BaseExprPtr>(funDef->call(expression.getArguments()).getBaseExpression().accept(*this, data));
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const& expression, boost::any const&) override {
        return expression.getSharedPointer();
    }

   private:
    std::unordered_map<std::string, FunctionDefinition> const* globalFunctions;
    std::unordered_map<std::string, FunctionDefinition> const* localFunctions;
};

class FunctionEliminatorTraverser : public JaniTraverser {
   public:
    using JaniTraverser::traverse;

    FunctionEliminatorTraverser() = default;

    virtual ~FunctionEliminatorTraverser() = default;

    void eliminate(Model& model, std::vector<Property>& properties) {
        // Replace all function calls by the function definition
        traverse(model, boost::any());

        // Replace function definitions in properties
        if (!model.getGlobalFunctionDefinitions().empty()) {
            FunctionEliminationExpressionVisitor v(&model.getGlobalFunctionDefinitions());
            for (auto& property : properties) {
                property = property.substitute([&v](storm::expressions::Expression const& exp) { return v.eliminate(exp); });
            }
        }

        // Erase function definitions in model and automata
        model.getGlobalFunctionDefinitions().clear();
        for (auto& automaton : model.getAutomata()) {
            automaton.getFunctionDefinitions().clear();
        }

        // Clear the model feature 'functions'
        model.getModelFeatures().remove(ModelFeature::Functions);

        // finalize the model
        model.finalize();
    }

    // To detect cyclic dependencies between function bodies, we need to eliminate the functions in a topological order
    enum class FunctionDefinitionStatus { Unprocessed, Current, Processed };
    void eliminateFunctionsInFunctionBodies(FunctionEliminationExpressionVisitor& eliminationVisitor,
                                            std::unordered_map<std::string, FunctionDefinition>& functions,
                                            std::unordered_map<std::string, FunctionDefinitionStatus>& status, std::string const& current) {
        status[current] = FunctionDefinitionStatus::Current;
        FunctionDefinition& funDef = functions.find(current)->second;
        auto calledFunctions = getOccurringFunctionCalls(funDef.getFunctionBody());
        for (auto const& calledFunction : calledFunctions) {
            STORM_LOG_THROW(calledFunction != current, storm::exceptions::NotSupportedException,
                            "Function '" << calledFunction << "' calls itself. This is not supported.");
            auto calledStatus = status.find(calledFunction);
            // Check whether the called function belongs to the ones that actually needed processing
            if (calledStatus != status.end()) {
                STORM_LOG_THROW(calledStatus->second != FunctionDefinitionStatus::Current, storm::exceptions::NotSupportedException,
                                "Found cyclic dependencies between functions '" << calledFunction << "' and '" << current << "'. This is not supported.");
                if (calledStatus->second == FunctionDefinitionStatus::Unprocessed) {
                    eliminateFunctionsInFunctionBodies(eliminationVisitor, functions, status, calledFunction);
                }
            }
        }
        // At this point, all called functions are processed already. So we can finally process this one.
        funDef.setFunctionBody(eliminationVisitor.eliminate(funDef.getFunctionBody()));
        status[current] = FunctionDefinitionStatus::Processed;
    }
    void eliminateFunctionsInFunctionBodies(FunctionEliminationExpressionVisitor& eliminationVisitor,
                                            std::unordered_map<std::string, FunctionDefinition>& functions) {
        std::unordered_map<std::string, FunctionDefinitionStatus> status;
        for (auto const& f : functions) {
            status.emplace(f.first, FunctionDefinitionStatus::Unprocessed);
        }
        for (auto const& f : functions) {
            if (status[f.first] == FunctionDefinitionStatus::Unprocessed) {
                eliminateFunctionsInFunctionBodies(eliminationVisitor, functions, status, f.first);
            }
        }
    }

    virtual void traverse(Model& model, boost::any const&) override {
        // First we need to apply functions called in function bodies
        FunctionEliminationExpressionVisitor globalFunctionEliminationVisitor(&model.getGlobalFunctionDefinitions());
        eliminateFunctionsInFunctionBodies(globalFunctionEliminationVisitor, model.getGlobalFunctionDefinitions());

        // Now run through the remaining components
        for (auto& c : model.getConstants()) {
            traverse(c, &globalFunctionEliminationVisitor);
        }
        traverse(model.getGlobalVariables(), &globalFunctionEliminationVisitor);
        for (auto& aut : model.getAutomata()) {
            traverse(aut, &globalFunctionEliminationVisitor);
        }
        if (model.hasInitialStatesRestriction()) {
            model.setInitialStatesRestriction(globalFunctionEliminationVisitor.eliminate(model.getInitialStatesRestriction()));
        }
        for (auto& nonTrivRew : model.getNonTrivialRewardExpressions()) {
            nonTrivRew.second = globalFunctionEliminationVisitor.eliminate(nonTrivRew.second);
        }
    }

    void traverse(Automaton& automaton, boost::any const& data) override {
        // First we need to apply functions called in function bodies
        auto functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data)->setLocalFunctions(&automaton.getFunctionDefinitions());
        eliminateFunctionsInFunctionBodies(functionEliminationVisitor, automaton.getFunctionDefinitions());

        // Now run through the remaining components
        traverse(automaton.getVariables(), &functionEliminationVisitor);
        for (auto& loc : automaton.getLocations()) {
            traverse(loc, &functionEliminationVisitor);
        }
        traverse(automaton.getEdgeContainer(), &functionEliminationVisitor);
        if (automaton.hasInitialStatesRestriction()) {
            automaton.setInitialStatesRestriction(functionEliminationVisitor.eliminate(automaton.getInitialStatesRestriction()));
        }
    }

    void traverse(Location& location, boost::any const& data) override {
        traverse(location.getAssignments(), data);
        if (location.hasTimeProgressInvariant()) {
            FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
            location.setTimeProgressInvariant(functionEliminationVisitor->eliminate(location.getTimeProgressInvariant()));
        }
    }

    void traverse(Constant& constant, boost::any const& data) override {
        if (constant.isDefined()) {
            FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
            constant.define(functionEliminationVisitor->eliminate(constant.getExpression()));
        }
    }

    void traverse(Variable& variable, boost::any const& data) override {
        FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
        if (variable.hasInitExpression()) {
            variable.setInitExpression(functionEliminationVisitor->eliminate(variable.getInitExpression()));
        }
        traverse(variable.getType(), data);
    }

    void traverse(JaniType& type, boost::any const& data) override {
        if (type.isBoundedType()) {
            FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
            auto& bndType = type.asBoundedType();
            if (bndType.hasLowerBound()) {
                bndType.setLowerBound(functionEliminationVisitor->eliminate(bndType.getLowerBound()));
            }
            if (bndType.hasUpperBound()) {
                bndType.setUpperBound(functionEliminationVisitor->eliminate(bndType.getUpperBound()));
            }
        } else if (type.isArrayType()) {
            traverse(type.asArrayType().getBaseType(), data);
        }
    }

    void traverse(TemplateEdge& templateEdge, boost::any const& data) override {
        FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
        templateEdge.setGuard(functionEliminationVisitor->eliminate(templateEdge.getGuard()));
        for (auto& dest : templateEdge.getDestinations()) {
            traverse(dest, data);
        }
        traverse(templateEdge.getAssignments(), data);
    }

    void traverse(Edge& edge, boost::any const& data) override {
        FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
        if (edge.hasRate()) {
            edge.setRate(functionEliminationVisitor->eliminate(edge.getRate()));
        }
        for (auto& dest : edge.getDestinations()) {
            traverse(dest, data);
        }
    }

    void traverse(EdgeDestination& edgeDestination, boost::any const& data) override {
        FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
        edgeDestination.setProbability(functionEliminationVisitor->eliminate(edgeDestination.getProbability()));
    }

    void traverse(Assignment& assignment, boost::any const& data) override {
        FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
        assignment.setAssignedExpression(functionEliminationVisitor->eliminate(assignment.getAssignedExpression()));
        traverse(assignment.getLValue(), data);
    }

    void traverse(LValue& lValue, boost::any const& data) override {
        if (lValue.isArrayAccess()) {
            FunctionEliminationExpressionVisitor* functionEliminationVisitor = boost::any_cast<FunctionEliminationExpressionVisitor*>(data);
            std::vector<storm::expressions::Expression> arrayIndex;
            for (auto const& indexExpr : lValue.getArrayIndexVector()) {
                arrayIndex.push_back(functionEliminationVisitor->eliminate(indexExpr));
            }
            lValue.setArrayIndex(arrayIndex);
        }
    }

    void traverse(storm::expressions::Expression const& expression, boost::any const&) override {
        STORM_LOG_THROW(getOccurringFunctionCalls(expression).empty(), storm::exceptions::UnexpectedException,
                        "Did not translate functions in expression " << expression << ".");
    }
};
}  // namespace detail

void eliminateFunctions(Model& model, std::vector<Property>& properties) {
    // Only perform actions if there actually are functions.
    if (model.getModelFeatures().hasFunctions()) {
        detail::FunctionEliminatorTraverser().eliminate(model, properties);
    }
    STORM_LOG_ASSERT(!containsFunctionCallExpression(model), "The model still seems to contain function calls.");
}

storm::expressions::Expression eliminateFunctionCallsInExpression(storm::expressions::Expression const& expression, Model const& model) {
    if (model.getModelFeatures().hasFunctions()) {
        detail::FunctionEliminationExpressionVisitor visitor(&model.getGlobalFunctionDefinitions());
        return visitor.eliminate(expression);
    } else {
        return expression;
    }
}
}  // namespace jani
}  // namespace storm
