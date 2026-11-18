#include "storm/modelchecker/helper/ModelCheckerHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void ModelCheckerHelper<ValueType, ModelRepresentation>::setRelevantStates(StateSet const& relevantStates) {
    _relevantStates = relevantStates;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
void ModelCheckerHelper<ValueType, ModelRepresentation>::clearRelevantStates() {
    _relevantStates = boost::none;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
bool ModelCheckerHelper<ValueType, ModelRepresentation>::hasRelevantStates() const {
    return _relevantStates.is_initialized();
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
boost::optional<typename ModelCheckerHelper<ValueType, ModelRepresentation>::StateSet> const&
ModelCheckerHelper<ValueType, ModelRepresentation>::getOptionalRelevantStates() const {
    return _relevantStates;
}

template<typename ValueType, storm::models::ModelRepresentation ModelRepresentation>
typename ModelCheckerHelper<ValueType, ModelRepresentation>::StateSet const& ModelCheckerHelper<ValueType, ModelRepresentation>::getRelevantStates() const {
    STORM_LOG_ASSERT(hasRelevantStates(), "Retrieving relevant states although none have been set.");
    return _relevantStates.get();
}

template class ModelCheckerHelper<double, storm::models::ModelRepresentation::Sparse>;
template class ModelCheckerHelper<storm::RationalNumber, storm::models::ModelRepresentation::Sparse>;
template class ModelCheckerHelper<storm::RationalFunction, storm::models::ModelRepresentation::Sparse>;

#define STORM_DD_INSTANTIATE(DD, ValueType) \
    template class ModelCheckerHelper<ValueType, storm::models::GetModelRepresentation<storm::dd::DdType::DD>::representation>;
STORM_DD_FOR_EACH_STANDARD(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm