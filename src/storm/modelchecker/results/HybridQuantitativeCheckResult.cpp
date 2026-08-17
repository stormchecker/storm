#include "storm/modelchecker/results/HybridQuantitativeCheckResult.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type, typename ValueType>
HybridQuantitativeCheckResult<Type, ValueType>::HybridQuantitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates,
                                                                              storm::dd::Bdd<Type> const& symbolicStates,
                                                                              storm::dd::Add<Type, ValueType> const& symbolicValues,
                                                                              storm::dd::Bdd<Type> const& explicitStates, storm::dd::Odd const& odd,
                                                                              std::vector<ValueType> const& explicitValues)
    : reachableStates(reachableStates),
      symbolicStates(symbolicStates),
      symbolicValues(symbolicValues),
      explicitStates(explicitStates),
      odd(odd),
      explicitValues(explicitValues) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<CheckResult> HybridQuantitativeCheckResult<Type, ValueType>::clone() const {
    return std::make_unique<HybridQuantitativeCheckResult<Type, ValueType>>(this->reachableStates, this->symbolicStates, this->symbolicValues,
                                                                            this->explicitStates, this->odd, this->explicitValues);
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<CheckResult> HybridQuantitativeCheckResult<Type, ValueType>::compareAgainstBound(storm::logic::ComparisonType comparisonType,
                                                                                                 ValueType const& bound) const {
    storm::dd::Bdd<Type> symbolicResult = symbolicStates;

    // First compute the symbolic part of the result.
    if (comparisonType == storm::logic::ComparisonType::Less) {
        symbolicResult &= symbolicValues.less(bound);
    } else if (comparisonType == storm::logic::ComparisonType::LessEqual) {
        symbolicResult &= symbolicValues.lessOrEqual(bound);
    } else if (comparisonType == storm::logic::ComparisonType::Greater) {
        symbolicResult &= symbolicValues.greater(bound);
    } else if (comparisonType == storm::logic::ComparisonType::GreaterEqual) {
        symbolicResult &= symbolicValues.greaterOrEqual(bound);
    }

    // Then translate the explicit part to a symbolic format and simultaneously to a qualitative result.
    symbolicResult |= storm::dd::Bdd<Type>::template fromVector<ValueType>(this->reachableStates.getDdManager(), this->explicitValues, this->odd,
                                                                           this->symbolicValues.getContainedMetaVariables(), comparisonType,
                                                                           storm::utility::convertNumber<ValueType>(bound));

    return std::unique_ptr<SymbolicQualitativeCheckResult<Type>>(new SymbolicQualitativeCheckResult<Type>(reachableStates, symbolicResult));
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<CheckResult> HybridQuantitativeCheckResult<Type, ValueType>::toExplicitQuantitativeCheckResult() const {
    storm::dd::Bdd<Type> allStates = symbolicStates || explicitStates;
    storm::dd::Odd allStatesOdd = allStates.createOdd();

    std::vector<ValueType> fullExplicitValues = symbolicValues.toVector(allStatesOdd);
    this->odd.expandExplicitVector(allStatesOdd, this->explicitValues, fullExplicitValues);
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(fullExplicitValues)));
}

template<storm::dd::DdType Type, typename ValueType>
bool HybridQuantitativeCheckResult<Type, ValueType>::isHybrid() const {
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
bool HybridQuantitativeCheckResult<Type, ValueType>::isResultForAllStates() const {
    return (symbolicStates || explicitStates) == reachableStates;
}

template<storm::dd::DdType Type, typename ValueType>
bool HybridQuantitativeCheckResult<Type, ValueType>::isHybridQuantitativeCheckResult() const {
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& HybridQuantitativeCheckResult<Type, ValueType>::getSymbolicStates() const {
    return symbolicStates;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> const& HybridQuantitativeCheckResult<Type, ValueType>::getSymbolicValueVector() const {
    return symbolicValues;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& HybridQuantitativeCheckResult<Type, ValueType>::getExplicitStates() const {
    return explicitStates;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Odd const& HybridQuantitativeCheckResult<Type, ValueType>::getOdd() const {
    return odd;
}

template<storm::dd::DdType Type, typename ValueType>
std::vector<ValueType> const& HybridQuantitativeCheckResult<Type, ValueType>::getExplicitValueVector() const {
    return explicitValues;
}

template<typename ValueType>
void print(std::ostream& out, ValueType const& value) {
    if (value == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << value;
        if (std::is_same<ValueType, storm::RationalNumber>::value) {
            out << " (approx. " << storm::utility::convertNumber<double>(value) << ")";
        }
    }
}

template<typename ValueType>
void printRange(std::ostream& out, ValueType const& min, ValueType const& max) {
    out << "[";
    if (min == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << min;
    }
    out << ", ";
    if (max == storm::utility::infinity<ValueType>()) {
        out << "inf";
    } else {
        out << max;
    }
    out << "]";
    if (std::is_same<ValueType, storm::RationalNumber>::value) {
        out << " (approx. [";
        if (min == storm::utility::infinity<ValueType>()) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(min);
        }
        out << ", ";
        if (max == storm::utility::infinity<ValueType>()) {
            out << "inf";
        } else {
            out << storm::utility::convertNumber<double>(max);
        }
        out << "])";
    }
    out << " (range)";
}

template<storm::dd::DdType Type, typename ValueType>
std::ostream& HybridQuantitativeCheckResult<Type, ValueType>::writeToStream(std::ostream& out) const {
    uint64_t totalNumberOfStates = this->symbolicStates.getNonZeroCount() + this->explicitStates.getNonZeroCount();
    bool minMaxSupported = std::is_same<ValueType, double>::value || std::is_same<ValueType, storm::RationalNumber>::value;
    bool printAsRange = false;

    if (totalNumberOfStates == 1) {
        if (this->symbolicStates.isZero()) {
            print(out, *this->explicitValues.begin());
        } else {
            print(out, this->symbolicValues.sumAbstract(this->symbolicValues.getContainedMetaVariables()).getValue());
        }
    } else if (totalNumberOfStates >= 10 && minMaxSupported) {
        printAsRange = true;
    } else {
        out << "{";
        bool first = true;
        if (!this->symbolicStates.isZero()) {
            if (this->symbolicValues.isZero()) {
                out << "0";
                first = false;
            } else {
                for (auto valuationValuePair : this->symbolicValues) {
                    if (!first) {
                        out << ", ";
                    } else {
                        first = false;
                    }
                    print(out, valuationValuePair.second);
                }
                if (symbolicStates.getNonZeroCount() != this->symbolicValues.getNonZeroCount()) {
                    out << ", 0";
                }
            }
        }
        if (!this->explicitStates.isZero()) {
            for (auto const& element : this->explicitValues) {
                if (!first) {
                    out << ", ";
                } else {
                    first = false;
                }
                print(out, element);
            }
        }
        out << "}";
    }

    if (printAsRange) {
        ValueType min = this->getMin();
        ValueType max = this->getMax();
        printRange(out, min, max);
    }
    return out;
}

template<storm::dd::DdType Type, typename ValueType>
void HybridQuantitativeCheckResult<Type, ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isSymbolicQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter hybrid check result with non-symbolic filter.");

    // First, we filter the symbolic values.
    this->symbolicStates = this->symbolicStates && filter.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    this->symbolicValues *= symbolicStates.template toAdd<ValueType>();

    // Next, we filter the explicit values.

    // Start by computing the new set of states that is stored explictly and the corresponding ODD.
    this->explicitStates = this->explicitStates && filter.asSymbolicQualitativeCheckResult<Type>().getTruthValuesVector();
    storm::dd::Odd newOdd = explicitStates.createOdd();

    // Then compute the new vector of explicit values and set the new data fields.
    this->explicitValues = explicitStates.filterExplicitVector(this->odd, explicitValues);

    this->odd = newOdd;
}

template<storm::dd::DdType Type, typename ValueType>
ValueType HybridQuantitativeCheckResult<Type, ValueType>::getMin() const {
    // In order to not get false zeros, we need to set the values of all states whose values is not stored
    // symbolically to infinity.
    storm::dd::Add<Type, ValueType> tmp =
        symbolicStates.ite(this->symbolicValues, reachableStates.getDdManager().getConstant(storm::utility::infinity<ValueType>()));
    ValueType min = tmp.getMin();
    if (!explicitStates.isZero()) {
        for (auto const& element : explicitValues) {
            min = std::min(min, element);
        }
    }
    return min;
}

template<storm::dd::DdType Type, typename ValueType>
ValueType HybridQuantitativeCheckResult<Type, ValueType>::getMax() const {
    ValueType max = this->symbolicValues.getMax();
    if (!explicitStates.isZero()) {
        for (auto const& element : explicitValues) {
            max = std::max(max, element);
        }
    }
    return max;
}

template<storm::dd::DdType Type, typename ValueType>
ValueType HybridQuantitativeCheckResult<Type, ValueType>::sum() const {
    ValueType sum = symbolicValues.sumAbstract(symbolicValues.getContainedMetaVariables()).getValue();
    for (auto const& value : explicitValues) {
        sum += value;
    }
    return sum;
}

template<storm::dd::DdType Type, typename ValueType>
ValueType HybridQuantitativeCheckResult<Type, ValueType>::average() const {
    return this->sum() / storm::utility::convertNumber<ValueType>((symbolicStates || explicitStates).getNonZeroCount());
}

template<storm::dd::DdType Type, typename ValueType>
void HybridQuantitativeCheckResult<Type, ValueType>::oneMinus() {
    storm::dd::Add<Type, ValueType> one = symbolicValues.getDdManager().template getAddOne<ValueType>();
    storm::dd::Add<Type, ValueType> zero = symbolicValues.getDdManager().template getAddZero<ValueType>();
    symbolicValues = symbolicStates.ite(one - symbolicValues, zero);

    for (auto& element : explicitValues) {
        element = storm::utility::one<ValueType>() - element;
    }
}

// Explicitly instantiate the class.
template class HybridQuantitativeCheckResult<storm::dd::DdType::CUDD>;
template class HybridQuantitativeCheckResult<storm::dd::DdType::Sylvan>;

template class HybridQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class HybridQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace modelchecker
}  // namespace storm
