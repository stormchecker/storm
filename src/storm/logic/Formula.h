#ifndef STORM_LOGIC_FORMULA_H_
#define STORM_LOGIC_FORMULA_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <boost/any.hpp>
#include "storm/logic/FormulasForwardDeclarations.h"

namespace storm {
namespace expressions {
class Variable;
class Expression;
class ExpressionManager;
}  // namespace expressions

namespace logic {

// Forward-declare visitor for accept() method.
class FormulaVisitor;

// Forward-declare fragment specification for isInFragment() method.
class FragmentSpecification;

// Forward-declare formula information class for info() method.
class FormulaInformation;

class Formula : public std::enable_shared_from_this<Formula> {
   public:
    // Make the destructor virtual to allow deletion of objects of subclasses via a pointer to this class.
    virtual ~Formula() {
        // Intentionally left empty.
    };

    friend std::ostream& operator<<(std::ostream& out, Formula const& formula);

    // Basic formula types.
    virtual bool isPathFormula() const;
    virtual bool isStateFormula() const;
    virtual bool isConditionalProbabilityFormula() const;
    virtual bool isConditionalRewardFormula() const;

    virtual bool isProbabilityPathFormula() const;
    virtual bool isRewardPathFormula() const;
    virtual bool isTimePathFormula() const;

    virtual bool isBinaryBooleanStateFormula() const;
    virtual bool isUnaryBooleanStateFormula() const;

    virtual bool isBinaryBooleanPathFormula() const;
    virtual bool isUnaryBooleanPathFormula() const;

    virtual bool isMultiObjectiveFormula() const;
    virtual bool isQuantileFormula() const;

    // Operator formulas.
    virtual bool isOperatorFormula() const;
    virtual bool isLongRunAverageOperatorFormula() const;
    virtual bool isTimeOperatorFormula() const;
    virtual bool isProbabilityOperatorFormula() const;
    virtual bool isRewardOperatorFormula() const;

    // Atomic state formulas.
    virtual bool isBooleanLiteralFormula() const;
    virtual bool isTrueFormula() const;
    virtual bool isFalseFormula() const;
    virtual bool isAtomicExpressionFormula() const;
    virtual bool isAtomicLabelFormula() const;

    // Probability path formulas.
    virtual bool isNextFormula() const;
    virtual bool isUntilFormula() const;
    virtual bool isBoundedUntilFormula() const;
    virtual bool isGloballyFormula() const;
    virtual bool isEventuallyFormula() const;
    virtual bool isReachabilityProbabilityFormula() const;
    virtual bool isHOAPathFormula() const;

    // Reward formulas.
    virtual bool isCumulativeRewardFormula() const;
    virtual bool isDiscountedCumulativeRewardFormula() const;
    virtual bool isInstantaneousRewardFormula() const;
    virtual bool isReachabilityRewardFormula() const;
    virtual bool isLongRunAverageRewardFormula() const;
    virtual bool isTotalRewardFormula() const;
    virtual bool isDiscountedTotalRewardFormula() const;

    // Expected time formulas.
    virtual bool isReachabilityTimeFormula() const;

    // Game formulas.
    virtual bool isGameFormula() const;

    // Type checks for abstract intermediate classes.
    virtual bool isBinaryPathFormula() const;
    virtual bool isBinaryStateFormula() const;
    virtual bool isUnaryPathFormula() const;
    virtual bool isUnaryStateFormula() const;
    bool isUnaryFormula() const;

    // Accessors for the return type of a formula.
    virtual bool hasQualitativeResult() const;
    virtual bool hasQuantitativeResult() const;

    bool isInFragment(FragmentSpecification const& fragment) const;
    FormulaInformation info(bool recurseIntoOperators = true) const;

    boost::any accept(FormulaVisitor const& visitor) const;
    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const = 0;

    static std::shared_ptr<Formula const> getTrueFormula();

    bool isInitialFormula() const;

    PathFormula& asPathFormula();
    PathFormula const& asPathFormula() const;

    StateFormula& asStateFormula();
    StateFormula const& asStateFormula() const;

    MultiObjectiveFormula& asMultiObjectiveFormula();
    MultiObjectiveFormula const& asMultiObjectiveFormula() const;

    QuantileFormula& asQuantileFormula();
    QuantileFormula const& asQuantileFormula() const;

    BinaryStateFormula& asBinaryStateFormula();
    BinaryStateFormula const& asBinaryStateFormula() const;

    UnaryStateFormula& asUnaryStateFormula();
    UnaryStateFormula const& asUnaryStateFormula() const;

    BinaryBooleanStateFormula& asBinaryBooleanStateFormula();
    BinaryBooleanStateFormula const& asBinaryBooleanStateFormula() const;

    UnaryBooleanStateFormula& asUnaryBooleanStateFormula();
    UnaryBooleanStateFormula const& asUnaryBooleanStateFormula() const;

    BooleanLiteralFormula& asBooleanLiteralFormula();
    BooleanLiteralFormula const& asBooleanLiteralFormula() const;

    AtomicExpressionFormula& asAtomicExpressionFormula();
    AtomicExpressionFormula const& asAtomicExpressionFormula() const;

    AtomicLabelFormula& asAtomicLabelFormula();
    AtomicLabelFormula const& asAtomicLabelFormula() const;

    UntilFormula& asUntilFormula();
    UntilFormula const& asUntilFormula() const;

    HOAPathFormula& asHOAPathFormula();
    HOAPathFormula const& asHOAPathFormula() const;

    BoundedUntilFormula& asBoundedUntilFormula();
    BoundedUntilFormula const& asBoundedUntilFormula() const;

    EventuallyFormula& asEventuallyFormula();
    EventuallyFormula const& asEventuallyFormula() const;

    EventuallyFormula& asReachabilityProbabilityFormula();
    EventuallyFormula const& asReachabilityProbabilityFormula() const;

    EventuallyFormula& asReachabilityRewardFormula();
    EventuallyFormula const& asReachabilityRewardFormula() const;

    EventuallyFormula& asReachabilityTimeFormula();
    EventuallyFormula const& asReachabilityTimeFormula() const;

    GameFormula& asGameFormula();
    GameFormula const& asGameFormula() const;

    GloballyFormula& asGloballyFormula();
    GloballyFormula const& asGloballyFormula() const;

    BinaryPathFormula& asBinaryPathFormula();
    BinaryPathFormula const& asBinaryPathFormula() const;

    UnaryPathFormula& asUnaryPathFormula();
    UnaryPathFormula const& asUnaryPathFormula() const;

    ConditionalFormula& asConditionalFormula();
    ConditionalFormula const& asConditionalFormula() const;

    NextFormula& asNextFormula();
    NextFormula const& asNextFormula() const;

    LongRunAverageOperatorFormula& asLongRunAverageOperatorFormula();
    LongRunAverageOperatorFormula const& asLongRunAverageOperatorFormula() const;

    TimeOperatorFormula& asTimeOperatorFormula();
    TimeOperatorFormula const& asTimeOperatorFormula() const;

    CumulativeRewardFormula& asCumulativeRewardFormula();
    CumulativeRewardFormula const& asCumulativeRewardFormula() const;

    TotalRewardFormula& asTotalRewardFormula();
    TotalRewardFormula const& asTotalRewardFormula() const;

    DiscountedCumulativeRewardFormula& asDiscountedCumulativeRewardFormula();
    DiscountedCumulativeRewardFormula const& asDiscountedCumulativeRewardFormula() const;

    DiscountedTotalRewardFormula& asDiscountedTotalRewardFormula();
    DiscountedTotalRewardFormula const& asDiscountedTotalRewardFormula() const;

    InstantaneousRewardFormula& asInstantaneousRewardFormula();
    InstantaneousRewardFormula const& asInstantaneousRewardFormula() const;

    LongRunAverageRewardFormula& asLongRunAverageRewardFormula();
    LongRunAverageRewardFormula const& asLongRunAverageRewardFormula() const;

    ProbabilityOperatorFormula& asProbabilityOperatorFormula();
    ProbabilityOperatorFormula const& asProbabilityOperatorFormula() const;

    RewardOperatorFormula& asRewardOperatorFormula();
    RewardOperatorFormula const& asRewardOperatorFormula() const;

    OperatorFormula& asOperatorFormula();
    OperatorFormula const& asOperatorFormula() const;

    std::vector<std::shared_ptr<AtomicExpressionFormula const>> getAtomicExpressionFormulas() const;
    std::vector<std::shared_ptr<AtomicLabelFormula const>> getAtomicLabelFormulas() const;
    std::set<storm::expressions::Variable> getUsedVariables() const;
    std::set<std::string> getReferencedRewardModels() const;

    std::shared_ptr<Formula const> asSharedPointer();
    std::shared_ptr<Formula const> asSharedPointer() const;

    std::shared_ptr<Formula> clone() const;

    std::shared_ptr<Formula> substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    std::shared_ptr<Formula> substitute(
        std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& expressionSubstitution) const;
    std::shared_ptr<Formula> substitute(std::map<std::string, storm::expressions::Expression> const& labelSubstitution) const;
    std::shared_ptr<Formula> substitute(std::map<std::string, std::string> const& labelSubstitution) const;
    std::shared_ptr<Formula> substituteRewardModelNames(std::map<std::string, std::string> const& rewardModelNameSubstitution) const;
    std::shared_ptr<Formula> substituteTranscendentalNumbers() const;

    /*!
     * Takes the formula and converts it to an equivalent expression. The formula may contain atomic labels, but
     * then the given mapping must provide a corresponding expression. Other than that, only atomic expression
     * formulas and boolean connectives may appear in the formula.
     *
     * @param manager The manager responsible for the expressions in the formula and the resulting expression.
     * @param A mapping from labels to the expressions that define them.
     * @return An equivalent expression.
     */
    storm::expressions::Expression toExpression(storm::expressions::ExpressionManager const& manager,
                                                std::map<std::string, storm::expressions::Expression> const& labelToExpressionMapping = {}) const;

    std::string toString() const;

    /*!
     * Writes the forumla to the given output stream
     * @param allowParenthesis if true, the output is *potentially* surrounded by parentheses depending on whether parentheses are needed to avoid ambiguity
     * when this formula appears as a subformula of some larger formula.
     * @return
     */
    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const = 0;

    std::string toPrefixString() const;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const;

   private:
    // Currently empty.
};

std::ostream& operator<<(std::ostream& out, Formula const& formula);
}  // namespace logic
}  // namespace storm

#endif /* STORM_LOGIC_FORMULA_H_ */
