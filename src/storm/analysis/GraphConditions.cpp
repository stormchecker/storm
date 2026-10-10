#include "storm/analysis/GraphConditions.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/HashVisitor.h"
#include "storm/storage/expressions/PolynomialToExpression.h"
#include "storm/utility/constants.h"

namespace storm {
namespace analysis {

namespace {

/*!
 * Retrieves the expression representing the given polynomial, declaring its variables in the given manager.
 */
template<typename PolynomialType>
storm::expressions::Expression toExpression(PolynomialType const& polynomial, std::shared_ptr<storm::expressions::ExpressionManager> const& manager) {
    return storm::expressions::polynomialToExpression(polynomial, manager);
}

/*!
 * Hashes the given expression by its structure instead of by the identity of its underlying node.
 */
std::size_t structuralHash(storm::expressions::Expression const& expression) {
    storm::expressions::HashVisitor visitor;
    return visitor.hash(expression);
}

}  // namespace

ConstraintCollector::ConstraintCollector(storm::models::sparse::Model<storm::RationalFunction> const& model)
    : expressionManager(std::make_shared<storm::expressions::ExpressionManager>()) {
    process(model);
}

ConstraintCollector::ConstraintSet const& ConstraintCollector::getWellformedConstraints() const {
    return this->wellformedConstraintSet;
}

ConstraintCollector::ConstraintSet const& ConstraintCollector::getGraphPreservingConstraints() const {
    return this->graphPreservingConstraintSet;
}

std::set<storm::RationalFunctionVariable> const& ConstraintCollector::getVariables() const {
    return this->variableSet;
}

storm::expressions::Expression ConstraintCollector::relateToZero(storm::RawPolynomial const& polynomial, storm::expressions::RelationType relation) const {
    auto zero = expressionManager->rational(storm::utility::convertNumber<storm::RationalNumber>(0));
    switch (relation) {
        case storm::expressions::RelationType::Less:
            return toExpression(polynomial, expressionManager) < zero;
        case storm::expressions::RelationType::LessOrEqual:
            return toExpression(polynomial, expressionManager) <= zero;
        case storm::expressions::RelationType::Greater:
            return toExpression(-polynomial, expressionManager) < zero;
        case storm::expressions::RelationType::GreaterOrEqual:
            return toExpression(-polynomial, expressionManager) <= zero;
        case storm::expressions::RelationType::Equal:
            return toExpression(polynomial, expressionManager) == zero;
        case storm::expressions::RelationType::NotEqual:
            return toExpression(polynomial, expressionManager) != zero;
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled relation.");
}

bool ConstraintCollector::addConstraint(ConstraintSet& constraintSet, ConstraintIndex& constraintIndex, storm::expressions::Expression const& constraint) {
    auto& candidates = constraintIndex[structuralHash(constraint)];
    for (auto const& candidate : candidates) {
        if (candidate.isSyntacticallyEqual(constraint)) {
            return false;
        }
    }
    candidates.push_back(constraint);
    constraintSet.push_back(constraint);
    return true;
}

void ConstraintCollector::wellformedRequiresNonNegativeEntries(storm::RationalFunction const& value) {
    if (storm::utility::isConstant(value)) {
        return;
    }

    auto valueVariables = value.gatherVariables();
    variableSet.insert(valueVariables.begin(), valueVariables.end());

    storm::RawPolynomial nominator = value.nominator().polynomialWithCoefficient();
    storm::RawPolynomial denominator = value.denominator().polynomialWithCoefficient();

    if (denominator.isConstant()) {
        // The sign of the denominator is known, so non-negativity of the quotient reduces to a sign constraint on
        // the nominator.
        STORM_LOG_ASSERT(denominator.constantPart() != 0, "Denominator should not be zero.");
        auto relation = denominator.constantPart() > 0 ? storm::expressions::RelationType::GreaterOrEqual : storm::expressions::RelationType::LessOrEqual;
        addConstraint(wellformedConstraintSet, wellformedConstraintIndex, relateToZero(nominator, relation));
    } else {
        // The denominator may change its sign, so we need to constrain that it never vanishes and that the sign of
        // the nominator follows the sign of the denominator.
        addConstraint(wellformedConstraintSet, wellformedConstraintIndex, relateToZero(denominator, storm::expressions::RelationType::NotEqual));
        addConstraint(wellformedConstraintSet, wellformedConstraintIndex,
                      storm::expressions::ite(relateToZero(denominator, storm::expressions::RelationType::Greater),
                                              relateToZero(nominator, storm::expressions::RelationType::GreaterOrEqual),
                                              relateToZero(nominator, storm::expressions::RelationType::LessOrEqual)));
    }
}

void ConstraintCollector::wellformedRequiresAtMostOne(storm::RationalFunction const& value) {
    if (storm::utility::isConstant(value)) {
        return;
    }

    auto denominator = value.denominator().polynomialWithCoefficient();
    if (!denominator.isConstant()) {
        // TODO: Assert: value <= 1 <==> if denom > 0 then nom - denom <= 0 else nom - denom >= 0
        // This sign reasoning cannot be expressed with the relations available in the expression system.
        return;
    }

    STORM_LOG_ASSERT(denominator.constantPart() != 0, "Denominator should not be zero.");
    storm::RawPolynomial nominator = value.nominator().polynomialWithCoefficient();
    // Express the bound as a relation of the difference to zero, so that it coincides with another constraint
    // whenever the two are equivalent.
    auto relation = denominator.constantPart() > 0 ? storm::expressions::RelationType::LessOrEqual : storm::expressions::RelationType::GreaterOrEqual;
    addConstraint(wellformedConstraintSet, wellformedConstraintIndex, relateToZero(nominator - denominator, relation));
}

void ConstraintCollector::graphPreservingRequiresNonZero(storm::RationalFunction const& value) {
    addConstraint(graphPreservingConstraintSet, graphPreservingConstraintIndex,
                  relateToZero(value.nominator().polynomialWithCoefficient(), storm::expressions::RelationType::NotEqual));
}

void ConstraintCollector::process(storm::models::sparse::Model<storm::RationalFunction> const& model) {
    bool const isCtmc = model.getType() == storm::models::ModelType::Ctmc;

    if (!isCtmc) {
        for (uint_fast64_t action = 0; action < model.getTransitionMatrix().getRowCount(); ++action) {
            storm::RationalFunction sum = storm::utility::zero<storm::RationalFunction>();

            for (auto transitionIt = model.getTransitionMatrix().begin(action); transitionIt != model.getTransitionMatrix().end(action); ++transitionIt) {
                auto const& value = transitionIt->getValue();
                sum += value;
                if (!storm::utility::isConstant(value)) {
                    // Assert: 0 <= transition <= 1
                    wellformedRequiresNonNegativeEntries(value);
                    wellformedRequiresAtMostOne(value);
                    // Assert: transition > 0, so that the underlying graph does not change.
                    graphPreservingRequiresNonZero(value);
                }
            }
            STORM_LOG_ASSERT(!storm::utility::isConstant(sum) || storm::utility::isOne(sum), "If the sum is a constant, it must be equal to 1.");
            if (!storm::utility::isConstant(sum)) {
                auto sumVariables = sum.gatherVariables();
                variableSet.insert(sumVariables.begin(), sumVariables.end());
                // Assert: sum == 1
                addConstraint(wellformedConstraintSet, wellformedConstraintIndex,
                              relateToZero(sum.nominator().polynomialWithCoefficient() - sum.denominator().polynomialWithCoefficient(),
                                           storm::expressions::RelationType::Equal));
            }
        }
    } else {
        for (auto const& transition : model.getTransitionMatrix()) {
            if (!storm::utility::isConstant(transition.getValue())) {
                // Assert: 0 <= transition
                wellformedRequiresNonNegativeEntries(transition.getValue());
                // Assert: transition > 0, so that the underlying graph does not change.
                graphPreservingRequiresNonZero(transition.getValue());
            }
        }
    }

    if (isCtmc) {
        auto const& exitRateVector = static_cast<storm::models::sparse::Ctmc<storm::RationalFunction> const&>(model).getExitRateVector();
        for (auto const& exitRate : exitRateVector) {
            wellformedRequiresNonNegativeEntries(exitRate);
        }
    } else if (model.getType() == storm::models::ModelType::MarkovAutomaton) {
        auto const& exitRateVector = static_cast<storm::models::sparse::MarkovAutomaton<storm::RationalFunction> const&>(model).getExitRates();
        for (auto const& exitRate : exitRateVector) {
            wellformedRequiresNonNegativeEntries(exitRate);
        }
    }

    for (auto const& rewModelEntry : model.getRewardModels()) {
        if (rewModelEntry.second.hasStateRewards()) {
            for (auto const& stateReward : rewModelEntry.second.getStateRewardVector()) {
                wellformedRequiresNonNegativeEntries(stateReward);
            }
        }
        if (rewModelEntry.second.hasStateActionRewards()) {
            for (auto const& stateActionReward : rewModelEntry.second.getStateActionRewardVector()) {
                wellformedRequiresNonNegativeEntries(stateActionReward);
            }
        }
        if (rewModelEntry.second.hasTransitionRewards()) {
            for (auto const& entry : rewModelEntry.second.getTransitionRewardMatrix()) {
                if (!storm::utility::isConstant(entry.getValue())) {
                    wellformedRequiresNonNegativeEntries(entry.getValue());
                }
            }
        }
    }
}

void ConstraintCollector::operator()(storm::models::sparse::Model<storm::RationalFunction> const& model) {
    process(model);
}

}  // namespace analysis
}  // namespace storm
