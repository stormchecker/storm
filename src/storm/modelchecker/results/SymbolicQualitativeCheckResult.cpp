#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

#include "storm/exceptions/NotImplementedException.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type>
SymbolicQualitativeCheckResult<Type>::SymbolicQualitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates, storm::dd::Bdd<Type> const& truthValues)
    : reachableStates(reachableStates), states(reachableStates), truthValues(truthValues) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
SymbolicQualitativeCheckResult<Type>::SymbolicQualitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates, storm::dd::Bdd<Type> const& states,
                                                                     storm::dd::Bdd<Type> const& truthValues)
    : reachableStates(reachableStates), states(states), truthValues(truthValues) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
std::unique_ptr<CheckResult> SymbolicQualitativeCheckResult<Type>::clone() const {
    return std::make_unique<SymbolicQualitativeCheckResult<Type>>(this->reachableStates, this->states, this->truthValues);
}

template<storm::dd::DdType Type>
bool SymbolicQualitativeCheckResult<Type>::isSymbolic() const {
    return true;
}

template<storm::dd::DdType Type>
bool SymbolicQualitativeCheckResult<Type>::isResultForAllStates() const {
    return reachableStates == states;
}

template<storm::dd::DdType Type>
bool SymbolicQualitativeCheckResult<Type>::isSymbolicQualitativeCheckResult() const {
    return true;
}

template<storm::dd::DdType Type>
QualitativeCheckResult& SymbolicQualitativeCheckResult<Type>::operator&=(QualitativeCheckResult const& other) {
    STORM_LOG_THROW(other.isSymbolicQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot perform logical 'and' on check results of incompatible type.");
    this->truthValues &= other.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    return *this;
}

template<storm::dd::DdType Type>
QualitativeCheckResult& SymbolicQualitativeCheckResult<Type>::operator|=(QualitativeCheckResult const& other) {
    STORM_LOG_THROW(other.isSymbolicQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot perform logical 'and' on check results of incompatible type.");
    this->truthValues |= other.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    return *this;
}

template<storm::dd::DdType Type>
void SymbolicQualitativeCheckResult<Type>::complement() {
    this->truthValues = !this->truthValues && reachableStates;
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicQualitativeCheckResult<Type>::getTruthValuesVector() const {
    return this->truthValues;
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicQualitativeCheckResult<Type>::getStates() const {
    return states;
}

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> const& SymbolicQualitativeCheckResult<Type>::getReachableStates() const {
    return reachableStates;
}

template<storm::dd::DdType Type>
bool SymbolicQualitativeCheckResult<Type>::existsTrue() const {
    return !this->truthValues.isZero();
}

template<storm::dd::DdType Type>
bool SymbolicQualitativeCheckResult<Type>::forallTrue() const {
    return this->truthValues == this->states;
}

template<storm::dd::DdType Type>
uint64_t SymbolicQualitativeCheckResult<Type>::count() const {
    return this->truthValues.getNonZeroCount();
}

template<storm::dd::DdType Type>
std::ostream& SymbolicQualitativeCheckResult<Type>::writeToStream(std::ostream& out) const {
    if (states.getNonZeroCount() == 1) {
        if (truthValues.isZero()) {
            out << "false";
        } else {
            out << "true";
        }
    } else if (states == truthValues) {
        out << "{true}\n";
    } else {
        if (truthValues.isZero()) {
            out << "{false}\n";
        } else {
            out << "{true, false}\n";
        }
    }
    return out;
}

template<storm::dd::DdType Type>
void SymbolicQualitativeCheckResult<Type>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isSymbolicQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter symbolic check result with non-symbolic filter.");
    this->states &= filter.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    this->truthValues &= filter.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    this->states &= filter.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
}

template class SymbolicQualitativeCheckResult<storm::dd::DdType::CUDD>;
template class SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>;
}  // namespace modelchecker
}  // namespace storm
