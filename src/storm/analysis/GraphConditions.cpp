#include "storm/analysis/GraphConditions.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/constants.h"

namespace storm {
namespace analysis {

template<typename ValueType>
ConstraintCollector<ValueType>::ConstraintCollector(storm::models::sparse::Model<ValueType> const& model) {
    process(model);
}

template<typename ValueType>
std::unordered_set<typename ConstraintType<ValueType>::val> const& ConstraintCollector<ValueType>::getWellformedConstraints() const {
    return this->wellformedConstraintSet;
}

template<typename ValueType>
std::unordered_set<typename ConstraintType<ValueType>::val> const& ConstraintCollector<ValueType>::getGraphPreservingConstraints() const {
    return this->graphPreservingConstraintSet;
}

template<typename ValueType>
std::set<storm::RationalFunctionVariable> const& ConstraintCollector<ValueType>::getVariables() const {
    return this->variableSet;
}

template<typename ValueType>
void ConstraintCollector<ValueType>::wellformedRequiresNonNegativeEntries(std::vector<ValueType> const& vec) {
    for (auto const& entry : vec) {
        if (!storm::utility::isConstant(entry)) {
            auto const& transitionVars = entry.gatherVariables();
            variableSet.insert(transitionVars.begin(), transitionVars.end());
            if (entry.denominator().isConstant()) {
                STORM_LOG_ASSERT(entry.denominator().constantPart() != 0, "Denominator should not be zero.");
                if (entry.denominator().constantPart() > 0) {
                    wellformedConstraintSet.emplace(entry.nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ);
                } else if (entry.denominator().constantPart() < 0) {
                    wellformedConstraintSet.emplace(entry.nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ);
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Should have failed before.");
                }
            } else {
                wellformedConstraintSet.emplace(entry.denominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
                wellformedConstraintSet.emplace(
                    carl::FormulaType::ITE,
                    typename ConstraintType<ValueType>::val(entry.denominator().polynomialWithCoefficient(), storm::CompareRelation::GREATER),
                    typename ConstraintType<ValueType>::val(entry.nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ),
                    typename ConstraintType<ValueType>::val(entry.nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ));
            }
        }
    }
}

template<typename ValueType>
void ConstraintCollector<ValueType>::process(storm::models::sparse::Model<ValueType> const& model) {
    if (model.getType() != storm::models::ModelType::Ctmc) {
        for (uint_fast64_t action = 0; action < model.getTransitionMatrix().getRowCount(); ++action) {
            ValueType sum = storm::utility::zero<ValueType>();

            for (auto transitionIt = model.getTransitionMatrix().begin(action); transitionIt != model.getTransitionMatrix().end(action); ++transitionIt) {
                auto const& transition = *transitionIt;
                sum += transition.getValue();
                if (!storm::utility::isConstant(transition.getValue())) {
                    auto const& transitionVars = transition.getValue().gatherVariables();
                    variableSet.insert(transitionVars.begin(), transitionVars.end());
                    // Assert: 0 <= transition <= 1
                    if (transition.getValue().denominator().isConstant()) {
                        STORM_LOG_ASSERT(transition.getValue().denominator().constantPart() != 0, "Denominator should not be zero.");
                        if (transition.getValue().denominator().constantPart() > 0) {
                            // Assert: nom <= denom
                            wellformedConstraintSet.emplace(
                                (transition.getValue().nominator() - transition.getValue().denominator()).polynomialWithCoefficient(),
                                storm::CompareRelation::LEQ);
                            // Assert: nom >= 0
                            wellformedConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ);
                        } else if (transition.getValue().denominator().constantPart() < 0) {
                            // Assert nom >= denom
                            wellformedConstraintSet.emplace(
                                (transition.getValue().nominator() - transition.getValue().denominator()).polynomialWithCoefficient(),
                                storm::CompareRelation::GEQ);
                            // Assert: nom <= 0
                            wellformedConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ);
                        } else {
                            STORM_LOG_ASSERT(false, "Denominator should not be zero.");
                        }
                    } else {
                        // Assert: denom != 0
                        wellformedConstraintSet.emplace(transition.getValue().denominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
                        // Assert: transition >= 0 <==> if denom > 0 then nom >= 0 else nom <= 0
                        wellformedConstraintSet.emplace(
                            carl::FormulaType::ITE,
                            typename ConstraintType<ValueType>::val(transition.getValue().denominator().polynomialWithCoefficient(),
                                                                    storm::CompareRelation::GREATER),
                            typename ConstraintType<ValueType>::val(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ),
                            typename ConstraintType<ValueType>::val(transition.getValue().nominator().polynomialWithCoefficient(),
                                                                    storm::CompareRelation::LEQ));
                        // TODO: Assert: transition <= 1 <==> if denom > 0 then nom - denom <= 0 else nom - denom >= 0
                    }
                    // Assert: transition > 0
                    graphPreservingConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
                }
            }
            STORM_LOG_ASSERT(!storm::utility::isConstant(sum) || storm::utility::isOne(sum), "If the sum is a constant, it must be equal to 1.");
            if (!storm::utility::isConstant(sum)) {
                // Assert: sum == 1
                wellformedConstraintSet.emplace((sum.nominator() - sum.denominator()).polynomialWithCoefficient(), storm::CompareRelation::EQ);
            }
        }
    } else {
        for (auto const& transition : model.getTransitionMatrix()) {
            if (!transition.getValue().isConstant()) {
                if (transition.getValue().denominator().isConstant()) {
                    STORM_LOG_ASSERT(transition.getValue().denominator().constantPart() != 0, "Denominator should not be zero.");
                    if (transition.getValue().denominator().constantPart() > 0) {
                        wellformedConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ);
                    } else if (transition.getValue().denominator().constantPart() < 0) {
                        wellformedConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ);
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Should have failed before.");
                    }
                } else {
                    wellformedConstraintSet.emplace(transition.getValue().denominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
                    wellformedConstraintSet.emplace(
                        carl::FormulaType::ITE,
                        typename ConstraintType<ValueType>::val(transition.getValue().denominator().polynomialWithCoefficient(),
                                                                storm::CompareRelation::GREATER),
                        typename ConstraintType<ValueType>::val(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ),
                        typename ConstraintType<ValueType>::val(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ));
                }
                graphPreservingConstraintSet.emplace(transition.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
            }
        }
    }

    if (model.getType() == storm::models::ModelType::Ctmc) {
        auto const& exitRateVector = static_cast<storm::models::sparse::Ctmc<ValueType> const&>(model).getExitRateVector();
        wellformedRequiresNonNegativeEntries(exitRateVector);
    } else if (model.getType() == storm::models::ModelType::MarkovAutomaton) {
        auto const& exitRateVector = static_cast<storm::models::sparse::MarkovAutomaton<ValueType> const&>(model).getExitRates();
        wellformedRequiresNonNegativeEntries(exitRateVector);
    }

    for (auto const& rewModelEntry : model.getRewardModels()) {
        if (rewModelEntry.second.hasStateRewards()) {
            wellformedRequiresNonNegativeEntries(rewModelEntry.second.getStateRewardVector());
        }
        if (rewModelEntry.second.hasStateActionRewards()) {
            wellformedRequiresNonNegativeEntries(rewModelEntry.second.getStateActionRewardVector());
        }
        if (rewModelEntry.second.hasTransitionRewards()) {
            for (auto const& entry : rewModelEntry.second.getTransitionRewardMatrix()) {
                if (!entry.getValue().isConstant()) {
                    if (entry.getValue().denominator().isConstant()) {
                        STORM_LOG_ASSERT(entry.getValue().denominator().constantPart() != 0, "Denominator should not be zero.");
                        if (entry.getValue().denominator().constantPart() > 0) {
                            wellformedConstraintSet.emplace(entry.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ);
                        } else if (entry.getValue().denominator().constantPart() < 0) {
                            wellformedConstraintSet.emplace(entry.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ);
                        } else {
                            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Should have failed before.");
                        }
                    } else {
                        wellformedConstraintSet.emplace(entry.getValue().denominator().polynomialWithCoefficient(), storm::CompareRelation::NEQ);
                        wellformedConstraintSet.emplace(
                            carl::FormulaType::ITE,
                            typename ConstraintType<ValueType>::val(entry.getValue().denominator().polynomialWithCoefficient(),
                                                                    storm::CompareRelation::GREATER),
                            typename ConstraintType<ValueType>::val(entry.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::GEQ),
                            typename ConstraintType<ValueType>::val(entry.getValue().nominator().polynomialWithCoefficient(), storm::CompareRelation::LEQ));
                    }
                }
            }
        }
    }
}

template<typename ValueType>
void ConstraintCollector<ValueType>::operator()(storm::models::sparse::Model<ValueType> const& model) {
    process(model);
}

template class ConstraintCollector<storm::RationalFunction>;
}  // namespace analysis
}  // namespace storm
