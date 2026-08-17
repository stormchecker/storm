#include "storm-dft/modelchecker/SmtConstraint.h"

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm::dft {
namespace modelchecker {

std::string IsMaximum::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(and ";
    // assert it is largereq than all values.
    for (auto const &ovi : varIndices) {
        sstr << "(>= " << varNames.at(varIndex) << " " << varNames.at(ovi) << ") ";
    }
    // assert it is one of the values.
    sstr << "(or ";
    for (auto const &ovi : varIndices) {
        sstr << "(= " << varNames.at(varIndex) << " " << varNames.at(ovi) << ") ";
    }
    sstr << ")";  // end of the or
    sstr << ")";  // end outer and.
    return sstr.str();
}

storm::expressions::Expression IsMaximum::toExpression(std::vector<std::string> const &varNames,
                                                       std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    std::vector<storm::expressions::Expression> outerAnd;
    std::vector<storm::expressions::Expression> innerOr;
    for (auto const &ovi : varIndices) {
        outerAnd.push_back((manager->getVariableExpression(varNames.at(varIndex)) >= manager->getVariableExpression(varNames.at(ovi))));
        innerOr.push_back((manager->getVariableExpression(varNames.at(varIndex)) == manager->getVariableExpression(varNames.at(ovi))));
    }
    outerAnd.push_back(disjunction(innerOr));
    return conjunction(outerAnd);
}

std::string IsMinimum::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(and ";
    // assert it is smallereq than all values.
    for (auto const &ovi : varIndices) {
        sstr << "(<= " << varNames.at(varIndex) << " " << varNames.at(ovi) << ") ";
    }
    // assert it is one of the values.
    sstr << "(or ";
    for (auto const &ovi : varIndices) {
        sstr << "(= " << varNames.at(varIndex) << " " << varNames.at(ovi) << ") ";
    }
    sstr << ")";  // end of the or
    sstr << ")";  // end outer and.
    return sstr.str();
}

storm::expressions::Expression IsMinimum::toExpression(std::vector<std::string> const &varNames,
                                                       std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    std::vector<storm::expressions::Expression> outerAnd;
    std::vector<storm::expressions::Expression> innerOr;
    for (auto const &ovi : varIndices) {
        outerAnd.push_back((manager->getVariableExpression(varNames.at(varIndex)) <= manager->getVariableExpression(varNames.at(ovi))));
        innerOr.push_back((manager->getVariableExpression(varNames.at(varIndex)) == manager->getVariableExpression(varNames.at(ovi))));
    }
    outerAnd.push_back(disjunction(innerOr));
    return conjunction(outerAnd);
}

std::string BetweenValues::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(and ";
    sstr << "(>= " << varNames.at(varIndex) << " " << lowerBound << ")";
    sstr << "(<= " << varNames.at(varIndex) << " " << upperBound << ")";
    sstr << ")";
    return sstr.str();
}

storm::expressions::Expression BetweenValues::toExpression(std::vector<std::string> const &varNames,
                                                           std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return (manager->getVariableExpression(varNames.at(varIndex)) >= lowerBound) && (manager->getVariableExpression(varNames.at(varIndex)) <= upperBound);
}

std::string And::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    if (constraints.empty()) {
        sstr << "true";
    } else {
        sstr << "(and";
        for (auto const &c : constraints) {
            sstr << " " << c->toSmtlib2(varNames);
        }
        sstr << ")";
    }
    return sstr.str();
}

storm::expressions::Expression And::toExpression(std::vector<std::string> const &varNames,
                                                 std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    if (constraints.empty()) {
        return manager->boolean(true);
    } else {
        std::vector<storm::expressions::Expression> conjuncts;
        for (auto const &c : constraints) {
            conjuncts.push_back(c->toExpression(varNames, manager));
        }
        return conjunction(conjuncts);
    }
}

std::string Or::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    if (constraints.empty()) {
        sstr << "false";
    } else {
        sstr << "(or";
        for (auto const &c : constraints) {
            sstr << " " << c->toSmtlib2(varNames);
        }
        sstr << ")";
    }
    return sstr.str();
}

