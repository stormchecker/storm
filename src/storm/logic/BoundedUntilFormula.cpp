#include "storm/logic/BoundedUntilFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
BoundedUntilFormula::BoundedUntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula,
                                         std::optional<TimeBound> const& lowerBound, std::optional<TimeBound> const& upperBound,
                                         TimeBoundReference const& timeBoundReference)
    : PathFormula(),
      leftSubformula({leftSubformula}),
      rightSubformula({rightSubformula}),
      timeBoundReference({timeBoundReference}),
      lowerBound({lowerBound}),
      upperBound({upperBound}) {
    STORM_LOG_THROW(lowerBound || upperBound, storm::exceptions::InvalidArgumentException, "Bounded until formula requires at least one bound.");
}

BoundedUntilFormula::BoundedUntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula,
                                         std::vector<std::optional<TimeBound>> const& lowerBounds, std::vector<std::optional<TimeBound>> const& upperBounds,
                                         std::vector<TimeBoundReference> const& timeBoundReferences)
    : PathFormula(),
      leftSubformula({leftSubformula}),
      rightSubformula({rightSubformula}),
      timeBoundReference(timeBoundReferences),
      lowerBound(lowerBounds),
      upperBound(upperBounds) {
    STORM_LOG_ASSERT(timeBoundReferences.size() == upperBound.size(), "Time bound reference/upper bound size mismatch.");
    STORM_LOG_ASSERT(timeBoundReferences.size() == lowerBound.size(), "Time bound reference/lower bound size mismatch.");
}

BoundedUntilFormula::BoundedUntilFormula(std::vector<std::shared_ptr<Formula const>> const& leftSubformulas,
                                         std::vector<std::shared_ptr<Formula const>> const& rightSubformulas,
                                         std::vector<std::optional<TimeBound>> const& lowerBounds, std::vector<std::optional<TimeBound>> const& upperBounds,
                                         std::vector<TimeBoundReference> const& timeBoundReferences)
    : PathFormula(),
      leftSubformula(leftSubformulas),
      rightSubformula(rightSubformulas),
      timeBoundReference(timeBoundReferences),
      lowerBound(lowerBounds),
      upperBound(upperBounds) {
    STORM_LOG_ASSERT(leftSubformula.size() == rightSubformula.size(), "Left/right subformula size mismatch.");
    STORM_LOG_ASSERT(rightSubformula.size() == timeBoundReference.size(), "Subformula/time bound reference size mismatch.");
    STORM_LOG_ASSERT(timeBoundReference.size() == lowerBound.size(), "Time bound reference/lower bound size mismatch.");
    STORM_LOG_ASSERT(lowerBound.size() == upperBound.size(), "Lower/upper bound size mismatch.");
    STORM_LOG_THROW(this->getDimension() != 0, storm::exceptions::InvalidArgumentException, "Bounded until formula requires at least one dimension.");
    for (unsigned i = 0; i < timeBoundReferences.size(); ++i) {
        STORM_LOG_THROW(hasLowerBound(i) || hasUpperBound(i), storm::exceptions::InvalidArgumentException,
                        "Bounded until formula requires at least one bound in each dimension.");
    }
}

bool BoundedUntilFormula::isBoundedUntilFormula() const {
    return true;
}

bool BoundedUntilFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any BoundedUntilFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void BoundedUntilFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    if (hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < this->getDimension(); ++i) {
            this->getLeftSubformula(i).gatherAtomicExpressionFormulas(atomicExpressionFormulas);
            this->getRightSubformula(i).gatherAtomicExpressionFormulas(atomicExpressionFormulas);
        }
    } else {
        this->getLeftSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
        this->getRightSubformula().gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    }
}

void BoundedUntilFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    if (hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < this->getDimension(); ++i) {
            this->getLeftSubformula(i).gatherAtomicLabelFormulas(atomicLabelFormulas);
            this->getRightSubformula(i).gatherAtomicLabelFormulas(atomicLabelFormulas);
        }
    } else {
        this->getLeftSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
        this->getRightSubformula().gatherAtomicLabelFormulas(atomicLabelFormulas);
    }
}

void BoundedUntilFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    for (unsigned i = 0; i < this->getDimension(); ++i) {
        if (this->getTimeBoundReference(i).isRewardBound()) {
            if (!this->getTimeBoundReference(i).hasRewardModelName()) {
                referencedRewardModels.insert("");
            } else {
                referencedRewardModels.insert(this->getTimeBoundReference(i).getRewardName());
            }
        }
    }
    if (hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < this->getDimension(); ++i) {
            this->getLeftSubformula(i).gatherReferencedRewardModels(referencedRewardModels);
            this->getRightSubformula(i).gatherReferencedRewardModels(referencedRewardModels);
        }
    } else {
        this->getLeftSubformula().gatherReferencedRewardModels(referencedRewardModels);
        this->getRightSubformula().gatherReferencedRewardModels(referencedRewardModels);
    }
}

void BoundedUntilFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    if (hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < this->getDimension(); ++i) {
            this->getLeftSubformula(i).gatherUsedVariables(usedVariables);
            this->getRightSubformula(i).gatherUsedVariables(usedVariables);
            if (this->hasLowerBound(i)) {
                this->getLowerBound(i).gatherVariables(usedVariables);
            }
            if (this->hasUpperBound(i)) {
                this->getUpperBound(i).gatherVariables(usedVariables);
            }
        }
    } else {
        this->getLeftSubformula().gatherUsedVariables(usedVariables);
        this->getRightSubformula().gatherUsedVariables(usedVariables);
        if (this->hasLowerBound(0)) {
            this->getLowerBound().gatherVariables(usedVariables);
        }
        if (this->hasUpperBound(0)) {
            this->getUpperBound().gatherVariables(usedVariables);
        }
    }
}

bool BoundedUntilFormula::hasQualitativeResult() const {
    return false;
}

bool BoundedUntilFormula::hasQuantitativeResult() const {
    return true;
}

bool BoundedUntilFormula::isMultiDimensional() const {
    STORM_LOG_ASSERT(timeBoundReference.size() != 0, "Time bound reference is empty.");
    return timeBoundReference.size() > 1;
}

bool BoundedUntilFormula::hasMultiDimensionalSubformulas() const {
    STORM_LOG_ASSERT(leftSubformula.size() != 0, "Left subformula is empty.");
    STORM_LOG_ASSERT(leftSubformula.size() == rightSubformula.size(), "Left/right subformula size mismatch.");
    return leftSubformula.size() > 1;
}

unsigned BoundedUntilFormula::getDimension() const {
    return timeBoundReference.size();
}

Formula const& BoundedUntilFormula::getLeftSubformula() const {
    STORM_LOG_ASSERT(leftSubformula.size() == 1, "The left subformula is not unique.");
    return *leftSubformula.at(0);
}

Formula const& BoundedUntilFormula::getLeftSubformula(unsigned i) const {
    if (leftSubformula.size() == 1 && i < getDimension()) {
        return getLeftSubformula();
    } else {
        return *leftSubformula.at(i);
    }
}

Formula const& BoundedUntilFormula::getRightSubformula() const {
    STORM_LOG_ASSERT(rightSubformula.size() == 1, "The right subformula is not unique.");
    return *rightSubformula.at(0);
}

Formula const& BoundedUntilFormula::getRightSubformula(unsigned i) const {
    if (rightSubformula.size() == 1 && i < getDimension()) {
        return getRightSubformula();
    } else {
        return *rightSubformula.at(i);
    }
}

TimeBoundReference const& BoundedUntilFormula::getTimeBoundReference(unsigned i) const {
    STORM_LOG_ASSERT(i < timeBoundReference.size(), "Time bound reference index out of range.");
    return timeBoundReference.at(i);
}

bool BoundedUntilFormula::isLowerBoundStrict(unsigned i) const {
    STORM_LOG_ASSERT(i < lowerBound.size(), "Lower bound index out of range.");
    if (!hasLowerBound(i)) {
        return false;
    }
    return lowerBound.at(i).value().isStrict();
}

bool BoundedUntilFormula::hasLowerBound() const {
    for (auto const& lb : lowerBound) {
        if (static_cast<bool>(lb)) {
            return true;
        }
    }
    return false;
}

bool BoundedUntilFormula::hasLowerBound(unsigned i) const {
    return static_cast<bool>(lowerBound.at(i));
}

bool BoundedUntilFormula::hasIntegerLowerBound(unsigned i) const {
    if (!hasLowerBound(i)) {
        return true;
    }
    return lowerBound.at(i).value().getBound().hasIntegerType();
}

bool BoundedUntilFormula::isUpperBoundStrict(unsigned i) const {
    return upperBound.at(i).value().isStrict();
}

bool BoundedUntilFormula::hasUpperBound() const {
    for (auto const& ub : upperBound) {
        if (static_cast<bool>(ub)) {
            return true;
        }
    }
    return false;
}

bool BoundedUntilFormula::hasUpperBound(unsigned i) const {
    return static_cast<bool>(upperBound.at(i));
}

bool BoundedUntilFormula::hasIntegerUpperBound(unsigned i) const {
    return upperBound.at(i).value().getBound().hasIntegerType();
}

storm::expressions::Expression const& BoundedUntilFormula::getLowerBound(unsigned i) const {
    return lowerBound.at(i).value().getBound();
}

storm::expressions::Expression const& BoundedUntilFormula::getUpperBound(unsigned i) const {
    return upperBound.at(i).value().getBound();
}

std::optional<TimeBound> BoundedUntilFormula::getLowerBoundAsOptionalTimeBound(unsigned i) const {
    if (hasLowerBound(i)) {
        return lowerBound.at(i).get();
    } else {
        return std::nullopt;
    }
}

std::optional<TimeBound> BoundedUntilFormula::getUpperBoundAsOptionalTimeBound(unsigned i) const {
    if (hasUpperBound(i)) {
        return upperBound.at(i).get();
    } else {
        return std::nullopt;
    }
}

template<>
double BoundedUntilFormula::getLowerBound(unsigned i) const {
    if (!hasLowerBound(i)) {
        return 0.0;
    }
    checkNoVariablesInBound(this->getLowerBound());
    double bound = this->getLowerBound(i).evaluateAsDouble();
    STORM_LOG_THROW(bound >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return bound;
}

template<>
double BoundedUntilFormula::getUpperBound(unsigned i) const {
    checkNoVariablesInBound(this->getUpperBound());
    double bound = this->getUpperBound(i).evaluateAsDouble();
    STORM_LOG_THROW(bound >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return bound;
}

template<>
storm::RationalNumber BoundedUntilFormula::getLowerBound(unsigned i) const {
    if (!hasLowerBound(i)) {
        return storm::utility::zero<storm::RationalNumber>();
    }
    checkNoVariablesInBound(this->getLowerBound(i));
    storm::RationalNumber bound = this->getLowerBound(i).evaluateAsRational();
    STORM_LOG_THROW(bound >= storm::utility::zero<storm::RationalNumber>(), storm::exceptions::InvalidPropertyException,
                    "Time-bound must not evaluate to negative number.");
    return bound;
}

template<>
storm::RationalNumber BoundedUntilFormula::getUpperBound(unsigned i) const {
    checkNoVariablesInBound(this->getUpperBound(i));
    storm::RationalNumber bound = this->getUpperBound(i).evaluateAsRational();
    STORM_LOG_THROW(bound >= storm::utility::zero<storm::RationalNumber>(), storm::exceptions::InvalidPropertyException,
                    "Time-bound must not evaluate to negative number.");
    return bound;
}

template<>
uint64_t BoundedUntilFormula::getLowerBound(unsigned i) const {
    if (!hasLowerBound(i)) {
        return 0;
    }
    checkNoVariablesInBound(this->getLowerBound(i));
    int_fast64_t bound = this->getLowerBound(i).evaluateAsInt();
    STORM_LOG_THROW(bound >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return static_cast<uint64_t>(bound);
}

template<>
uint64_t BoundedUntilFormula::getUpperBound(unsigned i) const {
    checkNoVariablesInBound(this->getUpperBound(i));
    int_fast64_t bound = this->getUpperBound(i).evaluateAsInt();
    STORM_LOG_THROW(bound >= 0, storm::exceptions::InvalidPropertyException, "Time-bound must not evaluate to negative number.");
    return static_cast<uint64_t>(bound);
}

template<>
double BoundedUntilFormula::getNonStrictUpperBound(unsigned i) const {
    double bound = getUpperBound<double>(i);
    STORM_LOG_THROW(!isUpperBoundStrict(i) || bound > 0, storm::exceptions::InvalidPropertyException,
                    "Cannot retrieve non-strict bound from strict zero-bound.");
    return bound;
}

template<>
uint64_t BoundedUntilFormula::getNonStrictUpperBound(unsigned i) const {
    int_fast64_t bound = getUpperBound<uint64_t>(i);
    if (isUpperBoundStrict(i)) {
        STORM_LOG_THROW(bound > 0, storm::exceptions::InvalidPropertyException, "Cannot retrieve non-strict bound from strict zero-bound.");
        return bound - 1;
    } else {
        return bound;
    }
}

template<>
double BoundedUntilFormula::getNonStrictLowerBound(unsigned i) const {
    double bound = getLowerBound<double>(i);
    STORM_LOG_THROW(!isLowerBoundStrict(i), storm::exceptions::InvalidPropertyException, "Cannot retrieve non-strict lower bound from strict lower-bound.");
    return bound;
}

template<>
uint64_t BoundedUntilFormula::getNonStrictLowerBound(unsigned i) const {
    int_fast64_t bound = getLowerBound<uint64_t>(i);
    if (isLowerBoundStrict(i)) {
        return bound + 1;
    } else {
        return bound;
    }
}

void BoundedUntilFormula::checkNoVariablesInBound(storm::expressions::Expression const& bound) {
    STORM_LOG_THROW(!bound.containsVariables(), storm::exceptions::InvalidOperationException,
                    "Cannot evaluate time-bound '" << bound << "' as it contains undefined constants.");
}

std::shared_ptr<BoundedUntilFormula const> BoundedUntilFormula::restrictToDimension(unsigned i) const {
    return std::make_shared<BoundedUntilFormula const>(getLeftSubformula(i).asSharedPointer(), getRightSubformula(i).asSharedPointer(), lowerBound.at(i),
                                                       upperBound.at(i), getTimeBoundReference(i));
}

std::ostream& BoundedUntilFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (hasMultiDimensionalSubformulas()) {
        out << "multi(";
        restrictToDimension(0)->writeToStream(out);
        for (unsigned i = 1; i < this->getDimension(); ++i) {
            out << ", ";
            restrictToDimension(i)->writeToStream(out);
        }
        out << ")";
    } else {
        if (allowParentheses) {
            out << "(";
        }
        this->getLeftSubformula().writeToStream(out, true);

        out << " U";
        if (this->isMultiDimensional()) {
            out << "^{";
        }
        for (unsigned i = 0; i < this->getDimension(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            if (this->getTimeBoundReference(i).isRewardBound()) {
                out << "rew";
                if (this->getTimeBoundReference(i).hasRewardAccumulation()) {
                    out << "[" << this->getTimeBoundReference(i).getRewardAccumulation() << "]";
                }
                if (this->getTimeBoundReference(i).hasRewardModelName()) {
                    out << "{\"" << this->getTimeBoundReference(i).getRewardName() << "\"}";
                }
            } else if (this->getTimeBoundReference(i).isStepBound()) {
                out << "steps";
                //} else if (this->getTimeBoundReference(i).isStepBound())
                //  Note: the 'time' keyword is optional.
                //    out << "time";
            }
            if (this->hasLowerBound(i)) {
                if (this->hasUpperBound(i)) {
                    if (this->isLowerBoundStrict(i)) {
                        out << "(";
                    } else {
                        out << "[";
                    }
                    out << this->getLowerBound(i);
                    out << ", ";
                    out << this->getUpperBound(i);
                    if (this->isUpperBoundStrict(i)) {
                        out << ")";
                    } else {
                        out << "]";
                    }
                } else {
                    if (this->isLowerBoundStrict(i)) {
                        out << ">";
                    } else {
                        out << ">=";
                    }
                    out << getLowerBound(i);
                }
            } else {
                if (this->isUpperBoundStrict(i)) {
                    out << "<";
                } else {
                    out << "<=";
                }
                out << this->getUpperBound(i);
            }
            out << " ";
        }
        if (this->isMultiDimensional()) {
            out << "}";
        }

        this->getRightSubformula().writeToStream(out, true);
        if (allowParentheses) {
            out << ")";
        }
    }

    return out;
}
}  // namespace logic
}  // namespace storm
