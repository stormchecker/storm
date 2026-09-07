#pragma once

#include "storm/logic/ComparisonType.h"
#include "storm/modelchecker/helper/ModelCheckerHelper.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {
namespace helper {

/*!
 * Helper for model checking queries where we are interested in (optimizing) a single value per state.
 * @tparam ValueType The type of a value
 * @tparam ModelRepresentation The used kind of model representation
 */
template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
class SingleValueModelCheckerHelper : public ModelCheckerHelper<ValueType, ModelRepresentation> {
   public:
    SingleValueModelCheckerHelper();

    /*!
     * Sets the optimization direction, i.e., whether we want to minimize or maximize the value for each state
     * Has no effect for models without nondeterminism.
     * Has to be set if there is nondeterminism in the model.
     */
    void setOptimizationDirection(storm::solver::OptimizationDirection const& direction);

    /*!
     * Clears the optimization direction if it was set before.
     */
    void clearOptimizationDirection();

    /*!
     * @return true if there is an optimization direction set
     */
    bool isOptimizationDirectionSet() const;

    /*!
     * @pre an optimization direction has to be set before calling this.
     * @return the optimization direction.
     */
    storm::solver::OptimizationDirection const& getOptimizationDirection() const;

    /*!
     * @pre an optimization direction has to be set before calling this.
     * @return true iff the optimization goal is to minimize the value for each state
     */
    bool minimize() const;

    /*!
     * @pre an optimization direction has to be set before calling this.
     * @return true iff the optimization goal is to maximize the value for each state
     */
    bool maximize() const;

    /*!
     * @return The optimization direction (if it was set)
     */
    boost::optional<storm::solver::OptimizationDirection> getOptionalOptimizationDirection() const;

    /*!
     * Sets a goal threshold for the value at each state. If such a threshold is set, it is assumed that we are only interested
     * in the satisfaction of the threshold. Setting this allows the helper to compute values only up to the precision
     * where satisfaction of the threshold can be decided.
     * @param comparisonType The relation used when comparing computed values (left hand side) with the given threshold value (right hand side).
     * @param thresholdValue The value used on the right hand side of the comparison relation.
     */
    void setValueThreshold(storm::logic::ComparisonType const& comparisonType, ValueType const& thresholdValue);

    /*!
     * Clears the valueThreshold if it was set before.
     */
    void clearValueThreshold();

    /*!
     * @return true, if a value threshold has been set.
     */
    bool isValueThresholdSet() const;

    /*!
     * @pre A value threshold has to be set before calling this.
     * @return The relation used when comparing computed values (left hand side) with the specified threshold value (right hand side).
     */
    storm::logic::ComparisonType const& getValueThresholdComparisonType() const;

    /*!
     * @pre A value threshold has to be set before calling this.
     * @return The value used on the right hand side of the comparison relation.
     */
    ValueType const& getValueThresholdValue() const;

    /*!
     * Sets whether an optimal scheduler shall be constructed during the computation
     */
    void setProduceScheduler(bool value);

    /*!
     * @return whether an optimal scheduler shall be constructed during the computation
     */
    bool isProduceSchedulerSet() const;

    /*!
     * Sets whether the property needs to be checked qualitatively
     */
    void setQualitative(bool value);

    /*!
     * @return whether the property needs to be checked qualitatively
     */
    bool isQualitativeSet() const;

   private:
    boost::optional<storm::solver::OptimizationDirection> _optimizationDirection;
    boost::optional<std::pair<storm::logic::ComparisonType, ValueType>> _valueThreshold;
    bool _produceScheduler;
    bool _isQualitativeSet;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm