#include "storm-pomdp/beliefs/exploration/BeliefExplorationMatrix.h"

#include <optional>
#include "storm/adapters/RationalNumberAdapter.h"

namespace storm::pomdp::beliefs {

template<typename ValueType, typename... ExtraTransitionDataTypes>
BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::BeliefExplorationMatrix() {
    rowIndications.push_back(0u);
    rowGroupIndices.push_back(0u);
}

template<typename ValueType, typename... ExtraTransitionDataTypes>
void BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::endCurrentRow() {
    rowIndications.push_back(transitions.size());
};

template<typename ValueType, typename... ExtraTransitionDataTypes>
void BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::endCurrentRowGroup() {
    rowGroupIndices.push_back(rowIndications.size() - 1);
};

template<typename ValueType, typename... ExtraTransitionDataTypes>
std::size_t BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::rows() const {
    return rowIndications.size() - 1;
}

template<typename ValueType, typename... ExtraTransitionDataTypes>
std::size_t BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::groups() const {
    return rowGroupIndices.size() - 1;
}

template<typename ValueType, typename... ExtraTransitionDataTypes>
bool BeliefExplorationMatrix<ValueType, ExtraTransitionDataTypes...>::hasChoiceLabels() const {
    return !choiceLabels.empty();
}

template class BeliefExplorationMatrix<double>;
template class BeliefExplorationMatrix<double, std::vector<double>>;
template class BeliefExplorationMatrix<storm::RationalNumber>;
template class BeliefExplorationMatrix<storm::RationalNumber, std::vector<storm::RationalNumber>>;
template class BeliefExplorationMatrix<double, std::optional<double>, std::optional<double>>;
template class BeliefExplorationMatrix<storm::RationalNumber, std::optional<storm::RationalNumber>, std::optional<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs