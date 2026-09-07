#include "SymbolicQuantitativeGameResult.h"

#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type, typename ValueType>
SymbolicQuantitativeGameResult<Type, ValueType>::SymbolicQuantitativeGameResult(storm::dd::Add<Type, ValueType> const& values) : values(values) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicQuantitativeGameResult<Type, ValueType>::SymbolicQuantitativeGameResult(boost::optional<std::pair<ValueType, ValueType>> const& initialStatesRange,
                                                                                storm::dd::Add<Type, ValueType> const& values,
                                                                                boost::optional<storm::dd::Bdd<Type>> const& player1Strategy,
                                                                                boost::optional<storm::dd::Bdd<Type>> const& player2Strategy)
    : initialStatesRange(initialStatesRange), values(values), player1Strategy(player1Strategy), player2Strategy(player2Strategy) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
bool SymbolicQuantitativeGameResult<Type, ValueType>::hasPlayer1Strategy() const {
    return static_cast<bool>(player1Strategy);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& SymbolicQuantitativeGameResult<Type, ValueType>::getPlayer1Strategy() const {
    return player1Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type>& SymbolicQuantitativeGameResult<Type, ValueType>::getPlayer1Strategy() {
    return player1Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
bool SymbolicQuantitativeGameResult<Type, ValueType>::hasPlayer2Strategy() const {
    return static_cast<bool>(player2Strategy);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& SymbolicQuantitativeGameResult<Type, ValueType>::getPlayer2Strategy() const {
    return player2Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type>& SymbolicQuantitativeGameResult<Type, ValueType>::getPlayer2Strategy() {
    return player2Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
bool SymbolicQuantitativeGameResult<Type, ValueType>::hasInitialStatesRange() const {
    return static_cast<bool>(initialStatesRange);
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<ValueType, ValueType> const& SymbolicQuantitativeGameResult<Type, ValueType>::getInitialStatesRange() const {
    return initialStatesRange.get();
}

template class SymbolicQuantitativeGameResult<storm::dd::DdType::CUDD, double>;
template class SymbolicQuantitativeGameResult<storm::dd::DdType::Sylvan, double>;
template class SymbolicQuantitativeGameResult<storm::dd::DdType::Sylvan, storm::RationalNumber>;

}  // namespace abstraction
}  // namespace storm::gbar
