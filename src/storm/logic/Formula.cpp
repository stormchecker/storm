#include <boost/any.hpp>
#include <sstream>
#include "storm/logic/Formulas.h"

#include "storm/logic/ExpressionSubstitutionVisitor.h"
#include "storm/logic/FormulaInformationVisitor.h"
#include "storm/logic/FragmentChecker.h"
#include "storm/logic/LabelSubstitutionVisitor.h"
#include "storm/logic/RewardModelNameSubstitutionVisitor.h"
#include "storm/logic/ToExpressionVisitor.h"
#include "storm/logic/ToPrefixStringVisitor.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"

namespace storm {
namespace logic {
boost::any Formula::accept(FormulaVisitor const& visitor) const {
    return accept(visitor, boost::any());
}

bool Formula::isPathFormula() const {
    return false;
}

bool Formula::isStateFormula() const {
    return false;
}

bool Formula::isMultiObjectiveFormula() const {
    return false;
}

bool Formula::isQuantileFormula() const {
    return false;
}

bool Formula::isBinaryStateFormula() const {
    return false;
}

bool Formula::isUnaryStateFormula() const {
    return false;
}

bool Formula::isBinaryBooleanStateFormula() const {
    return false;
}

bool Formula::isUnaryBooleanStateFormula() const {
    return false;
}

bool Formula::isBinaryBooleanPathFormula() const {
    return false;
}

bool Formula::isUnaryBooleanPathFormula() const {
    return false;
}

bool Formula::isBooleanLiteralFormula() const {
    return false;
}

bool Formula::isTrueFormula() const {
    return false;
}

bool Formula::isFalseFormula() const {
    return false;
}

bool Formula::isAtomicExpressionFormula() const {
    return false;
}

bool Formula::isAtomicLabelFormula() const {
    return false;
}

bool Formula::isUntilFormula() const {
    return false;
}

bool Formula::isBoundedUntilFormula() const {
    return false;
}

bool Formula::isEventuallyFormula() const {
    return false;
}

bool Formula::isReachabilityProbabilityFormula() const {
    return false;
}

bool Formula::isGloballyFormula() const {
    return false;
}

bool Formula::isHOAPathFormula() const {
    return false;
}

bool Formula::isBinaryPathFormula() const {
    return false;
}

bool Formula::isUnaryPathFormula() const {
    return false;
}

bool Formula::isConditionalProbabilityFormula() const {
    return false;
}

bool Formula::isConditionalRewardFormula() const {
    return false;
}

bool Formula::isProbabilityPathFormula() const {
    return false;
}

bool Formula::isRewardPathFormula() const {
    return false;
}

bool Formula::isTimePathFormula() const {
    return false;
}

bool Formula::isNextFormula() const {
    return false;
}

bool Formula::isLongRunAverageOperatorFormula() const {
    return false;
}

bool Formula::isTimeOperatorFormula() const {
    return false;
}

bool Formula::isCumulativeRewardFormula() const {
    return false;
}

bool Formula::isDiscountedCumulativeRewardFormula() const {
    return false;
}

bool Formula::isInstantaneousRewardFormula() const {
    return false;
}

bool Formula::isReachabilityRewardFormula() const {
    return false;
}

bool Formula::isLongRunAverageRewardFormula() const {
    return false;
}

bool Formula::isTotalRewardFormula() const {
    return false;
}

bool Formula::isDiscountedTotalRewardFormula() const {
    return false;
}

bool Formula::isReachabilityTimeFormula() const {
    return false;
}

bool Formula::isGameFormula() const {
    return false;
}

bool Formula::isProbabilityOperatorFormula() const {
    return false;
}

bool Formula::isRewardOperatorFormula() const {
    return false;
}

bool Formula::isOperatorFormula() const {
    return false;
}

bool Formula::isUnaryFormula() const {
    return isUnaryPathFormula() || isUnaryStateFormula();
}

bool Formula::hasQualitativeResult() const {
    return true;
}

bool Formula::hasQuantitativeResult() const {
    return false;
}

bool Formula::isInFragment(FragmentSpecification const& fragment) const {
    FragmentChecker checker;
    return checker.conformsToSpecification(*this, fragment);
}

FormulaInformation Formula::info(bool recurseIntoOperators) const {
    return FormulaInformationVisitor::getInformation(*this, recurseIntoOperators);
}

std::shared_ptr<Formula const> Formula::getTrueFormula() {
    return std::shared_ptr<Formula const>(new BooleanLiteralFormula(true));
}

bool Formula::isInitialFormula() const {
    return this->isAtomicLabelFormula() && this->asAtomicLabelFormula().getLabel() == "init";
}

PathFormula& Formula::asPathFormula() {
    return dynamic_cast<PathFormula&>(*this);
}

PathFormula const& Formula::asPathFormula() const {
    return dynamic_cast<PathFormula const&>(*this);
}

StateFormula& Formula::asStateFormula() {
    return dynamic_cast<StateFormula&>(*this);
}

StateFormula const& Formula::asStateFormula() const {
    return dynamic_cast<StateFormula const&>(*this);
}

MultiObjectiveFormula& Formula::asMultiObjectiveFormula() {
    return dynamic_cast<MultiObjectiveFormula&>(*this);
}

MultiObjectiveFormula const& Formula::asMultiObjectiveFormula() const {
    return dynamic_cast<MultiObjectiveFormula const&>(*this);
}

QuantileFormula& Formula::asQuantileFormula() {
    return dynamic_cast<QuantileFormula&>(*this);
}

QuantileFormula const& Formula::asQuantileFormula() const {
    return dynamic_cast<QuantileFormula const&>(*this);
}

BinaryStateFormula& Formula::asBinaryStateFormula() {
    return dynamic_cast<BinaryStateFormula&>(*this);
}

BinaryStateFormula const& Formula::asBinaryStateFormula() const {
    return dynamic_cast<BinaryStateFormula const&>(*this);
}

UnaryStateFormula& Formula::asUnaryStateFormula() {
    return dynamic_cast<UnaryStateFormula&>(*this);
}

UnaryStateFormula const& Formula::asUnaryStateFormula() const {
    return dynamic_cast<UnaryStateFormula const&>(*this);
}

ConditionalFormula& Formula::asConditionalFormula() {
    return dynamic_cast<ConditionalFormula&>(*this);
}

ConditionalFormula const& Formula::asConditionalFormula() const {
    return dynamic_cast<ConditionalFormula const&>(*this);
}

BinaryBooleanStateFormula& Formula::asBinaryBooleanStateFormula() {
    return dynamic_cast<BinaryBooleanStateFormula&>(*this);
}

BinaryBooleanStateFormula const& Formula::asBinaryBooleanStateFormula() const {
    return dynamic_cast<BinaryBooleanStateFormula const&>(*this);
}

UnaryBooleanStateFormula& Formula::asUnaryBooleanStateFormula() {
    return dynamic_cast<UnaryBooleanStateFormula&>(*this);
}

UnaryBooleanStateFormula const& Formula::asUnaryBooleanStateFormula() const {
    return dynamic_cast<UnaryBooleanStateFormula const&>(*this);
}

BooleanLiteralFormula& Formula::asBooleanLiteralFormula() {
    return dynamic_cast<BooleanLiteralFormula&>(*this);
}

BooleanLiteralFormula const& Formula::asBooleanLiteralFormula() const {
    return dynamic_cast<BooleanLiteralFormula const&>(*this);
}

AtomicExpressionFormula& Formula::asAtomicExpressionFormula() {
    return dynamic_cast<AtomicExpressionFormula&>(*this);
}

AtomicExpressionFormula const& Formula::asAtomicExpressionFormula() const {
    return dynamic_cast<AtomicExpressionFormula const&>(*this);
}

AtomicLabelFormula& Formula::asAtomicLabelFormula() {
    return dynamic_cast<AtomicLabelFormula&>(*this);
}

AtomicLabelFormula const& Formula::asAtomicLabelFormula() const {
    return dynamic_cast<AtomicLabelFormula const&>(*this);
}

HOAPathFormula& Formula::asHOAPathFormula() {
    return dynamic_cast<HOAPathFormula&>(*this);
}

HOAPathFormula const& Formula::asHOAPathFormula() const {
    return dynamic_cast<HOAPathFormula const&>(*this);
}

UntilFormula& Formula::asUntilFormula() {
    return dynamic_cast<UntilFormula&>(*this);
}

UntilFormula const& Formula::asUntilFormula() const {
    return dynamic_cast<UntilFormula const&>(*this);
}

BoundedUntilFormula& Formula::asBoundedUntilFormula() {
    return dynamic_cast<BoundedUntilFormula&>(*this);
}

BoundedUntilFormula const& Formula::asBoundedUntilFormula() const {
    return dynamic_cast<BoundedUntilFormula const&>(*this);
}

EventuallyFormula& Formula::asEventuallyFormula() {
    return dynamic_cast<EventuallyFormula&>(*this);
}

EventuallyFormula const& Formula::asEventuallyFormula() const {
    return dynamic_cast<EventuallyFormula const&>(*this);
}

EventuallyFormula& Formula::asReachabilityRewardFormula() {
    return dynamic_cast<EventuallyFormula&>(*this);
}

EventuallyFormula const& Formula::asReachabilityRewardFormula() const {
    return dynamic_cast<EventuallyFormula const&>(*this);
}

EventuallyFormula& Formula::asReachabilityProbabilityFormula() {
    return dynamic_cast<EventuallyFormula&>(*this);
}

EventuallyFormula const& Formula::asReachabilityProbabilityFormula() const {
    return dynamic_cast<EventuallyFormula const&>(*this);
}

EventuallyFormula& Formula::asReachabilityTimeFormula() {
    return dynamic_cast<EventuallyFormula&>(*this);
}

EventuallyFormula const& Formula::asReachabilityTimeFormula() const {
    return dynamic_cast<EventuallyFormula const&>(*this);
}

GameFormula& Formula::asGameFormula() {
    return dynamic_cast<GameFormula&>(*this);
}

GameFormula const& Formula::asGameFormula() const {
    return dynamic_cast<GameFormula const&>(*this);
}

GloballyFormula& Formula::asGloballyFormula() {
    return dynamic_cast<GloballyFormula&>(*this);
}

GloballyFormula const& Formula::asGloballyFormula() const {
    return dynamic_cast<GloballyFormula const&>(*this);
}

BinaryPathFormula& Formula::asBinaryPathFormula() {
    return dynamic_cast<BinaryPathFormula&>(*this);
}

BinaryPathFormula const& Formula::asBinaryPathFormula() const {
    return dynamic_cast<BinaryPathFormula const&>(*this);
}

UnaryPathFormula& Formula::asUnaryPathFormula() {
    return dynamic_cast<UnaryPathFormula&>(*this);
}

UnaryPathFormula const& Formula::asUnaryPathFormula() const {
    return dynamic_cast<UnaryPathFormula const&>(*this);
}

NextFormula& Formula::asNextFormula() {
    return dynamic_cast<NextFormula&>(*this);
}

NextFormula const& Formula::asNextFormula() const {
    return dynamic_cast<NextFormula const&>(*this);
}

LongRunAverageOperatorFormula& Formula::asLongRunAverageOperatorFormula() {
    return dynamic_cast<LongRunAverageOperatorFormula&>(*this);
}

LongRunAverageOperatorFormula const& Formula::asLongRunAverageOperatorFormula() const {
    return dynamic_cast<LongRunAverageOperatorFormula const&>(*this);
}

TimeOperatorFormula& Formula::asTimeOperatorFormula() {
    return dynamic_cast<TimeOperatorFormula&>(*this);
}

TimeOperatorFormula const& Formula::asTimeOperatorFormula() const {
    return dynamic_cast<TimeOperatorFormula const&>(*this);
}

CumulativeRewardFormula& Formula::asCumulativeRewardFormula() {
    return dynamic_cast<CumulativeRewardFormula&>(*this);
}

CumulativeRewardFormula const& Formula::asCumulativeRewardFormula() const {
    return dynamic_cast<CumulativeRewardFormula const&>(*this);
}

TotalRewardFormula& Formula::asTotalRewardFormula() {
    return dynamic_cast<TotalRewardFormula&>(*this);
}

TotalRewardFormula const& Formula::asTotalRewardFormula() const {
    return dynamic_cast<TotalRewardFormula const&>(*this);
}

DiscountedCumulativeRewardFormula& Formula::asDiscountedCumulativeRewardFormula() {
    return dynamic_cast<DiscountedCumulativeRewardFormula&>(*this);
}

DiscountedCumulativeRewardFormula const& Formula::asDiscountedCumulativeRewardFormula() const {
    return dynamic_cast<DiscountedCumulativeRewardFormula const&>(*this);
}

DiscountedTotalRewardFormula& Formula::asDiscountedTotalRewardFormula() {
    return dynamic_cast<DiscountedTotalRewardFormula&>(*this);
}
DiscountedTotalRewardFormula const& Formula::asDiscountedTotalRewardFormula() const {
    return dynamic_cast<DiscountedTotalRewardFormula const&>(*this);
}

InstantaneousRewardFormula& Formula::asInstantaneousRewardFormula() {
    return dynamic_cast<InstantaneousRewardFormula&>(*this);
}

InstantaneousRewardFormula const& Formula::asInstantaneousRewardFormula() const {
    return dynamic_cast<InstantaneousRewardFormula const&>(*this);
}

LongRunAverageRewardFormula& Formula::asLongRunAverageRewardFormula() {
    return dynamic_cast<LongRunAverageRewardFormula&>(*this);
}

LongRunAverageRewardFormula const& Formula::asLongRunAverageRewardFormula() const {
    return dynamic_cast<LongRunAverageRewardFormula const&>(*this);
}

ProbabilityOperatorFormula& Formula::asProbabilityOperatorFormula() {
    return dynamic_cast<ProbabilityOperatorFormula&>(*this);
}

ProbabilityOperatorFormula const& Formula::asProbabilityOperatorFormula() const {
    return dynamic_cast<ProbabilityOperatorFormula const&>(*this);
}

RewardOperatorFormula& Formula::asRewardOperatorFormula() {
    return dynamic_cast<RewardOperatorFormula&>(*this);
}

RewardOperatorFormula const& Formula::asRewardOperatorFormula() const {
    return dynamic_cast<RewardOperatorFormula const&>(*this);
}

OperatorFormula& Formula::asOperatorFormula() {
    return dynamic_cast<OperatorFormula&>(*this);
}

OperatorFormula const& Formula::asOperatorFormula() const {
    return dynamic_cast<OperatorFormula const&>(*this);
}

std::vector<std::shared_ptr<AtomicExpressionFormula const>> Formula::getAtomicExpressionFormulas() const {
    std::vector<std::shared_ptr<AtomicExpressionFormula const>> result;
    this->gatherAtomicExpressionFormulas(result);
    return result;
}

std::vector<std::shared_ptr<AtomicLabelFormula const>> Formula::getAtomicLabelFormulas() const {
    std::vector<std::shared_ptr<AtomicLabelFormula const>> result;
    this->gatherAtomicLabelFormulas(result);
    return result;
}

std::set<storm::expressions::Variable> Formula::getUsedVariables() const {
    std::set<storm::expressions::Variable> usedVariables;
    this->gatherUsedVariables(usedVariables);
    return usedVariables;
}

std::set<std::string> Formula::getReferencedRewardModels() const {
    std::set<std::string> referencedRewardModels;
    this->gatherReferencedRewardModels(referencedRewardModels);
    return referencedRewardModels;
}

std::shared_ptr<Formula> Formula::clone() const {
    CloneVisitor cv;
    return cv.clone(*this);
}

std::shared_ptr<Formula> Formula::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    storm::expressions::JaniExpressionSubstitutionVisitor<std::map<storm::expressions::Variable, storm::expressions::Expression>> v(substitution, false);
    return substitute([&v](storm::expressions::Expression const& exp) { return v.substitute(exp); });
}

std::shared_ptr<Formula> Formula::substitute(
    std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& expressionSubstitution) const {
    ExpressionSubstitutionVisitor visitor;
    return visitor.substitute(*this, expressionSubstitution);
}

std::shared_ptr<Formula> Formula::substitute(std::map<std::string, storm::expressions::Expression> const& labelSubstitution) const {
    LabelSubstitutionVisitor visitor(labelSubstitution);
    return visitor.substitute(*this);
}

std::shared_ptr<Formula> Formula::substitute(std::map<std::string, std::string> const& labelSubstitution) const {
    LabelSubstitutionVisitor visitor(labelSubstitution);
    return visitor.substitute(*this);
}

std::shared_ptr<Formula> Formula::substituteRewardModelNames(std::map<std::string, std::string> const& rewardModelNameSubstitution) const {
    RewardModelNameSubstitutionVisitor visitor(rewardModelNameSubstitution);
    return visitor.substitute(*this);
}

std::shared_ptr<Formula> Formula::substituteTranscendentalNumbers() const {
    using SubMap = std::map<storm::expressions::Variable, storm::expressions::Expression>;
    storm::expressions::JaniExpressionSubstitutionVisitor<SubMap> transcendentalsVisitor(SubMap(), true);
    return substitute([&transcendentalsVisitor](storm::expressions::Expression const& exp) { return transcendentalsVisitor.substitute(exp); });
}

storm::expressions::Expression Formula::toExpression(storm::expressions::ExpressionManager const& manager,
                                                     std::map<std::string, storm::expressions::Expression> const& labelToExpressionMapping) const {
    ToExpressionVisitor visitor;
    if (labelToExpressionMapping.empty()) {
        return visitor.toExpression(*this, manager);
    } else {
        return visitor.toExpression(*this->substitute(labelToExpressionMapping), manager);
    }
}

std::shared_ptr<Formula const> Formula::asSharedPointer() {
    return this->shared_from_this();
}

std::shared_ptr<Formula const> Formula::asSharedPointer() const {
    return this->shared_from_this();
}

void Formula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>&) const {}

void Formula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>&) const {}

void Formula::gatherReferencedRewardModels(std::set<std::string>&) const {}

void Formula::gatherUsedVariables(std::set<storm::expressions::Variable>&) const {}

std::string Formula::toString() const {
    std::stringstream str2;
    writeToStream(str2);
    return str2.str();
}

std::ostream& operator<<(std::ostream& out, Formula const& formula) {
    return formula.writeToStream(out);
}

std::string Formula::toPrefixString() const {
    ToPrefixStringVisitor visitor;
    return visitor.toPrefixString(*this);
}

}  // namespace logic
}  // namespace storm