storm::expressions::Expression Or::toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    if (constraints.empty()) {
        return manager->boolean(false);
    } else {
        std::vector<storm::expressions::Expression> disjuncts;
        for (auto const &c : constraints) {
            disjuncts.push_back(c->toExpression(varNames, manager));
        }
        return disjunction(disjuncts);
    }
}

std::string Implies::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(=> " << lhs->toSmtlib2(varNames) << " " << rhs->toSmtlib2(varNames) << ")";
    return sstr.str();
}

storm::expressions::Expression Implies::toExpression(std::vector<std::string> const &varNames,
                                                     std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return implies(lhs->toExpression(varNames, manager), rhs->toExpression(varNames, manager));
}

std::string Iff::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(= " << lhs->toSmtlib2(varNames) << " " << rhs->toSmtlib2(varNames) << ")";
    return sstr.str();
}

storm::expressions::Expression Iff::toExpression(std::vector<std::string> const &varNames,
                                                 std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return iff(lhs->toExpression(varNames, manager), rhs->toExpression(varNames, manager));
}

std::string IsTrue::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << (value ? "true" : "false");
    return sstr.str();
}

storm::expressions::Expression IsTrue::toExpression(std::vector<std::string> const &varNames,
                                                    std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->boolean(value);
}

std::string IsBoolValue::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    STORM_LOG_ASSERT(varIndex < varNames.size(), "Variable index out of range.");
    if (value) {
        sstr << varNames.at(varIndex);
    } else {
        sstr << "(not " << varNames.at(varIndex) << ")";
    }
    return sstr.str();
}

storm::expressions::Expression IsBoolValue::toExpression(std::vector<std::string> const &varNames,
                                                         std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    if (value) {
        return manager->getVariableExpression(varNames.at(varIndex));
    } else {
        return !(manager->getVariableExpression(varNames.at(varIndex)));
    }
}

std::string IsConstantValue::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    STORM_LOG_ASSERT(varIndex < varNames.size(), "Variable index out of range.");
    sstr << "(= " << varNames.at(varIndex) << " " << value << ")";
    return sstr.str();
}

storm::expressions::Expression IsConstantValue::toExpression(std::vector<std::string> const &varNames,
                                                             std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(varIndex)) == manager->integer(value);
}

std::string IsNotConstantValue::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    STORM_LOG_ASSERT(varIndex < varNames.size(), "Variable index out of range.");
    sstr << "(distinct " << varNames.at(varIndex) << " " << value << ")";
    return sstr.str();
}

storm::expressions::Expression IsNotConstantValue::toExpression(std::vector<std::string> const &varNames,
                                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(varIndex)) != manager->integer(value);
}

std::string IsLessConstant::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    STORM_LOG_ASSERT(varIndex < varNames.size(), "Variable index out of range.");
    sstr << "(< " << varNames.at(varIndex) << " " << value << ")";
    return sstr.str();
}

storm::expressions::Expression IsLessConstant::toExpression(std::vector<std::string> const &varNames,
                                                            std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(varIndex)) < value;
}

std::string IsLessEqualConstant::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    STORM_LOG_ASSERT(varIndex < varNames.size(), "Variable index out of range.");
    sstr << "(<= " << varNames.at(varIndex) << " " << value << ")";
    return sstr.str();
}

storm::expressions::Expression IsLessEqualConstant::toExpression(std::vector<std::string> const &varNames,
                                                                 std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(varIndex)) <= value;
}

std::string IsEqual::toSmtlib2(std::vector<std::string> const &varNames) const {
    return "(= " + varNames.at(var1Index) + " " + varNames.at(var2Index) + ")";
}

storm::expressions::Expression IsEqual::toExpression(std::vector<std::string> const &varNames,
                                                     std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(var1Index)) == manager->getVariableExpression(varNames.at(var2Index));
}

std::string IsUnequal::toSmtlib2(std::vector<std::string> const &varNames) const {
    return "(distinct " + varNames.at(var1Index) + " " + varNames.at(var2Index) + ")";
}

storm::expressions::Expression IsUnequal::toExpression(std::vector<std::string> const &varNames,
                                                       std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(var1Index)) != manager->getVariableExpression(varNames.at(var2Index));
}

