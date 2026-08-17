#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::SingleValueModelCheckerHelper() : _produceScheduler(false) {
    // Intentionally left empty
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::setOptimizationDirection(storm::solver::OptimizationDirection const& direction) {
    _optimizationDirection = direction;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::clearOptimizationDirection() {
    _optimizationDirection = boost::none;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::isOptimizationDirectionSet() const {
    return _optimizationDirection.is_initialized();
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
storm::solver::OptimizationDirection const& SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::getOptimizationDirection() const {
    STORM_LOG_ASSERT(isOptimizationDirectionSet(), "Requested optimization direction but none was set.");
    return _optimizationDirection.get();
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::minimize() const {
    return storm::solver::minimize(getOptimizationDirection());
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::maximize() const {
    return storm::solver::maximize(getOptimizationDirection());
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
boost::optional<storm::solver::OptimizationDirection> SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::getOptionalOptimizationDirection() const {
    return _optimizationDirection;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::setValueThreshold(storm::logic::ComparisonType const& comparisonType,
                                                                                      ValueType const& threshold) {
    _valueThreshold = std::make_pair(comparisonType, threshold);
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::clearValueThreshold() {
    _valueThreshold = boost::none;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::isValueThresholdSet() const {
    return _valueThreshold.is_initialized();
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
storm::logic::ComparisonType const& SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::getValueThresholdComparisonType() const {
    STORM_LOG_ASSERT(isValueThresholdSet(), "Value Threshold comparison type was requested but not set before.");
    return _valueThreshold->first;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
ValueType const& SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::getValueThresholdValue() const {
    STORM_LOG_ASSERT(isValueThresholdSet(), "Value Threshold comparison type was requested but not set before.");
    return _valueThreshold->second;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::setProduceScheduler(bool value) {
    _produceScheduler = value;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::isProduceSchedulerSet() const {
    return _produceScheduler;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::setQualitative(bool value) {
    _isQualitativeSet = value;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool SingleValueModelCheckerHelper<ValueType, ModelRepresentation>::isQualitativeSet() const {
    return _isQualitativeSet;
}

template class SingleValueModelCheckerHelper<double, storm::models::ModelRepresentation::Sparse>;
template class SingleValueModelCheckerHelper<storm::RationalNumber, storm::models::ModelRepresentation::Sparse>;
template class SingleValueModelCheckerHelper<storm::RationalFunction, storm::models::ModelRepresentation::Sparse>;

template class SingleValueModelCheckerHelper<double, storm::models::ModelRepresentation::DdSylvan>;
template class SingleValueModelCheckerHelper<storm::RationalNumber, storm::models::ModelRepresentation::DdSylvan>;
template class SingleValueModelCheckerHelper<storm::RationalFunction, storm::models::ModelRepresentation::DdSylvan>;

template class SingleValueModelCheckerHelper<double, storm::models::ModelRepresentation::DdCudd>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm