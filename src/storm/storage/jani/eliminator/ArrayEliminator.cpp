#include "ArrayEliminator.h"

#include <unordered_map>

#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/Variable.h"
#include "storm/storage/jani/traverser/ArrayExpressionFinder.h"
#include "storm/storage/jani/traverser/JaniTraverser.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace jani {

namespace detail {
class ArrayReplacementsCollectorExpressionVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;

    typedef typename ArrayEliminatorData::Replacement Replacement;
    typedef std::unordered_map<storm::expressions::Variable, Replacement> ReplMap;

    ArrayReplacementsCollectorExpressionVisitor(Model& model) : model(model), converged(false), declaringAutomaton(nullptr) {};
    virtual ~ArrayReplacementsCollectorExpressionVisitor() = default;

    /*!
     * Adds necessary replacements for the given array variable, assuming that the given expression is assigned to the array variable.
     * @returns true iff no replacement had to be added
     */
    bool get(storm::expressions::Expression const& expression, storm::jani::Variable const& arrayVariable, Replacement& currentReplacement,
             ReplMap const& allCollectedReplacements, Automaton* declaringAutomaton = nullptr, std::vector<std::size_t>* arrayAccessIndices = nullptr) {
        currentVar = &arrayVariable;
        this->declaringAutomaton = declaringAutomaton;
        STORM_LOG_ASSERT(currentVar->getType().isArrayType(), "expected array variable");
        STORM_LOG_ASSERT(((declaringAutomaton == nullptr && model.hasGlobalVariable(currentVar->getName())) ||
                          (declaringAutomaton != nullptr && declaringAutomaton->hasVariable(currentVar->getName()))),
                         "Cannot find variable declaration.");
        collectedRepl = &allCollectedReplacements;

        std::vector<std::size_t> indices;
        std::vector<std::size_t>* indicesPtr;
        if (arrayAccessIndices) {
            indicesPtr = arrayAccessIndices;
        } else {
            indicesPtr = &indices;
        }

        converged = true;
        expression.accept(*this, std::make_pair(&currentReplacement, indicesPtr));
        return converged;
    }

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        if (expression.getCondition()->containsVariables()) {
            expression.getThenExpression()->accept(*this, data);
            expression.getElseExpression()->accept(*this, data);
        } else {
            if (expression.getCondition()->evaluateAsBool()) {
                expression.getThenExpression()->accept(*this, data);
            } else {
                expression.getElseExpression()->accept(*this, data);
            }
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    void varToVarAssignmentHelper(Replacement& cur, Replacement const& other, std::vector<std::size_t>& indices) {
        if (other.isVariable()) {
            if (!cur.isVariable()) {
                STORM_LOG_ASSERT(cur.size() == 0, "Unexpected replacement type. Assignments incompatible?");
                cur = Replacement(addReplacementVariable(indices));
            }
        } else {
            STORM_LOG_ASSERT(!cur.isVariable(), "Unexpected replacement type. Assignments incompatible?");
            cur.grow(other.size());
            indices.push_back(0);
            for (std::size_t i = 0; i < other.size(); ++i) {
                indices.back() = i;
                varToVarAssignmentHelper(cur.at(i), other.at(i), indices);
            }
            indices.pop_back();
        }
    }

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data) override {
        if (expression.getType().isArrayType()) {
            auto varIt = collectedRepl->find(expression.getVariable());
            if (varIt != collectedRepl->end()) {
                // We have to make sure that the sizes for the current replacements fit at least the number of other replacements
                auto current = boost::any_cast<std::pair<Replacement*, std::vector<std::size_t>*>>(data);
                varToVarAssignmentHelper(*current.first, varIt->second, *current.second);
            }
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const&, boost::any const&) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const&, boost::any const&) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const&, boost::any const&) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

    void arrayExpressionHelper(storm::expressions::ArrayExpression const& expression, Replacement& curRepl, std::vector<size_t>& curIndices) {
        STORM_LOG_ASSERT(!expression.size()->containsVariables(), "Did not expect variables in size expression.");
        auto expSize = static_cast<uint64_t>(expression.size()->evaluateAsInt());
        STORM_LOG_ASSERT(!curRepl.isVariable(), "Unexpected replacement type. Illegal array assignment?");
        curRepl.grow(expSize);
        curIndices.push_back(0);
        for (std::size_t i = 0; i < expSize; ++i) {
            curIndices.back() = i;
            auto subexpr = expression.at(i);
            if (subexpr->getType().isArrayType()) {
                // This is a nested array.
                subexpr->accept(*this, std::make_pair(&curRepl.at(i), &curIndices));
            } else {
                if (!curRepl.at(i).isVariable()) {
                    STORM_LOG_ASSERT(curRepl.at(i).size() == 0, "Unexpected replacement type. Illegal array assignment?");
                    curRepl.at(i) = Replacement(addReplacementVariable(curIndices));
                }
            }
        }
        curIndices.pop_back();
    }

    virtual boost::any visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(expression.size()->isIntegerLiteralExpression(),
                         "unexpected kind of size expression of ValueArrayExpression (" << expression.size()->toExpression() << ").");
        auto current = boost::any_cast<std::pair<Replacement*, std::vector<std::size_t>*>>(data);
        arrayExpressionHelper(expression, *current.first, *current.second);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) override {
        if (!expression.size()->containsVariables()) {
            auto current = boost::any_cast<std::pair<Replacement*, std::vector<std::size_t>*>>(data);
            arrayExpressionHelper(expression, *current.first, *current.second);
        } else {
            auto vars = expression.size()->toExpression().getVariables();
            std::string variables = "";
            for (auto const& v : vars) {
                if (variables != "") {
                    variables += ", ";
                }
                variables += v.getName();
            }
            if (vars.size() == 1) {
                variables = "variable " + variables;
            } else {
                variables = "variables " + variables;
            }
            STORM_LOG_THROW(
                false, storm::exceptions::NotSupportedException,
                "Unable to determine array size: Size of ConstructorArrayExpression '" << expression << "' still contains the " << variables << ".");
        }
        return boost::any();
    }

    void arrayVariableAccessHelper(std::vector<std::shared_ptr<storm::expressions::BaseExpression const>>& indexStack, Replacement& cur,
                                   Replacement const& other, std::vector<std::size_t>& curIndices) {
        if (indexStack.empty()) {
            varToVarAssignmentHelper(cur, other, curIndices);
        } else {
            uint64_t i, size;
            if (indexStack.back()->containsVariables()) {
                // We have to run over all possible indices
                i = 0;
                size = other.size();
            } else {
                // only consider the accessed index
                i = indexStack.back()->evaluateAsInt();
                size = std::min<uint64_t>(i + 1, other.size());  // Avoid invalid access, e.g. if the sizes of the other variable aren't ready yet
            }
            indexStack.pop_back();
            for (; i < size; ++i) {
                arrayVariableAccessHelper(indexStack, cur, other.at(i), curIndices);
            }
        }
    }

    void arrayAccessHelper(std::shared_ptr<storm::expressions::BaseExpression const> const& base,
                           std::vector<std::shared_ptr<storm::expressions::BaseExpression const>>& indexStack, boost::any const& data) {
        // We interpret the input as expression base[i_n][i_{n-1}]...[i_1] for (potentially empty) indexStack = [i_1, ..., i_n].
        // This deals with *nested* array accesses
        auto baseAsArrayAccessExp = std::dynamic_pointer_cast<storm::expressions::ArrayAccessExpression const>(base);
        auto baseAsArrayExp = std::dynamic_pointer_cast<storm::expressions::ArrayExpression const>(base);
        if (indexStack.empty()) {
            base->accept(*this, data);
        } else if (baseAsArrayAccessExp) {
            indexStack.push_back(baseAsArrayAccessExp->getSecondOperand());
            arrayAccessHelper(baseAsArrayAccessExp->getFirstOperand(), indexStack, data);
        } else if (baseAsArrayExp) {
            // the base is an array expression.
            uint64_t i, size;
            if (indexStack.back()->containsVariables()) {
                // We have to run over all possible indices
                STORM_LOG_THROW(
                    !baseAsArrayExp->size()->containsVariables(), storm::exceptions::NotSupportedException,
                    "Array elimination failed because array access expression considers  array  expression '" << *baseAsArrayExp << "' of unknown size.");
                i = 0;
                size = baseAsArrayExp->size()->evaluateAsInt();
            } else {
                // only consider the accessed index
                i = indexStack.back()->evaluateAsInt();
                size = i + 1;
                STORM_LOG_THROW(baseAsArrayExp->size()->containsVariables() || i < static_cast<uint64_t>(baseAsArrayExp->size()->evaluateAsInt()),
                                storm::exceptions::InvalidOperationException, "Array access " << base << "[" << i << "] out of bounds.");
            }
            indexStack.pop_back();
            for (; i < size; ++i) {
                auto indexStackCpy = indexStack;
                arrayAccessHelper(baseAsArrayExp->at(i), indexStackCpy, data);
            }
        } else if (base->isVariableExpression()) {
            // the lhs is an array variable.
            STORM_LOG_ASSERT(base->asVariableExpression().getVariable().getType().isArrayType(), "Unexpected variable type in array access.");
            auto varIt = collectedRepl->find(base->asVariableExpression().getVariable());
            if (varIt != collectedRepl->end()) {
                STORM_LOG_ASSERT(!varIt->second.isVariable(), "Unexpected replacement type. Invalid array assignment?");
                auto current = boost::any_cast<std::pair<Replacement*, std::vector<std::size_t>*>>(data);
                arrayVariableAccessHelper(indexStack, *current.first, varIt->second, *current.second);
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected kind of array access expression.");
        }
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) override {
        std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> indexStack = {expression.getSecondOperand()};
        arrayAccessHelper(expression.getFirstOperand(), indexStack, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const&, boost::any const&) override {
        STORM_LOG_THROW(
            false, storm::exceptions::UnexpectedException,
            "Found Function call expression within an array expression. This is not expected since functions are expected to be eliminated at this point.");
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const&, boost::any const&) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of expression.");
        return boost::any();
    }

   private:
    storm::jani::Variable const& addReplacementVariable(std::vector<std::size_t> const& indices) {
        auto& manager = model.getExpressionManager();
        std::string name = currentVar->getExpressionVariable().getName();
        for (auto const& i : indices) {
            name += "_at_" + std::to_string(i);
        }

        storm::expressions::Expression initValue;
        if (currentVar->hasInitExpression()) {
            initValue = currentVar->getInitExpression();
            for (auto const& i : indices) {
                auto aa = std::make_shared<storm::expressions::ArrayAccessExpression>(
                    manager, initValue.getType().getElementType(), initValue.getBaseExpressionPointer(), manager.integer(i).getBaseExpressionPointer());
                initValue = storm::expressions::Expression(aa);
            }
        }

        auto const& baseType = currentVar->getType().asArrayType().getBaseTypeRecursive();
        storm::expressions::Variable exprVariable;
        if (baseType.isBasicType()) {
            switch (baseType.asBasicType().get()) {
                case BasicType::Type::Bool:
                    exprVariable = manager.declareBooleanVariable(name);
                    break;
                case BasicType::Type::Int:
                    exprVariable = manager.declareIntegerVariable(name);
                    break;
                case BasicType::Type::Real:
                    exprVariable = manager.declareRationalVariable(name);
                    break;
            }
        } else if (baseType.isBoundedType()) {
            switch (baseType.asBoundedType().getBaseType()) {
                case BoundedType::BaseType::Int:
                    exprVariable = manager.declareIntegerVariable(name);
                    break;
                case BoundedType::BaseType::Real:
                    exprVariable = manager.declareRationalVariable(name);
                    break;
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unhandled base type for array of type " << currentVar->getType() << ".");
        }

        auto janiVar = storm::jani::Variable::makeVariable(name, baseType, exprVariable, initValue, currentVar->isTransient());

        converged = false;
        if (declaringAutomaton) {
            return declaringAutomaton->addVariable(*janiVar);
        } else {
            return model.addVariable(*janiVar);
        }
    }

    Model& model;
    bool converged;
    jani::Variable const* currentVar;
    Automaton* declaringAutomaton;  // nullptr if currentVar is global
    ReplMap const* collectedRepl;
};

/*!
 * Gets the data necessary for array elimination.
 * Also adds replacement variables to the model and removes array variables from the model.
 */
class ArrayEliminatorDataCollector : public JaniTraverser {
   public:
    ArrayEliminatorDataCollector(Model& model) : model(model), currentAutomaton(nullptr), exprVisitor(model), converged(false) {}
    virtual ~ArrayEliminatorDataCollector() = default;

    ArrayEliminatorData get() {
        // To correctly determine the array sizes, we repeat the steps until convergence. This is to cover assignments of one array variable to another (A := B)
        ArrayEliminatorData result;
        do {
            converged = true;
            JaniTraverser::traverse(model, &result);
        } while (!converged);

        // drop all occurring array variables
        auto elVars = model.getGlobalVariables().dropAllArrayVariables();
        result.eliminatedArrayVariables.insert(result.eliminatedArrayVariables.end(), elVars.begin(), elVars.end());
        for (auto& automaton : model.getAutomata()) {
            elVars = automaton.getVariables().dropAllArrayVariables();
            result.eliminatedArrayVariables.insert(result.eliminatedArrayVariables.end(), elVars.begin(), elVars.end());
        }

        return result;
    }

    virtual void traverse(Automaton& automaton, boost::any const& data) override {
        currentAutomaton = &automaton;
        JaniTraverser::traverse(automaton, data);
        currentAutomaton = nullptr;
    }

    Automaton* declaringAutomaton(Variable const& var) const {
        if (currentAutomaton && currentAutomaton->getVariables().hasVariable(var)) {
            return currentAutomaton;
        }
        STORM_LOG_ASSERT(model.getGlobalVariables().hasVariable(var), "Variable " << var.getName() << " not found.");
        return nullptr;
    }

    virtual void traverse(VariableSet& variableSet, boost::any const& data) override {
        // *Only* traverse array variables.
        // We do this to make the adding of new replacements (which adds new non-array variables to the model) a bit more safe.
        // (Adding new elements to a container while iterating over it might be a bit dangerous, depending on the container)
        for (auto& v : variableSet.getArrayVariables()) {
            traverse(v, data);
        }
    }

    void gatherArrayAccessIndices(std::vector<std::vector<std::size_t>>& gatheredIndices, std::vector<std::size_t>& current,
                                  typename ArrayEliminatorData::Replacement const& repl, std::vector<storm::expressions::Expression> const& indices) {
        STORM_LOG_ASSERT(!repl.isVariable(), "Unexpected replacement type. Assignments incompatible?");
        if (current.size() < indices.size()) {
            auto const& indexExp = indices[current.size()];
            if (indexExp.containsVariables()) {
                current.push_back(std::numeric_limits<std::size_t>::max());
                // We have to consider all possible indices
                for (std::size_t i = 0; i < repl.size(); ++i) {
                    current.back() = i;
                    gatherArrayAccessIndices(gatheredIndices, current, repl.at(i), indices);
                }
                current.pop_back();
            } else {
                auto i = static_cast<uint64_t>(indexExp.evaluateAsInt());
                if (i < repl.size()) {
                    current.push_back(i);
                    gatherArrayAccessIndices(gatheredIndices, current, repl.at(i), indices);
                    current.pop_back();
                }  // The else case might either be an invalid array access or we haven't processed the corresponding assignment yet (and thus will be covered
                   // in a subsequent run)
            }
        } else {
            gatheredIndices.push_back(current);
        }
    }

    virtual void traverse(Assignment& assignment, boost::any const& data) override {
        if (assignment.getLValue().isArray()) {
            auto const& var = assignment.getLValue().getVariable();
            if (!assignment.getLValue().isArrayAccess()) {
                // Variable assignment
                auto& result = *boost::any_cast<ArrayEliminatorData*>(data);
                converged &= exprVisitor.get(assignment.getAssignedExpression(), var, result.replacements[var.getExpressionVariable()], result.replacements,
                                             declaringAutomaton(var));
            } else if (!assignment.getLValue().isFullArrayAccess()) {
                // Incomplete array assignment
                auto& result = *boost::any_cast<ArrayEliminatorData*>(data);
                auto& arrayVarReplacements = result.replacements[var.getExpressionVariable()];  // creates if it does not yet exist

                // If the indices are not constant expressions, we run through every possible combination of indices
                std::vector<std::vector<std::size_t>> gatheredIndices;
                std::vector<std::size_t> tmp;
                gatherArrayAccessIndices(gatheredIndices, tmp, result.replacements[var.getExpressionVariable()], assignment.getLValue().getArrayIndexVector());
                for (auto& indices : gatheredIndices) {
                    converged &= exprVisitor.get(assignment.getAssignedExpression(), var, arrayVarReplacements.at(indices), result.replacements,
                                                 declaringAutomaton(var), &indices);
                }
            }
        }
    }

    virtual void traverse(Variable& variable, boost::any const& data) override {
        STORM_LOG_ASSERT(variable.getType().isArrayType(), "Expected to only traverse over array variables.");
        auto& result = *boost::any_cast<ArrayEliminatorData*>(data);
        if (variable.hasInitExpression()) {
            converged &= exprVisitor.get(variable.getInitExpression(), variable, result.replacements[variable.getExpressionVariable()], result.replacements,
                                         declaringAutomaton(variable));
        }
    }

   private:
    Model& model;
    Automaton* currentAutomaton;
    ArrayReplacementsCollectorExpressionVisitor exprVisitor;
    bool converged;
};

/// Eliminates the array accesses in the given expression, for example  ([[1],[2,3]])[i][j]  --> i=0 ? [1][j] : [2,3][j] --> i=0 ? 1 : (j=0 ? 2 : 3)
class ArrayExpressionEliminationVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;

    typedef std::shared_ptr<storm::expressions::BaseExpression const> BaseExprPtr;
    class ResultType {
       public:
        ResultType(ResultType const& other) = default;
        ResultType(BaseExprPtr expression) : expression(expression) {}
        ResultType() {}
        BaseExprPtr& expr() {
            STORM_LOG_ASSERT(!isArrayOutOfBounds(), "Tried to get the result expression, but the expression is out-of-bounds");
            return expression;
        };
        bool isArrayOutOfBounds() {
            return expression.get() == nullptr;
        };

       private:
        BaseExprPtr expression;
    };

    typedef std::vector<storm::expressions::Expression> ArrayAccessIndices;

    ArrayExpressionEliminationVisitor(ArrayEliminatorData const& data) : replacements(data.replacements) {}

    ArrayAccessIndices const& getArrayAccessIndices(boost::any const& data) {
        STORM_LOG_ASSERT(!data.empty(), "tried to convert data but it is empty.");
        return *boost::any_cast<ArrayAccessIndices*>(data);
    }

    storm::expressions::Expression eliminate(storm::expressions::Expression const& expression) {
        auto res = eliminateChkSizes(expression);
        STORM_LOG_THROW(!res.isArrayOutOfBounds(), storm::exceptions::OutOfRangeException,
                        "Cannot eliminate expression " << expression << " as it is out-of-bounds.");
        return res.expr();
    }

    ResultType eliminateChkSizes(storm::expressions::Expression const& expression) {
        // We use the data parameter of the visitors to keep track of the array access indices.
        // If data is empty, there currently are no array accesses. Otherwise, it is a vector where the last entry corresponds to the inner most access, i.e., 1
        // in a[1][2]
        ArrayAccessIndices arrayAccessIndices;
        auto res = boost::any_cast<ResultType>(expression.accept(*this, &arrayAccessIndices));
        if (!res.isArrayOutOfBounds()) {
            STORM_LOG_ASSERT(!containsArrayExpression(res.expr()->toExpression()), "Expression still contains array expressions. Before: \n"
                                                                                       << expression << "\nAfter:\n"
                                                                                       << res.expr()->toExpression());
            res.expr() = res.expr()->simplify();
        }

        return res;
    }

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        // for the condition expression, array access indices do not matter. We can still have array accesses inside, though
        ArrayAccessIndices conditionArrayAccessIndices;
        ResultType conditionResult = boost::any_cast<ResultType>(expression.getCondition()->accept(*this, &conditionArrayAccessIndices));
        if (conditionResult.isArrayOutOfBounds()) {
            return conditionResult;
        }

        // We need to handle expressions of the kind '42<size ? A[42] : 0', where size is a variable and A[42] might be out of bounds.
        ResultType thenResult = boost::any_cast<ResultType>(expression.getThenExpression()->accept(*this, data));
        ResultType elseResult = boost::any_cast<ResultType>(expression.getElseExpression()->accept(*this, data));
        if (thenResult.isArrayOutOfBounds()) {
            if (elseResult.isArrayOutOfBounds()) {
                return thenResult;
            } else {
                // Assume the else expression
                return elseResult;
            }
        } else if (elseResult.isArrayOutOfBounds()) {
            // Assume the then expression
            return thenResult;
        } else {
            // If the arguments did not change, we simply push the expression itself.
            if (conditionResult.expr().get() == expression.getCondition().get() && thenResult.expr().get() == expression.getThenExpression().get() &&
                elseResult.expr().get() == expression.getElseExpression().get()) {
                return ResultType(expression.getSharedPointer());
            } else {
                return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                    std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::IfThenElseExpression(
                        expression.getManager(), thenResult.expr()->getType(), conditionResult.expr(), thenResult.expr(), elseResult.expr()))));
            }
        }
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(getArrayAccessIndices(data).empty(),
                         "BinaryBooleanFunctionExpressions should not be direct subexpressions of array access expressions. However, the expression "
                             << expression << " is.");
        ResultType firstResult = boost::any_cast<ResultType>(expression.getFirstOperand()->accept(*this, data));
        ResultType secondResult = boost::any_cast<ResultType>(expression.getSecondOperand()->accept(*this, data));

        if (firstResult.isArrayOutOfBounds()) {
            return firstResult;
        } else if (secondResult.isArrayOutOfBounds()) {
            return secondResult;
        }

        // If the arguments did not change, we simply push the expression itself.
        if (firstResult.expr().get() == expression.getFirstOperand().get() && secondResult.expr().get() == expression.getSecondOperand().get()) {
            return ResultType(expression.getSharedPointer());
        } else {
            return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryBooleanFunctionExpression(
                    expression.getManager(), expression.getType(), firstResult.expr(), secondResult.expr(), expression.getOperatorType()))));
        }
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(getArrayAccessIndices(data).empty(),
                         "BinaryNumericalFunctionExpression should not be direct subexpressions of array access expressions. However, the expression "
                             << expression << " is.");
        ResultType firstResult = boost::any_cast<ResultType>(expression.getFirstOperand()->accept(*this, data));
        ResultType secondResult = boost::any_cast<ResultType>(expression.getSecondOperand()->accept(*this, data));

        if (firstResult.isArrayOutOfBounds()) {
            return firstResult;
        } else if (secondResult.isArrayOutOfBounds()) {
            return secondResult;
        }

        // If the arguments did not change, we simply push the expression itself.
        if (firstResult.expr().get() == expression.getFirstOperand().get() && secondResult.expr().get() == expression.getSecondOperand().get()) {
            return ResultType(expression.getSharedPointer());
        } else {
            return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryNumericalFunctionExpression(
                    expression.getManager(), expression.getType(), firstResult.expr(), secondResult.expr(), expression.getOperatorType()))));
        }
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(
            getArrayAccessIndices(data).empty(),
            "BinaryRelationExpression should not be direct subexpressions of array access expressions. However, the expression " << expression << " is.");
        ResultType firstResult = boost::any_cast<ResultType>(expression.getFirstOperand()->accept(*this, data));
        ResultType secondResult = boost::any_cast<ResultType>(expression.getSecondOperand()->accept(*this, data));

        if (firstResult.isArrayOutOfBounds()) {
            return firstResult;
        } else if (secondResult.isArrayOutOfBounds()) {
            return secondResult;
        }

        // If the arguments did not change, we simply push the expression itself.
        if (firstResult.expr().get() == expression.getFirstOperand().get() && secondResult.expr().get() == expression.getSecondOperand().get()) {
            return ResultType(expression.getSharedPointer());
        } else {
            return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::BinaryRelationExpression(
                    expression.getManager(), expression.getType(), firstResult.expr(), secondResult.expr(), expression.getRelationType()))));
        }
    }

    storm::expressions::Expression varElimHelper(typename ArrayEliminatorData::Replacement const& replacement, ArrayAccessIndices const& indices,
                                                 uint64_t const& pos) {
        if (pos == 0) {
            STORM_LOG_ASSERT(replacement.isVariable(), "Replacement is not a variable. Is this a full array access?");
            return replacement.getVariable().getExpressionVariable().getExpression();
        } else {
            STORM_LOG_ASSERT(!replacement.isVariable(), "Are there too many nested array accesses?");
            auto indexExpr = indices[pos - 1];
            storm::expressions::Expression result;
            if (indexExpr.containsVariables()) {
                for (uint64_t index = 0; index < replacement.size(); ++index) {
                    auto child = varElimHelper(replacement.at(index), indices, pos - 1);
                    if (child.isInitialized()) {  // i.e. there is no out-of-bounds situation for the child
                        if (result.isInitialized()) {
                            auto currIndexExpr = (indexExpr == indexExpr.getManager().integer(index));
                            result = storm::expressions::ite(currIndexExpr, child, result);
                        } else {
                            result = child;
                        }
                    }
                }
                // At this point result remains uninitialized iff all childs are uninitialized (i.e. out-of-bounds).
                // The underlying assumption here is that indexExpr will never evaluate to an index where the access is out-of-bounds.
                return result;
            } else {
                auto index = static_cast<uint64_t>(indexExpr.evaluateAsInt());
                if (index < replacement.size()) {
                    return varElimHelper(replacement.at(index), indices, pos - 1);
                } else {
                    // We have an out-of-bounds situation
                    return storm::expressions::Expression();
                }
            }
        }
    }

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data) override {
        if (expression.getType().isArrayType()) {
            auto const& indices = getArrayAccessIndices(data);
            STORM_LOG_THROW(!indices.empty(), storm::exceptions::NotSupportedException,
                            "Unable to translate array variable to basic variable, since it does not seem to be within an array access expression.");
            STORM_LOG_ASSERT(replacements.count(expression.getVariable()) > 0, "Unable to find array variable " << expression << " in array replacements.");
            auto const& arrayVarReplacements = replacements.at(expression.getVariable());
            auto result = varElimHelper(arrayVarReplacements, indices, indices.size());
            // Try to get the replacement variable
            if (result.isInitialized()) {
                return ResultType(result.getBaseExpressionPointer());
            } else {
                return ResultType();
            }
        } else {
            STORM_LOG_ASSERT(getArrayAccessIndices(data).empty(),
                             "VariableExpression of non-array variable should not be a subexpressions of array access expressions. However, the expression "
                                 << expression << " is.");
            return ResultType(expression.getSharedPointer());
        }
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(
            getArrayAccessIndices(data).empty(),
            "UnaryBooleanFunctionExpression should not be direct subexpressions of array access expressions. However, the expression " << expression << " is.");
        ResultType operandResult = boost::any_cast<ResultType>(expression.getOperand()->accept(*this, data));

        if (operandResult.isArrayOutOfBounds()) {
            return operandResult;
        }

        // If the argument did not change, we simply push the expression itself.
        if (operandResult.expr().get() == expression.getOperand().get()) {
            return ResultType(expression.getSharedPointer());
        } else {
            return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::UnaryBooleanFunctionExpression(
                    expression.getManager(), expression.getType(), operandResult.expr(), expression.getOperatorType()))));
        }
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(
            getArrayAccessIndices(data).empty(),
            "UnaryBooleanFunctionExpression should not be direct subexpressions of array access expressions. However, the expression " << expression << " is.");

        ResultType operandResult = boost::any_cast<ResultType>(expression.getOperand()->accept(*this, data));

        if (operandResult.isArrayOutOfBounds()) {
            return operandResult;
        }

        // If the argument did not change, we simply push the expression itself.
        if (operandResult.expr().get() == expression.getOperand().get()) {
            return ResultType(expression.getSharedPointer());
        } else {
            return ResultType(std::const_pointer_cast<storm::expressions::BaseExpression const>(
                std::shared_ptr<storm::expressions::BaseExpression>(new storm::expressions::UnaryNumericalFunctionExpression(
                    expression.getManager(), expression.getType(), operandResult.expr(), expression.getOperatorType()))));
        }
    }

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) override {
        return ResultType(expression.getSharedPointer());
    }

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) override {
        return ResultType(expression.getSharedPointer());
    }

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) override {
        return ResultType(expression.getSharedPointer());
    }

    boost::any arrayExprHelper(storm::expressions::ArrayExpression const& expression, boost::any const& data) {
        auto& indices = getArrayAccessIndices(data);
        STORM_LOG_THROW(!indices.empty(), storm::exceptions::NotSupportedException,
                        "Unable to translate ValueArrayExpression to element expression since it does not seem to be within an array access expression.");
        auto indexExpr = indices.back();
        auto childIndices = indices;
        childIndices.pop_back();
        if (indexExpr.containsVariables()) {
            STORM_LOG_THROW(!expression.size()->containsVariables(), storm::exceptions::NotSupportedException,
                            "Unable to eliminate array expression of unknown size.");
            auto exprSize = static_cast<uint64_t>(expression.size()->evaluateAsInt());
            storm::expressions::Expression result;
            for (uint64_t index = 0; index < exprSize; ++index) {
                auto child = boost::any_cast<ResultType>(expression.at(index)->accept(*this, &childIndices));
                if (!child.isArrayOutOfBounds()) {
                    if (result.isInitialized()) {
                        auto currIndexExpr = (indexExpr == indexExpr.getManager().integer(index));
                        result = storm::expressions::ite(currIndexExpr, child.expr()->toExpression(), result);
                    } else {
                        result = child.expr()->toExpression();
                    }
                }
            }
            if (result.isInitialized()) {
                return ResultType(result.getBaseExpressionPointer());
            } else {
                return ResultType();
            }
        } else {
            auto index = indexExpr.evaluateAsInt();
            if (expression.size()->containsVariables()) {
                STORM_LOG_WARN("ignoring size of array expression " << expression << " as it is not constant.");
            } else {
                if (index >= expression.size()->evaluateAsInt()) {
                    return ResultType();
                }
            }
            return expression.at(index)->accept(*this, &childIndices);
        }
    }

    virtual boost::any visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(expression.size()->isIntegerLiteralExpression(),
                         "unexpected kind of size expression of ValueArrayExpression (" << expression.size()->toExpression() << ").");
        return arrayExprHelper(expression, data);
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) override {
        return arrayExprHelper(expression, data);
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) override {
        // Eliminate arrays in the index expression e.g. for a[b[i]]
        ArrayAccessIndices aaIndicesInIndexExpr;
        auto indexExprResult = boost::any_cast<ResultType>(expression.getSecondOperand()->accept(*this, &aaIndicesInIndexExpr));
        if (indexExprResult.isArrayOutOfBounds()) {
            return indexExprResult;
        }
        auto& indices = getArrayAccessIndices(data);
        auto childIndices = indices;
        childIndices.push_back(indexExprResult.expr()->toExpression());
        return expression.getFirstOperand()->accept(*this, &childIndices);
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const&, boost::any const&) override {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                        "Found Function call expression while eliminating array expressions. This is not expected since functions are expected to be "
                        "eliminated at this point.");
        return false;
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const& expression, boost::any const&) override {
        return ResultType(expression.getSharedPointer());
    }

   private:
    std::unordered_map<storm::expressions::Variable, typename ArrayEliminatorData::Replacement> const& replacements;
};

