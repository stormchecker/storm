#pragma once

#include <string>

#include "storm/storage/expressions/Expression.h"

namespace storm::dft {
namespace modelchecker {

class SmtConstraint {
   public:
    virtual ~SmtConstraint() {}

    /** Generate a string describing the constraint in Smtlib2 format
     *
     * @param varNames vector of variable names
     * @return Smtlib2 format string
     */
    virtual std::string toSmtlib2(std::vector<std::string> const &varNames) const = 0;

    /** Generate an expression describing the constraint in Storm format
     *
     * @param varNames vector of variable names
     * @param manager the expression manager used to handle the expressions
     * @return the expression
     */
    virtual storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                        std::shared_ptr<storm::expressions::ExpressionManager> manager) const = 0;

    virtual std::string description() const {
        return descript;
    }

    void setDescription(std::string const &descr) {
        descript = descr;
    }

   private:
    std::string descript;
};

/*
 * Variable[VarIndex] is the maximum of the others
 */
class IsMaximum : public SmtConstraint {
   public:
    IsMaximum(uint64_t varIndex, std::vector<uint64_t> const &varIndices) : varIndex(varIndex), varIndices(varIndices) {}

    virtual ~IsMaximum() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    std::vector<uint64_t> varIndices;
};

/*
 * First is the minimum of the others
 */
class IsMinimum : public SmtConstraint {
   public:
    IsMinimum(uint64_t varIndex, std::vector<uint64_t> const &varIndices) : varIndex(varIndex), varIndices(varIndices) {}

    virtual ~IsMinimum() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    std::vector<uint64_t> varIndices;
};

class BetweenValues : public SmtConstraint {
   public:
    BetweenValues(uint64_t varIndex, uint64_t lower, uint64_t upper) : varIndex(varIndex), upperBound(upper), lowerBound(lower) {}

    virtual ~BetweenValues() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    uint64_t upperBound;
    uint64_t lowerBound;
};

class And : public SmtConstraint {
   public:
    And(std::vector<std::shared_ptr<SmtConstraint>> const &constraints) : constraints(constraints) {}

    virtual ~And() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::vector<std::shared_ptr<SmtConstraint>> constraints;
};

class Or : public SmtConstraint {
   public:
    Or(std::vector<std::shared_ptr<SmtConstraint>> const &constraints) : constraints(constraints) {}

    virtual ~Or() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::vector<std::shared_ptr<SmtConstraint>> constraints;
};

class Implies : public SmtConstraint {
   public:
    Implies(std::shared_ptr<SmtConstraint> l, std::shared_ptr<SmtConstraint> r) : lhs(l), rhs(r) {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::shared_ptr<SmtConstraint> lhs;
    std::shared_ptr<SmtConstraint> rhs;
};

class Iff : public SmtConstraint {
   public:
    Iff(std::shared_ptr<SmtConstraint> l, std::shared_ptr<SmtConstraint> r) : lhs(l), rhs(r) {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::shared_ptr<SmtConstraint> lhs;
    std::shared_ptr<SmtConstraint> rhs;
};

class IsTrue : public SmtConstraint {
   public:
    IsTrue(bool val) : value(val) {}

    virtual ~IsTrue() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    bool value;
};

class IsBoolValue : public SmtConstraint {
   public:
    IsBoolValue(uint64_t varIndex, bool val) : varIndex(varIndex), value(val) {}

    virtual ~IsBoolValue() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    bool value;
};

class IsConstantValue : public SmtConstraint {
   public:
    IsConstantValue(uint64_t varIndex, uint64_t val) : varIndex(varIndex), value(val) {}

    virtual ~IsConstantValue() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    uint64_t value;
};

class IsNotConstantValue : public SmtConstraint {
   public:
    IsNotConstantValue(uint64_t varIndex, uint64_t val) : varIndex(varIndex), value(val) {}

    virtual ~IsNotConstantValue() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    uint64_t value;
};

class IsLessConstant : public SmtConstraint {
   public:
    IsLessConstant(uint64_t varIndex, uint64_t val) : varIndex(varIndex), value(val) {}

    virtual ~IsLessConstant() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    uint64_t value;
};

class IsLessEqualConstant : public SmtConstraint {
   public:
    IsLessEqualConstant(uint64_t varIndex, uint64_t val) : varIndex(varIndex), value(val) {}

    virtual ~IsLessEqualConstant() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t varIndex;
    uint64_t value;
};

class IsEqual : public SmtConstraint {
   public:
    IsEqual(uint64_t varIndex1, uint64_t varIndex2) : var1Index(varIndex1), var2Index(varIndex2) {}

    virtual ~IsEqual() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t var1Index;
    uint64_t var2Index;
};

class IsUnequal : public SmtConstraint {
   public:
    IsUnequal(uint64_t varIndex1, uint64_t varIndex2) : var1Index(varIndex1), var2Index(varIndex2) {}

    virtual ~IsUnequal() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t var1Index;
    uint64_t var2Index;
};

class IsLess : public SmtConstraint {
   public:
    IsLess(uint64_t varIndex1, uint64_t varIndex2) : var1Index(varIndex1), var2Index(varIndex2) {}

    virtual ~IsLess() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t var1Index;
    uint64_t var2Index;
};

class IsLessEqual : public SmtConstraint {
   public:
    IsLessEqual(uint64_t varIndex1, uint64_t varIndex2) : var1Index(varIndex1), var2Index(varIndex2) {}

    virtual ~IsLessEqual() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    uint64_t var1Index;
    uint64_t var2Index;
};

class PairwiseDifferent : public SmtConstraint {
   public:
    PairwiseDifferent(std::vector<uint64_t> const &indices) : varIndices(indices) {}

    virtual ~PairwiseDifferent() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::vector<uint64_t> varIndices;
};

class Sorted : public SmtConstraint {
   public:
    Sorted(std::vector<uint64_t> varIndices) : varIndices(varIndices) {}

    virtual ~Sorted() {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::vector<uint64_t> varIndices;
};

class IfThenElse : public SmtConstraint {
   public:
    IfThenElse(std::shared_ptr<SmtConstraint> ifC, std::shared_ptr<SmtConstraint> thenC, std::shared_ptr<SmtConstraint> elseC)
        : ifConstraint(ifC), thenConstraint(thenC), elseConstraint(elseC) {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::shared_ptr<SmtConstraint> ifConstraint;
    std::shared_ptr<SmtConstraint> thenConstraint;
    std::shared_ptr<SmtConstraint> elseConstraint;
};

class FalseCountIsEqualConstant : public SmtConstraint {
   public:
    FalseCountIsEqualConstant(std::vector<uint64_t> varIndices, uint64_t val) : varIndices(varIndices), value(val) {}

    std::string toSmtlib2(std::vector<std::string> const &varNames) const override;

    storm::expressions::Expression toExpression(std::vector<std::string> const &varNames,
                                                std::shared_ptr<storm::expressions::ExpressionManager> manager) const override;

   private:
    std::vector<uint64_t> varIndices;
    uint64_t value;
};

}  // namespace modelchecker
}  // namespace storm::dft
