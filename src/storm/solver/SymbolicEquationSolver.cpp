#include "storm/solver/SymbolicEquationSolver.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<storm::dd::DdType DdType, typename ValueType>
SymbolicEquationSolver<DdType, ValueType>::SymbolicEquationSolver(storm::dd::Bdd<DdType> const& allRows) : allRows(allRows) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::DdManager<DdType>& SymbolicEquationSolver<DdType, ValueType>::getDdManager() const {
    return this->allRows.getDdManager();
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setAllRows(storm::dd::Bdd<DdType> const& allRows) {
    this->allRows = allRows;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> const& SymbolicEquationSolver<DdType, ValueType>::getAllRows() const {
    return this->allRows;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setLowerBounds(storm::dd::Add<DdType, ValueType> const& lowerBounds) {
    this->lowerBounds = lowerBounds;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setLowerBound(ValueType const& lowerBound) {
    this->lowerBound = lowerBound;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setUpperBounds(storm::dd::Add<DdType, ValueType> const& upperBounds) {
    this->upperBounds = upperBounds;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setUpperBound(ValueType const& upperBound) {
    this->upperBound = upperBound;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setBounds(ValueType const& lowerBound, ValueType const& upperBound) {
    setLowerBound(lowerBound);
    setUpperBound(upperBound);
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicEquationSolver<DdType, ValueType>::setBounds(storm::dd::Add<DdType, ValueType> const& lowerBounds,
                                                          storm::dd::Add<DdType, ValueType> const& upperBounds) {
    setLowerBounds(lowerBounds);
    setUpperBounds(upperBounds);
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicEquationSolver<DdType, ValueType>::hasLowerBound() const {
    return static_cast<bool>(lowerBound);
}

template<storm::dd::DdType DdType, typename ValueType>
ValueType const& SymbolicEquationSolver<DdType, ValueType>::getLowerBound() const {
    return lowerBound.get();
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicEquationSolver<DdType, ValueType>::hasLowerBounds() const {
    return static_cast<bool>(lowerBounds);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& SymbolicEquationSolver<DdType, ValueType>::getLowerBounds() const {
    return lowerBounds.get();
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicEquationSolver<DdType, ValueType>::hasUpperBound() const {
    return static_cast<bool>(upperBound);
}

template<storm::dd::DdType DdType, typename ValueType>
ValueType const& SymbolicEquationSolver<DdType, ValueType>::getUpperBound() const {
    return upperBound.get();
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicEquationSolver<DdType, ValueType>::hasUpperBounds() const {
    return static_cast<bool>(upperBounds);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& SymbolicEquationSolver<DdType, ValueType>::getUpperBounds() const {
    return upperBounds.get();
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicEquationSolver<DdType, ValueType>::getLowerBoundsVector() const {
    STORM_LOG_THROW(lowerBound || lowerBounds, storm::exceptions::UnmetRequirementException, "Requiring lower bounds, but did not get any.");
    if (lowerBounds) {
        return lowerBounds.get();
    } else {
        return this->allRows.ite(this->allRows.getDdManager().getConstant(lowerBound.get()), this->allRows.getDdManager().template getAddZero<ValueType>());
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicEquationSolver<DdType, ValueType>::getUpperBoundsVector() const {
    STORM_LOG_THROW(upperBound || upperBounds, storm::exceptions::UnmetRequirementException, "Requiring upper bounds, but did not get any.");
    if (upperBounds) {
        return upperBounds.get();
    } else {
        return this->allRows.ite(this->allRows.getDdManager().getConstant(upperBound.get()), this->allRows.getDdManager().template getAddZero<ValueType>());
    }
}

template class SymbolicEquationSolver<storm::dd::DdType::CUDD, double>;
template class SymbolicEquationSolver<storm::dd::DdType::Sylvan, double>;

template class SymbolicEquationSolver<storm::dd::DdType::CUDD, storm::RationalNumber>;
template class SymbolicEquationSolver<storm::dd::DdType::Sylvan, storm::RationalNumber>;

template class SymbolicEquationSolver<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace solver
}  // namespace storm