class ArrayVariableReplacer : public JaniTraverser {
   public:
    ArrayVariableReplacer(ArrayEliminatorData const& data, bool keepNonTrivialArrayAccess = false)
        : elimData(data), arrayExprEliminator(data), keepNonTrivialArrayAccess(keepNonTrivialArrayAccess) {}

    virtual ~ArrayVariableReplacer() = default;
    void replace(Model& model) {
        traverse(model, boost::any());
    }

    virtual void traverse(Model& model, boost::any const& data) override {
        // Traverse relevant model components
        for (auto& c : model.getConstants()) {
            traverse(c, data);
        }
        STORM_LOG_ASSERT(model.getGlobalFunctionDefinitions().empty(), "Expected functions to be eliminated at this point");
        JaniTraverser::traverse(model.getGlobalVariables(), data);
        for (auto& aut : model.getAutomata()) {
            traverse(aut, data);
        }

        // Replace expressions
        if (model.hasInitialStatesRestriction()) {
            model.setInitialStatesRestriction(arrayExprEliminator.eliminate(model.getInitialStatesRestriction()));
        }
        for (auto& nonTrivRew : model.getNonTrivialRewardExpressions()) {
            nonTrivRew.second = arrayExprEliminator.eliminate(nonTrivRew.second);
        }
    }