std::string IsLess::toSmtlib2(std::vector<std::string> const &varNames) const {
    return "(< " + varNames.at(var1Index) + " " + varNames.at(var2Index) + ")";
}

storm::expressions::Expression IsLess::toExpression(std::vector<std::string> const &varNames,
                                                    std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(var1Index)) < manager->getVariableExpression(varNames.at(var2Index));
}

std::string IsLessEqual::toSmtlib2(std::vector<std::string> const &varNames) const {
    return "(<= " + varNames.at(var1Index) + " " + varNames.at(var2Index) + ")";
}

storm::expressions::Expression IsLessEqual::toExpression(std::vector<std::string> const &varNames,
                                                         std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return manager->getVariableExpression(varNames.at(var1Index)) <= manager->getVariableExpression(varNames.at(var2Index));
}

std::string PairwiseDifferent::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(distinct";
    for (auto const &varIndex : varIndices) {
        sstr << " " << varNames.at(varIndex);
    }
    sstr << ")";
    return sstr.str();
}

storm::expressions::Expression PairwiseDifferent::toExpression(std::vector<std::string> const &varNames,
                                                               std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    std::vector<storm::expressions::Expression> conjuncts;
    for (uint64_t i = 0; i < varIndices.size(); ++i) {
        for (uint64_t j = i + 1; j < varIndices.size(); ++j) {
            // check all elements pairwise for inequality
            conjuncts.push_back(manager->getVariableExpression(varNames.at(varIndices.at(i))) != manager->getVariableExpression(varNames.at(varIndices.at(j))));
        }
    }
    // take the conjunction of all pairwise inequalities
    return conjunction(conjuncts);
}

std::string Sorted::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(and ";
    for (uint64_t i = 1; i < varIndices.size(); ++i) {
        sstr << "(<= " << varNames.at(varIndices.at(i - 1)) << " " << varNames.at(varIndices.at(i)) << ")";
    }
    sstr << ") ";
    return sstr.str();
}

storm::expressions::Expression Sorted::toExpression(std::vector<std::string> const &varNames,
                                                    std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    std::vector<storm::expressions::Expression> conjuncts;
    for (uint64_t i = 1; i < varIndices.size(); ++i) {
        conjuncts.push_back(manager->getVariableExpression(varNames.at(varIndices.at(i - 1))) <= manager->getVariableExpression(varNames.at(varIndices.at(i))));
    }
    // take the conjunction of all pairwise inequalities
    return conjunction(conjuncts);
}

std::string IfThenElse::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(ite " << ifConstraint->toSmtlib2(varNames) << " " << thenConstraint->toSmtlib2(varNames) << " " << elseConstraint->toSmtlib2(varNames) << ")";
    return sstr.str();
}

storm::expressions::Expression IfThenElse::toExpression(std::vector<std::string> const &varNames,
                                                        std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    return ite(ifConstraint->toExpression(varNames, manager), thenConstraint->toExpression(varNames, manager), elseConstraint->toExpression(varNames, manager));
}

std::string FalseCountIsEqualConstant::toSmtlib2(std::vector<std::string> const &varNames) const {
    std::stringstream sstr;
    sstr << "(= (+ ";
    for (uint64_t i = 0; i < varIndices.size(); ++i) {
        sstr << "(ite " << varNames.at(varIndices.at(i)) << " 0 1 )";
    }
    sstr << ") " << value << " )";
    return sstr.str();
}

storm::expressions::Expression FalseCountIsEqualConstant::toExpression(std::vector<std::string> const &varNames,
                                                                       std::shared_ptr<storm::expressions::ExpressionManager> manager) const {
    std::vector<storm::expressions::Expression> boolToInt;
    for (uint64_t i = 0; i < varIndices.size(); ++i) {
        boolToInt.push_back(ite(manager->getVariableExpression(varNames.at(varIndices.at(i))),  // If variable is true
                                manager->integer(0),                                            // set 0
                                manager->integer(1)));                                          // else 1
    }
    return sum(boolToInt) == manager->integer(value);
}

}  // namespace modelchecker
}  // namespace storm::dft