    virtual void traverse(Automaton& automaton, boost::any const& data) override {
        JaniTraverser::traverse(automaton.getVariables(), data);
        STORM_LOG_ASSERT(automaton.getFunctionDefinitions().empty(), "Expected functions to be eliminated at this point");
        for (auto& loc : automaton.getLocations()) {
            traverse(loc, data);
        }
        JaniTraverser::traverse(automaton.getEdgeContainer(), data);
        if (automaton.hasInitialStatesRestriction()) {
            automaton.setInitialStatesRestriction(arrayExprEliminator.eliminate(automaton.getInitialStatesRestriction()));
        }
    }

    void traverse(Constant& constant, boost::any const& data) override {
        // There are no array constants in the JANI standard.
        // Still, we might have some constant array expressions with (constant) array access here.
        if (constant.isDefined()) {
            constant.define(arrayExprEliminator.eliminate(constant.getExpression()));
        }
        if (constant.hasConstraint()) {
            constant.setConstraintExpression(arrayExprEliminator.eliminate(constant.getConstraintExpression()));
        }
    }

    virtual void traverse(Location& location, boost::any const& data) override {
        traverse(location.getAssignments(), data);
        if (location.hasTimeProgressInvariant()) {
            location.setTimeProgressInvariant(arrayExprEliminator.eliminate(location.getTimeProgressInvariant()));
        }
    }

    void traverse(Variable& variable, boost::any const& data) override {
        // It's important to traverse the type first as we might be using expressions within the type in getOutOfBoundsValue
        traverse(variable.getType(), data);

        if (variable.hasInitExpression()) {
            auto eliminationRes = arrayExprEliminator.eliminateChkSizes(variable.getInitExpression());
            if (eliminationRes.isArrayOutOfBounds()) {
                // We have an out-of-bounds situation. This is expected when we have an incomplete array access
                // For example, when we set a:=b where b is smaller than the maximal size of a
                variable.setInitExpression(getOutOfBoundsValue(variable));
            } else {
                variable.setInitExpression(eliminationRes.expr());
            }
        }
    }

    void traverse(JaniType& type, boost::any const& data) override {
        STORM_LOG_ASSERT(!type.isArrayType(), "did not expect any array variable declarations at this point.");
        if (type.isBoundedType()) {
            auto& boundedType = type.asBoundedType();
            if (boundedType.hasLowerBound()) {
                boundedType.setLowerBound(arrayExprEliminator.eliminate(boundedType.getLowerBound()));
            }
            if (boundedType.hasUpperBound()) {
                boundedType.setUpperBound(arrayExprEliminator.eliminate(boundedType.getUpperBound()));
            }
        }
    }

    void traverse(TemplateEdge& templateEdge, boost::any const& data) override {
        templateEdge.setGuard(arrayExprEliminator.eliminate(templateEdge.getGuard()));
        for (auto& dest : templateEdge.getDestinations()) {
            JaniTraverser::traverse(dest, data);
        }
        traverse(templateEdge.getAssignments(), data);
    }

    void traverse(Edge& edge, boost::any const& data) override {
        if (edge.hasRate()) {
            edge.setRate(arrayExprEliminator.eliminate(edge.getRate()));
        }
        for (auto& dest : edge.getDestinations()) {
            traverse(dest, data);
        }
    }

    void traverse(EdgeDestination& edgeDestination, boost::any const&) override {
        edgeDestination.setProbability(arrayExprEliminator.eliminate(edgeDestination.getProbability()));
    }

    virtual void traverse(OrderedAssignments& orderedAssignments, boost::any const& data) override {
        // Replace array occurrences in LValues and assigned expressions.
        std::vector<Assignment> newAssignments;
        if (!orderedAssignments.empty()) {
            int64_t level = orderedAssignments.getLowestLevel();
            // Collect for each array variable all kinds of assignments to that variable on the current level
            std::unordered_map<storm::expressions::Variable, std::vector<Assignment const*>> collectedArrayAssignments;

            auto processCollectedAssignments = [&]() {
                for (auto& arrayAssignments : collectedArrayAssignments) {
                    handleArrayAssignments(arrayAssignments.second, level, elimData.replacements.at(arrayAssignments.first), newAssignments);
                }
                collectedArrayAssignments.clear();
            };

            for (Assignment const& assignment : orderedAssignments) {
                if (assignment.getLevel() != level) {
                    STORM_LOG_ASSERT(assignment.getLevel() > level, "Ordered Assignment does not have the expected order.");
                    processCollectedAssignments();
                    level = assignment.getLevel();
                }

                if (assignment.getLValue().isArray()) {
                    auto insertionRes = collectedArrayAssignments.emplace(assignment.getVariable().getExpressionVariable(), std::vector<Assignment const*>());
                    insertionRes.first->second.push_back(&assignment);
                } else {
                    // In this case we have a non-array variable, we don't need to eliminate anything in the lhs
                    newAssignments.emplace_back(assignment.getLValue(), arrayExprEliminator.eliminate(assignment.getAssignedExpression()),
                                                assignment.getLevel());
                }
            }
            processCollectedAssignments();

            // Now insert new assignments
            orderedAssignments.clear();
            for (auto const& assignment : newAssignments) {
                orderedAssignments.add(assignment);
                STORM_LOG_ASSERT(!storm::jani::containsArrayExpression(assignment.getAssignedExpression()),
                                 "Assignment still contains array expression: " << assignment);
            }
        }
    }

   private:
    template<class InsertionCallback>
    void insertArrayAssignmentReplacements(std::vector<storm::expressions::Expression> const& aaIndices, uint64_t const& currDepth,
                                           typename ArrayEliminatorData::Replacement const& currReplacement, storm::expressions::Expression const& currRhs,
                                           storm::expressions::Expression const& currCondition, InsertionCallback const& insert) {
        if (currDepth < aaIndices.size()) {
            STORM_LOG_ASSERT(!currReplacement.isVariable(), "Did not expect a variable replacement at this depth.");
            auto currIndexExpr = arrayExprEliminator.eliminate(aaIndices[currDepth]);
            if (currIndexExpr.containsVariables()) {
                for (uint64_t index = 0; index < currReplacement.size(); ++index) {
                    auto indexExpr = currIndexExpr.getManager().integer(index);
                    auto childCondition = (currIndexExpr == indexExpr);
                    if (currCondition.isInitialized()) {
                        childCondition = currCondition && childCondition;
                    }
                    // if the current index expression is a single variable, we can substitute it by the concrete value in the rhs
                    if (indexExpr.isVariable()) {
                        auto sub = std::map<storm::expressions::Variable, storm::expressions::Expression>(
                            {{indexExpr.getBaseExpression().asVariableExpression().getVariable(), indexExpr}});
                        auto childRhs = currRhs.substitute(sub);
                        insertArrayAssignmentReplacements(aaIndices, currDepth + 1, currReplacement.at(index), childRhs, childCondition, insert);
                    } else {
                        insertArrayAssignmentReplacements(aaIndices, currDepth + 1, currReplacement.at(index), currRhs, childCondition, insert);
                    }
                }
            } else {
                insertArrayAssignmentReplacements(aaIndices, currDepth + 1, currReplacement.at(currIndexExpr.evaluateAsInt()), currRhs, currCondition, insert);
            }
        } else if (currReplacement.isVariable()) {
            auto eliminationRes = arrayExprEliminator.eliminateChkSizes(currRhs);
            if (eliminationRes.isArrayOutOfBounds()) {
                // We have an out-of-bounds situation. This is expected if we have an incomplete array access
                // For example, when we set a:=b where b is smaller than a
                STORM_LOG_THROW(currDepth > aaIndices.size(), storm::exceptions::InvalidOperationException,
                                "Detected out of bounds array access in assignment to " << currReplacement.getVariable().getName() << ".");
                insert(currReplacement.getVariable(), {}, currCondition);
            } else {
                insert(currReplacement.getVariable(), eliminationRes.expr(), currCondition);
            }
        } else {
            for (uint64_t index = 0; index < currReplacement.size(); ++index) {
                auto childRhs = std::make_shared<storm::expressions::ArrayAccessExpression>(currRhs.getManager(), currRhs.getType().getElementType(),
                                                                                            currRhs.getBaseExpressionPointer(),
                                                                                            currRhs.getManager().integer(index).getBaseExpressionPointer())
                                    ->toExpression();
                insertArrayAssignmentReplacements(aaIndices, currDepth + 1, currReplacement.at(index), childRhs, currCondition, insert);
            }
        }
    }

    /*!
     * Handles assignments to a single array variable at a given assignment level.
     * @param arrayAssignments must consider the same array variable
     * @param level the joint level of the assignments
     * @param replacements the replacements for this variable
     * @param newAssignments container in which the newly created assignments are inserted.
     */
    void handleArrayAssignments(std::vector<Assignment const*> const& arrayAssignments, int64_t level,
                                typename ArrayEliminatorData::Replacement const& replacements, std::vector<Assignment>& newAssignments) {
        // We keep array access assignments if we are requested to do so, if all array accesses are full, and if at least one index is not constant
        if (keepNonTrivialArrayAccess &&
            std::all_of(arrayAssignments.begin(), arrayAssignments.end(), [](Assignment const* a) { return a->getLValue().isFullArrayAccess(); }) &&
            std::any_of(arrayAssignments.begin(), arrayAssignments.end(), [](Assignment const* a) { return a->getLValue().arrayIndexContainsVariable(); })) {
            for (auto aa : arrayAssignments) {
                // Eliminate array expressions in array access indices and the assigned expression
                auto const& lValue = aa->getLValue();
                STORM_LOG_ASSERT(lValue.isFullArrayAccess(), "unexpected type of lValue");
                std::vector<storm::expressions::Expression> newAaIndices;
                newAaIndices.reserve(lValue.getArrayIndexVector().size());
                for (auto const& i : lValue.getArrayIndexVector()) {
                    newAaIndices.push_back(arrayExprEliminator.eliminate(i));
                }
                newAssignments.emplace_back(storm::jani::LValue(lValue.getVariable(), newAaIndices), arrayExprEliminator.eliminate(aa->getAssignedExpression()),
                                            level);
            }
        } else {
            convertToNonArrayAssignments(arrayAssignments, level, replacements, newAssignments);
        }
    }

    /*!
     * Converts the given array variable or array access assignments to non-array assignments.
     * @param arrayAssignments must consider the same array variable
     * @param level the joint level of the assignments
     * @param replacements the replacements for this variable
     * @param newAssignments container in which the newly created assignments are inserted.
     */
    void convertToNonArrayAssignments(std::vector<Assignment const*> const& arrayAssignments, int64_t level,
                                      typename ArrayEliminatorData::Replacement const& replacements, std::vector<Assignment>& newAssignments) {
        std::map<storm::expressions::Variable, Assignment> collectedAssignments;
        auto insert = [&](storm::jani::Variable const& var, storm::expressions::Expression const& newRhs, storm::expressions::Expression const& condition) {
            STORM_LOG_ASSERT(!condition.isInitialized() || !storm::jani::containsArrayExpression(condition),
                             "condition " << condition << " still contains array expressions.");
            auto rhs = newRhs.isInitialized() ? newRhs : getOutOfBoundsValue(var);
            auto findIt = collectedAssignments.find(var.getExpressionVariable());
            if (findIt == collectedAssignments.end()) {
                if (condition.isInitialized()) {
                    rhs = storm::expressions::ite(condition, rhs, var.getExpressionVariable().getExpression());
                }
                collectedAssignments.emplace(var.getExpressionVariable(), Assignment(LValue(var), rhs, level));
            } else {
                auto otherRhs = findIt->second.getAssignedExpression();
                // Deal with cases like 'a[0]:=x; a:=b' or 'a:=b; a[0]:=x'
                STORM_LOG_THROW(condition.isInitialized(), storm::exceptions::InvalidOperationException,
                                "Found conflicting array assignments to " << var.getName() << ".");
                STORM_LOG_THROW(otherRhs.getBaseExpression().isIfThenElseExpression(), storm::exceptions::InvalidOperationException,
                                "Found conflicting array assignments to " << var.getName() << ".");
                rhs = storm::expressions::ite(condition, rhs, otherRhs);
                findIt->second.setAssignedExpression(rhs);
            }
        };

        for (auto aa : arrayAssignments) {
            auto const& lValue = aa->getLValue();
            if (lValue.isArrayAccess()) {
                insertArrayAssignmentReplacements(lValue.getArrayIndexVector(), 0, replacements, aa->getAssignedExpression(), storm::expressions::Expression(),
                                                  insert);
            } else {
                insertArrayAssignmentReplacements({}, 0, replacements, aa->getAssignedExpression(), storm::expressions::Expression(), insert);
            }
        }

        for (auto& newAs : collectedAssignments) {
            newAssignments.push_back(std::move(newAs.second));
        }
    }

    // Returns some dedicated value used in out-of-bound situations.
    storm::expressions::Expression getOutOfBoundsValue(Variable const& var) const {
        auto const& expressionManager = var.getExpressionVariable().getManager();
        return getOutOfBoundsValue(var.getType(), expressionManager);
    }

    storm::expressions::Expression getOutOfBoundsValue(JaniType const& type, storm::expressions::ExpressionManager const& expressionManager) const {
        if (type.isBasicType()) {
            switch (type.asBasicType().get()) {
                case BasicType::Type::Bool:
                    return expressionManager.boolean(false);
                case BasicType::Type::Int:
                    return expressionManager.integer(0);
                case BasicType::Type::Real:
                    return expressionManager.rational(0.0);
            }
        } else if (type.isBoundedType()) {
            auto const& bndType = type.asBoundedType();
            if (bndType.hasLowerBound()) {
                return bndType.getLowerBound();
            } else if (bndType.hasUpperBound()) {
                return bndType.getUpperBound();
            } else {
                switch (bndType.getBaseType()) {
                    case BoundedType::BaseType::Int:
                        return expressionManager.integer(0);
                    case BoundedType::BaseType::Real:
                        return expressionManager.rational(0.0);
                }
            }
        }
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled variable type.");
        return storm::expressions::Expression();
    }

    ArrayEliminatorData const& elimData;
    ArrayExpressionEliminationVisitor arrayExprEliminator;
    bool const keepNonTrivialArrayAccess;
};
}  // namespace detail

ArrayEliminatorData::Replacement::Replacement(storm::jani::Variable const& variable) : data(&variable) {
    // Intentionally left empty
}

ArrayEliminatorData::Replacement::Replacement(std::vector<Replacement>&& replacements) : data(std::move(replacements)) {
    // Intentionally left empty
}

bool ArrayEliminatorData::Replacement::isVariable() const {
    return data.which() == 0;
}

storm::jani::Variable const& ArrayEliminatorData::Replacement::getVariable() const {
    STORM_LOG_ASSERT(isVariable(), "unexpected replacement type");
    return *boost::get<storm::jani::Variable const*>(data);
}

std::vector<typename ArrayEliminatorData::Replacement> const& ArrayEliminatorData::Replacement::getReplacements() const {
    STORM_LOG_ASSERT(!isVariable(), "unexpected replacement type");
    return boost::get<std::vector<Replacement>>(data);
}

typename ArrayEliminatorData::Replacement const& ArrayEliminatorData::Replacement::at(std::size_t const& index) const {
    STORM_LOG_ASSERT(!isVariable(), "unexpected replacement type");
    return getReplacements().at(index);
}

typename ArrayEliminatorData::Replacement& ArrayEliminatorData::Replacement::at(std::size_t const& index) {
    STORM_LOG_ASSERT(!isVariable(), "unexpected replacement type");
    return boost::get<std::vector<Replacement>>(data).at(index);
}

typename ArrayEliminatorData::Replacement const& ArrayEliminatorData::Replacement::at(std::vector<std::size_t> const& indices) const {
    auto const* cur = this;
    for (auto const& i : indices) {
        cur = &cur->at(i);
    }
    return *cur;
}

typename ArrayEliminatorData::Replacement& ArrayEliminatorData::Replacement::at(std::vector<std::size_t> const& indices) {
    auto* cur = this;
    for (auto const& i : indices) {
        cur = &cur->at(i);
    }
    return *cur;
}

std::size_t ArrayEliminatorData::Replacement::size() const {
    return getReplacements().size();
}

void ArrayEliminatorData::Replacement::grow(std::size_t const& minimumSize) {
    if (getReplacements().size() < minimumSize) {
        boost::get<std::vector<Replacement>>(data).resize(minimumSize);
    }
}

storm::expressions::Expression ArrayEliminatorData::transformExpression(storm::expressions::Expression const& arrayExpression) const {
    detail::ArrayExpressionEliminationVisitor eliminator(*this);
    return eliminator.eliminate(arrayExpression);
}

void ArrayEliminatorData::transformProperty(storm::jani::Property& property) const {
    property = property.substitute([this](storm::expressions::Expression const& exp) { return transformExpression(exp); });
}

ArrayEliminatorData ArrayEliminator::eliminate(Model& model, bool keepNonTrivialArrayAccess) {
    // Only perform actions if there actually are arrays.
    if (model.getModelFeatures().hasArrays()) {
        auto elimData = detail::ArrayEliminatorDataCollector(model).get();
        detail::ArrayVariableReplacer(elimData, keepNonTrivialArrayAccess).replace(model);
        if (!keepNonTrivialArrayAccess) {
            model.getModelFeatures().remove(ModelFeature::Arrays);
        }
        model.finalize();
        STORM_LOG_ASSERT(keepNonTrivialArrayAccess || !containsArrayExpression(model), "the model still contains array expressions.");
        return elimData;
    }
    STORM_LOG_ASSERT(!containsArrayExpression(model), "the model contains array expressions although the array feature is not enabled.");
    return {};
}
}  // namespace jani
}  // namespace storm
