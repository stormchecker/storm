#include "storm/storage/dd/sylvan/InternalSylvanAdd.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/sylvan/InternalSylvanDdManager.h"
#include "storm/storage/dd/sylvan/SylvanAddIterator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_SYLVAN
template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>::InternalAdd() : ddManager(nullptr), sylvanMtbdd() {
    // Intentionally left empty.
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>::InternalAdd(InternalDdManager<DdType::Sylvan> const* ddManager, sylvan::Mtbdd const& sylvanMtbdd)
    : ddManager(ddManager), sylvanMtbdd(sylvanMtbdd) {
    // Intentionally left empty.
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getValue(MTBDD const& node) {
    STORM_LOG_ASSERT(mtbdd_isleaf(node), "Expected leaf, but got variable " << mtbdd_getvar(node) << ".");

    bool negated = mtbdd_hascomp(node);
    MTBDD n = mtbdd_regular(node);

    if (std::is_same<ValueType, double>::value) {
        STORM_LOG_ASSERT(mtbdd_gettype(n) == 1, "Expected a double value.");
        return negated ? -mtbdd_getdouble(n) : mtbdd_getdouble(n);
    } else if (std::is_same<ValueType, uint_fast64_t>::value) {
        STORM_LOG_ASSERT(mtbdd_gettype(node) == 0, "Expected an unsigned value.");
        return negated ? -mtbdd_getint64(node) : mtbdd_getint64(node);
    } else if (std::is_same<ValueType, storm::RationalFunction>::value) {
        STORM_LOG_ASSERT(false, "Non-specialized version of getValue() called for storm::RationalFunction value.");
    } else {
        STORM_LOG_ASSERT(false, "Illegal or unknown type in MTBDD.");
    }
}

template<>
storm::RationalNumber InternalAdd<DdType::Sylvan, storm::RationalNumber>::getValue(MTBDD const& node) {
    STORM_LOG_ASSERT(mtbdd_isleaf(node), "Expected leaf, but got variable " << mtbdd_getvar(node) << ".");

    bool negated = mtbdd_hascomp(node);

    STORM_LOG_ASSERT(mtbdd_gettype(node) == sylvan_storm_rational_number_get_type(), "Expected a storm::RationalNumber value.");
    storm_rational_number_ptr ptr = (storm_rational_number_ptr)mtbdd_getstorm_rational_number_ptr(node);
    storm::RationalNumber* rationalNumber = (storm::RationalNumber*)(ptr);

    return negated ? -(*rationalNumber) : (*rationalNumber);
}

template<>
storm::RationalFunction InternalAdd<DdType::Sylvan, storm::RationalFunction>::getValue(MTBDD const& node) {
    STORM_LOG_ASSERT(mtbdd_isleaf(node), "Expected leaf, but got variable " << mtbdd_getvar(node) << ".");

    bool negated = mtbdd_hascomp(node);

    STORM_LOG_ASSERT(mtbdd_gettype(node) == sylvan_storm_rational_function_get_type(), "Expected a storm::RationalFunction value.");
    uint64_t value = mtbdd_getvalue(node);
    storm_rational_function_ptr ptr = (storm_rational_function_ptr)value;

    storm::RationalFunction* rationalFunction = (storm::RationalFunction*)(ptr);

    return negated ? -(*rationalFunction) : (*rationalFunction);
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::operator==(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return this->sylvanMtbdd == other.sylvanMtbdd;
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::operator!=(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return this->sylvanMtbdd != other.sylvanMtbdd;
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator+(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Plus(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator+(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.PlusRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator+(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.PlusRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator+=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.Plus(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber>& InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator+=(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.PlusRN(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction>& InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator+=(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.PlusRF(other.sylvanMtbdd);
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator*(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Times(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator*(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.TimesRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator*(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.TimesRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator*=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.Times(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber>& InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator*=(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.TimesRN(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction>& InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator*=(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.TimesRF(other.sylvanMtbdd);
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator-(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Minus(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator-(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.MinusRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator-(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.MinusRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator-=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.Minus(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber>& InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator-=(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.MinusRN(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction>& InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator-=(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.MinusRF(other.sylvanMtbdd);
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator/(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Divide(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator/(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.DivideRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator/(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.DivideRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator/=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.Divide(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber>& InternalAdd<DdType::Sylvan, storm::RationalNumber>::operator/=(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.DivideRN(other.sylvanMtbdd);
    return *this;
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction>& InternalAdd<DdType::Sylvan, storm::RationalFunction>::operator/=(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) {
    this->sylvanMtbdd = this->sylvanMtbdd.DivideRF(other.sylvanMtbdd);
    return *this;
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::equals(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.Equals(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::equals(InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.EqualsRN(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalFunction>::equals(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.EqualsRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::notEquals(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return !this->equals(other);
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::less(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.Less(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::less(InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.LessRN(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalFunction>::less(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.LessRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::lessOrEqual(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.LessOrEqual(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::lessOrEqual(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.LessOrEqualRN(other.sylvanMtbdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalFunction>::lessOrEqual(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.LessOrEqualRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greater(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return !this->lessOrEqual(other);
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greaterOrEqual(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return !this->less(other);
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::pow(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Pow(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::pow(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.PowRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::pow(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.PowRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::mod(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Mod(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::mod(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.ModRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::mod(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Operation (mod) not supported by rational functions.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::logxy(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Logxy(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::logxy(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Operation (logxy) not supported by rational numbers.");
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::logxy(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Operation (logxy) not supported by rational functions.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::floor() const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Floor());
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::floor() const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.FloorRN());
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::floor() const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.FloorRF());
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::ceil() const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Ceil());
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::ceil() const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.CeilRN());
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::ceil() const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.CeilRF());
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, ValueType>::sharpenKwekMehlhorn(size_t precision) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.SharpenKwekMehlhorn(precision));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalFunction>::sharpenKwekMehlhorn(size_t /*precision*/) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::minimum(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Min(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::minimum(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.MinRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::minimum(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.MinRF(other.sylvanMtbdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::maximum(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Max(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::maximum(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.MaxRN(other.sylvanMtbdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::maximum(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.MaxRF(other.sylvanMtbdd));
}

template<typename ValueType>
template<typename TargetValueType>
InternalAdd<DdType::Sylvan, TargetValueType> InternalAdd<DdType::Sylvan, ValueType>::toValueType() const {
    if (std::is_same<TargetValueType, ValueType>::value) {
        return *this;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot convert this ADD to the target type.");
}

template<>
template<>
InternalAdd<DdType::Sylvan, double> InternalAdd<DdType::Sylvan, storm::RationalNumber>::toValueType() const {
    return InternalAdd<DdType::Sylvan, double>(ddManager, this->sylvanMtbdd.ToDoubleRN());
}

template<>
template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, double>::toValueType() const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.ToRationalNumber());
}

template<>
template<>
InternalAdd<DdType::Sylvan, double> InternalAdd<DdType::Sylvan, storm::RationalFunction>::toValueType() const {
    return InternalAdd<DdType::Sylvan, double>(ddManager, this->sylvanMtbdd.ToDoubleRF());
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::sumAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.AbstractPlus(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::sumAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.AbstractPlusRN(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::sumAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.AbstractPlusRF(cube.sylvanBdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::minAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.AbstractMin(cube.sylvanBdd));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::minAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.AbstractMinRepresentative(cube.sylvanBdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::minAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.AbstractMinRepresentativeRN(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::minAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.AbstractMinRN(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::minAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.AbstractMinRF(cube.sylvanBdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::maxAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.AbstractMax(cube.sylvanBdd));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::maxAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.AbstractMaxRepresentative(cube.sylvanBdd));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::maxAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.AbstractMaxRepresentativeRN(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::maxAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager, this->sylvanMtbdd.AbstractMaxRN(cube.sylvanBdd));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::maxAbstract(
    InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager, this->sylvanMtbdd.AbstractMaxRF(cube.sylvanBdd));
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::equalModuloPrecision(InternalAdd<DdType::Sylvan, ValueType> const& other, ValueType const& precision,
                                                                  bool relative) const {
    if (relative) {
        return this->sylvanMtbdd.EqualNormRel(other.sylvanMtbdd, precision);
    } else {
        return this->sylvanMtbdd.EqualNorm(other.sylvanMtbdd, precision);
    }
}

template<>
bool InternalAdd<DdType::Sylvan, storm::RationalNumber>::equalModuloPrecision(InternalAdd<DdType::Sylvan, storm::RationalNumber> const& other,
                                                                              storm::RationalNumber const& precision, bool relative) const {
    if (relative) {
        return this->sylvanMtbdd.EqualNormRelRN(other.sylvanMtbdd, precision);
    } else {
        return this->sylvanMtbdd.EqualNormRN(other.sylvanMtbdd, precision);
    }
}

template<>
bool InternalAdd<DdType::Sylvan, storm::RationalFunction>::equalModuloPrecision(InternalAdd<DdType::Sylvan, storm::RationalFunction> const& other,
                                                                                storm::RationalFunction const& precision, bool relative) const {
    if (relative) {
        return this->sylvanMtbdd.EqualNormRelRF(other.sylvanMtbdd, precision);
    } else {
        return this->sylvanMtbdd.EqualNormRF(other.sylvanMtbdd, precision);
    }
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::swapVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                                             std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    std::vector<uint32_t> fromIndices;
    std::vector<uint32_t> toIndices;
    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        fromIndices.push_back(it1->getIndex());
        fromIndices.push_back(it2->getIndex());
        toIndices.push_back(it2->getIndex());
        toIndices.push_back(it1->getIndex());
    }
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Permute(fromIndices, toIndices));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::permuteVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                                                std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    std::vector<uint32_t> fromIndices;
    std::vector<uint32_t> toIndices;
    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        fromIndices.push_back(it1->getIndex());
        toIndices.push_back(it2->getIndex());
    }
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.Permute(fromIndices, toIndices));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::multiplyMatrix(
    InternalAdd<DdType::Sylvan, ValueType> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanMtbdd.AndExists(otherMatrix.sylvanMtbdd, summationVariables.getSylvanBdd()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::multiplyMatrix(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(ddManager,
                                                                this->sylvanMtbdd.AndExistsRF(otherMatrix.sylvanMtbdd, summationVariables.getSylvanBdd()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::multiplyMatrix(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(ddManager,
                                                              this->sylvanMtbdd.AndExistsRN(otherMatrix.sylvanMtbdd, summationVariables.getSylvanBdd()));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::multiplyMatrix(
    InternalBdd<DdType::Sylvan> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, ValueType>(
        ddManager, this->sylvanMtbdd.AndExists(sylvan::Bdd(otherMatrix.getSylvanBdd().GetBDD()), summationVariables.getSylvanBdd()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalAdd<DdType::Sylvan, storm::RationalFunction>::multiplyMatrix(
    InternalBdd<DdType::Sylvan> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(
        ddManager, this->sylvanMtbdd.AndExistsRF(sylvan::Bdd(otherMatrix.getSylvanBdd().GetBDD()), summationVariables.getSylvanBdd()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, storm::RationalNumber>::multiplyMatrix(
    InternalBdd<DdType::Sylvan> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    InternalBdd<DdType::Sylvan> summationVariables = ddManager->getBddOne();
    for (auto const& ddVariable : summationDdVariables) {
        summationVariables &= ddVariable;
    }

    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(
        ddManager, this->sylvanMtbdd.AndExistsRN(sylvan::Bdd(otherMatrix.getSylvanBdd().GetBDD()), summationVariables.getSylvanBdd()));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greater(ValueType const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddStrictThreshold(value));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::greater(storm::RationalNumber const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddStrictThresholdRN(value));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalFunction>::greater(storm::RationalFunction const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddStrictThresholdRF(value));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greaterOrEqual(ValueType const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddThreshold(value));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalNumber>::greaterOrEqual(storm::RationalNumber const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddThresholdRN(value));
}

template<>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, storm::RationalFunction>::greaterOrEqual(storm::RationalFunction const& value) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.BddThresholdRF(value));
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::less(ValueType const& value) const {
    return !this->greaterOrEqual(value);
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::lessOrEqual(ValueType const& value) const {
    return !this->greater(value);
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::notZero() const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanMtbdd.NotZero());
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::constrain(InternalAdd<DdType::Sylvan, ValueType> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Operation (constrain) not yet implemented.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::restrict(InternalAdd<DdType::Sylvan, ValueType> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Operation (restrict) not yet implemented.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::getSupport() const {
    return InternalBdd<DdType::Sylvan>(ddManager, sylvan::Bdd(static_cast<BDD>(this->sylvanMtbdd.Support().GetMTBDD())));
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    if (numberOfDdVariables == 0) {
        return 0;
    }
    return static_cast<uint_fast64_t>(this->sylvanMtbdd.NonZeroCount(numberOfDdVariables));
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getLeafCount() const {
    return static_cast<uint_fast64_t>(this->sylvanMtbdd.CountLeaves());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getNodeCount() const {
    return static_cast<uint_fast64_t>(this->sylvanMtbdd.NodeCount());
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getMin() const {
    return getValue(this->sylvanMtbdd.Minimum().GetMTBDD());
}

template<>
storm::RationalNumber InternalAdd<DdType::Sylvan, storm::RationalNumber>::getMin() const {
    return getValue(this->sylvanMtbdd.MinimumRN().GetMTBDD());
}

template<>
storm::RationalFunction InternalAdd<DdType::Sylvan, storm::RationalFunction>::getMin() const {
    return getValue(this->sylvanMtbdd.MinimumRF().GetMTBDD());
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getMax() const {
    return getValue(this->sylvanMtbdd.Maximum().GetMTBDD());
}

template<>
storm::RationalNumber InternalAdd<DdType::Sylvan, storm::RationalNumber>::getMax() const {
    return getValue(this->sylvanMtbdd.MaximumRN().GetMTBDD());
}

template<>
storm::RationalFunction InternalAdd<DdType::Sylvan, storm::RationalFunction>::getMax() const {
    return getValue(this->sylvanMtbdd.MaximumRF().GetMTBDD());
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getValue() const {
    return getValue(this->sylvanMtbdd.GetMTBDD());
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isOne() const {
    return *this == ddManager->getAddOne<ValueType>();
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isZero() const {
    return *this == ddManager->getAddZero<ValueType>();
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isConstant() const {
    return this->sylvanMtbdd.isTerminal();
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getIndex() const {
    return static_cast<uint_fast64_t>(this->sylvanMtbdd.TopVar());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getLevel() const {
    return this->getIndex();
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::exportToDot(std::string const& filename, std::vector<std::string> const&, bool) const {
    // Open the file, dump the DD and close it again.
    FILE* filePointer = fopen(filename.c_str(), "a+");
    // fopen returns a nullptr on failure
    if (filePointer == nullptr) {
        STORM_LOG_ERROR("Failure to open file: " << filename);
    } else {
        this->sylvanMtbdd.PrintDot(filePointer);
        fclose(filePointer);
    }
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::exportToText(std::string const& filename) const {
    // Open the file, dump the DD and close it again.
    FILE* filePointer = fopen(filename.c_str(), "a+");
    // fopen returns a nullptr on failure
    if (filePointer == nullptr) {
        STORM_LOG_ERROR("Failure to open file: " << filename);
    } else {
        this->sylvanMtbdd.PrintText(filePointer);
        fclose(filePointer);
    }
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::begin(DdManager<DdType::Sylvan> const& fullDdManager,
                                                                                     InternalBdd<DdType::Sylvan> const& variableCube,
                                                                                     uint_fast64_t numberOfDdVariables,
                                                                                     std::set<storm::expressions::Variable> const& metaVariables,
                                                                                     bool enumerateDontCareMetaVariables) const {
    return AddIterator<DdType::Sylvan, ValueType>::createBeginIterator(fullDdManager, this->getSylvanMtbdd(), variableCube.getSylvanBdd(), numberOfDdVariables,
                                                                       &metaVariables, enumerateDontCareMetaVariables);
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::end(DdManager<DdType::Sylvan> const& fullDdManager) const {
    return AddIterator<DdType::Sylvan, ValueType>::createEndIterator(fullDdManager);
}

template<typename ValueType>
Odd InternalAdd<DdType::Sylvan, ValueType>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    // Prepare a unique table for each level that keeps the constructed ODD nodes unique.
    std::vector<std::unordered_map<BDD, std::shared_ptr<Odd>>> uniqueTableForLevels(ddVariableIndices.size() + 1);

    // Now construct the ODD structure from the ADD.
    std::shared_ptr<Odd> rootOdd =
        createOddRec(mtbdd_regular(this->getSylvanMtbdd().GetMTBDD()), 0, ddVariableIndices.size(), ddVariableIndices, uniqueTableForLevels);

    // Return a copy of the root node to remove the shared_ptr encapsulation.
    return Odd(*rootOdd);
}

template<typename ValueType>
std::shared_ptr<Odd> InternalAdd<DdType::Sylvan, ValueType>::createOddRec(BDD dd, uint_fast64_t currentLevel, uint_fast64_t maxLevel,
                                                                          std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                          std::vector<std::unordered_map<BDD, std::shared_ptr<Odd>>>& uniqueTableForLevels) {
    // Check whether the ODD for this node has already been computed (for this level) and if so, return this instead.
    auto const& iterator = uniqueTableForLevels[currentLevel].find(dd);
    if (iterator != uniqueTableForLevels[currentLevel].end()) {
        return iterator->second;
    } else {
        // Otherwise, we need to recursively compute the ODD.

        // If we are already past the maximal level that is to be considered, we can simply create an Odd without
        // successors
        if (currentLevel == maxLevel) {
            uint_fast64_t elseOffset = 0;
            uint_fast64_t thenOffset = 0;

            STORM_LOG_ASSERT(mtbdd_isleaf(dd), "Expected leaf at last level.");

            // If the DD is not the zero leaf, then the then-offset is 1.
            if (!mtbdd_iszero(dd)) {
                thenOffset = 1;
            }

            auto oddNode = std::make_shared<Odd>(nullptr, elseOffset, nullptr, thenOffset);
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else if (mtbdd_isleaf(dd) || ddVariableIndices[currentLevel] < mtbdd_getvar(dd)) {
            // If we skipped the level in the DD, we compute the ODD just for the else-successor and use the same
            // node for the then-successor as well.
            std::shared_ptr<Odd> elseNode = createOddRec(dd, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = elseNode;
            auto oddNode = std::make_shared<Odd>(elseNode, elseNode->getElseOffset() + elseNode->getThenOffset(), thenNode,
                                                 thenNode->getElseOffset() + thenNode->getThenOffset());
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else {
            // Otherwise, we compute the ODDs for both the then- and else successors.
            std::shared_ptr<Odd> elseNode = createOddRec(mtbdd_regular(mtbdd_getlow(dd)), currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = createOddRec(mtbdd_regular(mtbdd_gethigh(dd)), currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);

            uint_fast64_t totalElseOffset = elseNode->getElseOffset() + elseNode->getThenOffset();
            uint_fast64_t totalThenOffset = thenNode->getElseOffset() + thenNode->getThenOffset();

            auto oddNode = std::make_shared<Odd>(elseNode, totalElseOffset, thenNode, totalThenOffset);
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        }
    }
}

template<typename ValueType>
InternalDdManager<DdType::Sylvan> const& InternalAdd<DdType::Sylvan, ValueType>::getInternalDdManager() const {
    return *ddManager;
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                       std::vector<ValueType>& targetVector,
                                                                       std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    forEachRec(this->getSylvanMtbdd().GetMTBDD(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices,
               [&function, &targetVector](uint64_t const& offset, ValueType const& value) { targetVector[offset] = function(targetVector[offset], value); });
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                       std::vector<uint_fast64_t> const& offsets, std::vector<ValueType>& targetVector,
                                                                       std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    forEachRec(this->getSylvanMtbdd().GetMTBDD(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices,
               [&function, &targetVector, &offsets](uint64_t const& offset, ValueType const& value) {
                   ValueType& targetValue = targetVector[offsets[offset]];
                   targetValue = function(targetValue, value);
               });
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::forEach(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    forEachRec(this->getSylvanMtbdd().GetMTBDD(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices, function);
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::forEachRec(MTBDD dd, uint_fast64_t currentLevel, uint_fast64_t maxLevel, uint_fast64_t currentOffset,
                                                        Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                        std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    // For the empty DD, we do not need to add any entries.
    if (mtbdd_isleaf(dd) && mtbdd_iszero(dd)) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentLevel == maxLevel) {
        function(currentOffset, getValue(dd));
    } else if (mtbdd_isleaf(dd) || ddVariableIndices[currentLevel] < mtbdd_getvar(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        forEachRec(dd, currentLevel + 1, maxLevel, currentOffset, odd.getElseSuccessor(), ddVariableIndices, function);
        forEachRec(dd, currentLevel + 1, maxLevel, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), ddVariableIndices, function);
    } else {
        // Otherwise, we simply recursively call the function for both (different) cases.
        MTBDD thenNode = mtbdd_gethigh(dd);
        MTBDD elseNode = mtbdd_getlow(dd);

        forEachRec(elseNode, currentLevel + 1, maxLevel, currentOffset, odd.getElseSuccessor(), ddVariableIndices, function);
        forEachRec(thenNode, currentLevel + 1, maxLevel, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), ddVariableIndices, function);
    }
}

template<typename ValueType>
std::vector<uint64_t> InternalAdd<DdType::Sylvan, ValueType>::decodeGroupLabels(std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                                storm::storage::BitVector const& ddLabelVariableIndices) const {
    std::vector<uint64_t> result;
    decodeGroupLabelsRec(mtbdd_regular(this->getSylvanMtbdd().GetMTBDD()), result, ddGroupVariableIndices, ddLabelVariableIndices, 0,
                         ddGroupVariableIndices.size(), 0);
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::decodeGroupLabelsRec(MTBDD dd, std::vector<uint64_t>& labels,
                                                                  std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                  storm::storage::BitVector const& ddLabelVariableIndices, uint_fast64_t currentLevel,
                                                                  uint_fast64_t maxLevel, uint64_t label) const {
    // For the empty DD, we do not need to create a group.
    if (mtbdd_isleaf(dd) && mtbdd_iszero(dd)) {
        return;
    }

    if (currentLevel == maxLevel) {
        labels.push_back(label);
    } else {
        uint64_t elseLabel = label;
        uint64_t thenLabel = label;

        if (ddLabelVariableIndices.get(currentLevel)) {
            elseLabel <<= 1;
            thenLabel = (thenLabel << 1) | 1;
        }

        if (mtbdd_isleaf(dd) || ddGroupVariableIndices[currentLevel] < mtbdd_getvar(dd)) {
            decodeGroupLabelsRec(dd, labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, elseLabel);
            decodeGroupLabelsRec(dd, labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, thenLabel);
        } else {
            // Otherwise, we compute the ODDs for both the then- and else successors.
            MTBDD thenDdNode = mtbdd_gethigh(dd);
            MTBDD elseDdNode = mtbdd_getlow(dd);

            decodeGroupLabelsRec(mtbdd_regular(elseDdNode), labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, elseLabel);
            decodeGroupLabelsRec(mtbdd_regular(thenDdNode), labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, thenLabel);
        }
    }
}

template<typename ValueType>
std::vector<InternalAdd<DdType::Sylvan, ValueType>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<InternalAdd<DdType::Sylvan, ValueType>> result;
    splitIntoGroupsRec(mtbdd_regular(this->getSylvanMtbdd().GetMTBDD()), mtbdd_hascomp(this->getSylvanMtbdd().GetMTBDD()), result, ddGroupVariableIndices, 0,
                       ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
std::vector<std::pair<InternalAdd<DdType::Sylvan, ValueType>, InternalAdd<DdType::Sylvan, ValueType>>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    InternalAdd<DdType::Sylvan, ValueType> vector, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<std::pair<InternalAdd<DdType::Sylvan, ValueType>, InternalAdd<DdType::Sylvan, ValueType>>> result;
    splitIntoGroupsRec(mtbdd_regular(this->getSylvanMtbdd().GetMTBDD()), mtbdd_hascomp(this->getSylvanMtbdd().GetMTBDD()),
                       mtbdd_regular(vector.getSylvanMtbdd().GetMTBDD()), mtbdd_hascomp(vector.getSylvanMtbdd().GetMTBDD()), result, ddGroupVariableIndices, 0,
                       ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
std::vector<std::vector<InternalAdd<DdType::Sylvan, ValueType>>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    std::vector<InternalAdd<DdType::Sylvan, ValueType>> const& vectors, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<std::vector<InternalAdd<DdType::Sylvan, ValueType>>> result;
    std::vector<MTBDD> dds;
    storm::storage::BitVector negatedDds(vectors.size() + 1);
    for (auto const& vector : vectors) {
        negatedDds.set(dds.size(), mtbdd_hascomp(vector.getSylvanMtbdd().GetMTBDD()));
        dds.push_back(mtbdd_regular(vector.getSylvanMtbdd().GetMTBDD()));
    }
    dds.push_back(this->getSylvanMtbdd().GetMTBDD());
    negatedDds.set(vectors.size(), mtbdd_hascomp(this->getSylvanMtbdd().GetMTBDD()));

    splitIntoGroupsRec(dds, negatedDds, result, ddGroupVariableIndices, 0, ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroupsRec(MTBDD dd, bool negated, std::vector<InternalAdd<DdType::Sylvan, ValueType>>& groups,
                                                                std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                                uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (mtbdd_isleaf(dd) && mtbdd_iszero(dd)) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(InternalAdd<DdType::Sylvan, ValueType>(ddManager, negated ? sylvan::Mtbdd(dd).Negate() : sylvan::Mtbdd(dd)));
    } else if (mtbdd_isleaf(dd) || ddGroupVariableIndices[currentLevel] < mtbdd_getvar(dd)) {
        splitIntoGroupsRec(dd, negated, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(dd, negated, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        MTBDD thenDdNode = mtbdd_gethigh(dd);
        MTBDD elseDdNode = mtbdd_getlow(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = mtbdd_hascomp(elseDdNode) ^ negated;
        bool thenComplemented = mtbdd_hascomp(thenDdNode) ^ negated;

        // FIXME: We first traverse the else successor (unlike other variants of this method).
        // Otherwise, the GameBasedMdpModelCheckerTest would not terminate. See github issue #64
        splitIntoGroupsRec(mtbdd_regular(elseDdNode), elseComplemented, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(mtbdd_regular(thenDdNode), thenComplemented, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroupsRec(
    MTBDD dd1, bool negated1, MTBDD dd2, bool negated2,
    std::vector<std::pair<InternalAdd<DdType::Sylvan, ValueType>, InternalAdd<DdType::Sylvan, ValueType>>>& groups,
    std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel, uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (mtbdd_isleaf(dd1) && mtbdd_isleaf(dd2) && mtbdd_iszero(dd1) && mtbdd_iszero(dd2)) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(std::make_pair(InternalAdd<DdType::Sylvan, ValueType>(ddManager, negated1 ? sylvan::Mtbdd(dd1).Negate() : sylvan::Mtbdd(dd1)),
                                        InternalAdd<DdType::Sylvan, ValueType>(ddManager, negated2 ? sylvan::Mtbdd(dd2).Negate() : sylvan::Mtbdd(dd2))));
    } else if (mtbdd_isleaf(dd1) || ddGroupVariableIndices[currentLevel] < mtbdd_getvar(dd1)) {
        if (mtbdd_isleaf(dd2) || ddGroupVariableIndices[currentLevel] < mtbdd_getvar(dd2)) {
            splitIntoGroupsRec(dd1, negated1, dd2, negated2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
            splitIntoGroupsRec(dd1, negated1, dd2, negated2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        } else {
            MTBDD dd2ThenNode = mtbdd_gethigh(dd2);
            MTBDD dd2ElseNode = mtbdd_getlow(dd2);

            // Determine whether we have to evaluate the successors as if they were complemented.
            bool elseComplemented = mtbdd_hascomp(dd2ElseNode) ^ negated2;
            bool thenComplemented = mtbdd_hascomp(dd2ThenNode) ^ negated2;

            splitIntoGroupsRec(dd1, negated1, mtbdd_regular(dd2ThenNode), thenComplemented, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
            splitIntoGroupsRec(dd1, negated1, mtbdd_regular(dd2ElseNode), elseComplemented, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        }
    } else if (mtbdd_isleaf(dd2) || ddGroupVariableIndices[currentLevel] < mtbdd_getvar(dd2)) {
        MTBDD dd1ThenNode = mtbdd_gethigh(dd1);
        MTBDD dd1ElseNode = mtbdd_getlow(dd1);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = mtbdd_hascomp(dd1ElseNode) ^ negated1;
        bool thenComplemented = mtbdd_hascomp(dd1ThenNode) ^ negated1;

        splitIntoGroupsRec(mtbdd_regular(dd1ThenNode), thenComplemented, dd2, negated2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(mtbdd_regular(dd1ElseNode), elseComplemented, dd2, negated2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        MTBDD dd1ThenNode = mtbdd_gethigh(dd1);
        MTBDD dd1ElseNode = mtbdd_getlow(dd1);
        MTBDD dd2ThenNode = mtbdd_gethigh(dd2);
        MTBDD dd2ElseNode = mtbdd_getlow(dd2);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool dd1ElseComplemented = mtbdd_hascomp(dd1ElseNode) ^ negated1;
        bool dd1ThenComplemented = mtbdd_hascomp(dd1ThenNode) ^ negated1;
        bool dd2ElseComplemented = mtbdd_hascomp(dd2ElseNode) ^ negated2;
        bool dd2ThenComplemented = mtbdd_hascomp(dd2ThenNode) ^ negated2;

        splitIntoGroupsRec(mtbdd_regular(dd1ThenNode), dd1ThenComplemented, mtbdd_regular(dd2ThenNode), dd2ThenComplemented, groups, ddGroupVariableIndices,
                           currentLevel + 1, maxLevel);
        splitIntoGroupsRec(mtbdd_regular(dd1ElseNode), dd1ElseComplemented, mtbdd_regular(dd2ElseNode), dd2ElseComplemented, groups, ddGroupVariableIndices,
                           currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroupsRec(std::vector<MTBDD> const& dds, storm::storage::BitVector const& negatedDds,
                                                                std::vector<std::vector<InternalAdd<DdType::Sylvan, ValueType>>>& groups,
                                                                std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                                uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    {
        bool emptyDd = true;
        for (auto const& dd : dds) {
            if (!(mtbdd_isleaf(dd) && mtbdd_iszero(dd))) {
                emptyDd = false;
                break;
            }
        }
        if (emptyDd) {
            return;
        }
    }

    if (currentLevel == maxLevel) {
        std::vector<InternalAdd<DdType::Sylvan, ValueType>> newGroup;
        for (uint64_t ddIndex = 0; ddIndex < dds.size(); ++ddIndex) {
            newGroup.emplace_back(ddManager, negatedDds.get(ddIndex) ? sylvan::Mtbdd(dds[ddIndex]).Negate() : sylvan::Mtbdd(dds[ddIndex]));
        }
        groups.push_back(std::move(newGroup));
    } else {
        std::vector<MTBDD> thenSubDds(dds), elseSubDds(dds);
        storm::storage::BitVector thenNegatedSubDds(negatedDds), elseNegatedSubDds(negatedDds);
        for (uint64_t ddIndex = 0; ddIndex < dds.size(); ++ddIndex) {
            auto const& dd = dds[ddIndex];
            if (!mtbdd_isleaf(dd) && ddGroupVariableIndices[currentLevel] == mtbdd_getvar(dd)) {
                MTBDD ddThenNode = mtbdd_gethigh(dd);
                MTBDD ddElseNode = mtbdd_getlow(dd);
                thenSubDds[ddIndex] = mtbdd_regular(ddThenNode);
                elseSubDds[ddIndex] = mtbdd_regular(ddElseNode);
                // Determine whether we have to evaluate the successors as if they were complemented.
                thenNegatedSubDds.set(ddIndex, mtbdd_hascomp(ddThenNode) ^ negatedDds.get(ddIndex));
                elseNegatedSubDds.set(ddIndex, mtbdd_hascomp(ddElseNode) ^ negatedDds.get(ddIndex));
            }
        }
        splitIntoGroupsRec(thenSubDds, thenNegatedSubDds, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(elseSubDds, elseNegatedSubDds, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::toMatrixComponents(std::vector<uint_fast64_t> const& rowGroupIndices, std::vector<uint_fast64_t>& rowIndications,
                                                                std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                                Odd const& rowOdd, Odd const& columnOdd, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                                std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool writeValues) const {
    return toMatrixComponentsRec(mtbdd_regular(this->getSylvanMtbdd().GetMTBDD()), mtbdd_hascomp(this->getSylvanMtbdd().GetMTBDD()), rowGroupIndices,
                                 rowIndications, columnsAndValues, rowOdd, columnOdd, 0, 0, ddRowVariableIndices.size() + ddColumnVariableIndices.size(), 0, 0,
                                 ddRowVariableIndices, ddColumnVariableIndices, writeValues);
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::toMatrixComponentsRec(MTBDD dd, bool negated, std::vector<uint_fast64_t> const& rowGroupOffsets,
                                                                   std::vector<uint_fast64_t>& rowIndications,
                                                                   std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                                   Odd const& rowOdd, Odd const& columnOdd, uint_fast64_t currentRowLevel,
                                                                   uint_fast64_t currentColumnLevel, uint_fast64_t maxLevel, uint_fast64_t currentRowOffset,
                                                                   uint_fast64_t currentColumnOffset, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                                   std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool generateValues) const {
    // For the empty DD, we do not need to add any entries.
    if (mtbdd_isleaf(dd) && mtbdd_iszero(dd)) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentRowLevel + currentColumnLevel == maxLevel) {
        if (generateValues) {
            columnsAndValues[rowIndications[rowGroupOffsets[currentRowOffset]]] =
                storm::storage::MatrixEntry<uint_fast64_t, ValueType>(currentColumnOffset, negated ? -getValue(dd) : getValue(dd));
        }
        ++rowIndications[rowGroupOffsets[currentRowOffset]];
    } else {
        MTBDD elseElse;
        MTBDD elseThen;
        MTBDD thenElse;
        MTBDD thenThen;

        if (mtbdd_isleaf(dd) || ddColumnVariableIndices[currentColumnLevel] < mtbdd_getvar(dd)) {
            elseElse = elseThen = thenElse = thenThen = dd;
        } else if (ddRowVariableIndices[currentColumnLevel] < mtbdd_getvar(dd)) {
            elseElse = thenElse = mtbdd_getlow(dd);
            elseThen = thenThen = mtbdd_gethigh(dd);
        } else {
            MTBDD elseNode = mtbdd_getlow(dd);
            if (mtbdd_isleaf(elseNode) || ddColumnVariableIndices[currentColumnLevel] < mtbdd_getvar(elseNode)) {
                elseElse = elseThen = elseNode;
            } else {
                elseElse = mtbdd_getlow(elseNode);
                elseThen = mtbdd_gethigh(elseNode);
            }

            MTBDD thenNode = mtbdd_gethigh(dd);
            if (mtbdd_isleaf(thenNode) || ddColumnVariableIndices[currentColumnLevel] < mtbdd_getvar(thenNode)) {
                thenElse = thenThen = thenNode;
            } else {
                thenElse = mtbdd_getlow(thenNode);
                thenThen = mtbdd_gethigh(thenNode);
            }
        }

        // Visit else-else.
        toMatrixComponentsRec(mtbdd_regular(elseElse), mtbdd_hascomp(elseElse) ^ negated, rowGroupOffsets, rowIndications, columnsAndValues,
                              rowOdd.getElseSuccessor(), columnOdd.getElseSuccessor(), currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset,
                              currentColumnOffset, ddRowVariableIndices, ddColumnVariableIndices, generateValues);
        // Visit else-then.
        toMatrixComponentsRec(mtbdd_regular(elseThen), mtbdd_hascomp(elseThen) ^ negated, rowGroupOffsets, rowIndications, columnsAndValues,
                              rowOdd.getElseSuccessor(), columnOdd.getThenSuccessor(), currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset,
                              currentColumnOffset + columnOdd.getElseOffset(), ddRowVariableIndices, ddColumnVariableIndices, generateValues);
        // Visit then-else.
        toMatrixComponentsRec(mtbdd_regular(thenElse), mtbdd_hascomp(thenElse) ^ negated, rowGroupOffsets, rowIndications, columnsAndValues,
                              rowOdd.getThenSuccessor(), columnOdd.getElseSuccessor(), currentRowLevel + 1, currentColumnLevel + 1, maxLevel,
                              currentRowOffset + rowOdd.getElseOffset(), currentColumnOffset, ddRowVariableIndices, ddColumnVariableIndices, generateValues);
        // Visit then-then.
        toMatrixComponentsRec(mtbdd_regular(thenThen), mtbdd_hascomp(thenThen) ^ negated, rowGroupOffsets, rowIndications, columnsAndValues,
                              rowOdd.getThenSuccessor(), columnOdd.getThenSuccessor(), currentRowLevel + 1, currentColumnLevel + 1, maxLevel,
                              currentRowOffset + rowOdd.getElseOffset(), currentColumnOffset + columnOdd.getElseOffset(), ddRowVariableIndices,
                              ddColumnVariableIndices, generateValues);
    }
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::fromVector(InternalDdManager<DdType::Sylvan> const* ddManager,
                                                                                          std::vector<ValueType> const& values, storm::dd::Odd const& odd,
                                                                                          std::vector<uint_fast64_t> const& ddVariableIndices) {
    uint_fast64_t offset = 0;
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, sylvan::Mtbdd(fromVectorRec(offset, 0, ddVariableIndices.size(), values, odd, ddVariableIndices)));
}

template<typename ValueType>
MTBDD InternalAdd<DdType::Sylvan, ValueType>::fromVectorRec(uint_fast64_t& currentOffset, uint_fast64_t currentLevel, uint_fast64_t maxLevel,
                                                            std::vector<ValueType> const& values, Odd const& odd,
                                                            std::vector<uint_fast64_t> const& ddVariableIndices) {
    if (currentLevel == maxLevel) {
        // If we are in a terminal node of the ODD, we need to check whether the then-offset of the ODD is one
        // (meaning the encoding is a valid one) or zero (meaning the encoding is not valid). Consequently, we
        // need to copy the next value of the vector iff the then-offset is greater than zero.
        if (odd.getThenOffset() > 0) {
            return getLeaf(values[currentOffset++]);
        } else {
            return getLeaf(storm::utility::zero<ValueType>());
        }
    } else {
        // If the total offset is zero, we can just return the constant zero DD.
        if (odd.getThenOffset() + odd.getElseOffset() == 0) {
            return getLeaf(storm::utility::zero<ValueType>());
        }

        // Determine the new else-successor.
        MTBDD elseSuccessor;
        if (odd.getElseOffset() > 0) {
            elseSuccessor = fromVectorRec(currentOffset, currentLevel + 1, maxLevel, values, odd.getElseSuccessor(), ddVariableIndices);
        } else {
            elseSuccessor = getLeaf(storm::utility::zero<ValueType>());
        }
        mtbdd_refs_push(elseSuccessor);

        // Determine the new then-successor.
        MTBDD thenSuccessor;
        if (odd.getThenOffset() > 0) {
            thenSuccessor = fromVectorRec(currentOffset, currentLevel + 1, maxLevel, values, odd.getThenSuccessor(), ddVariableIndices);
        } else {
            thenSuccessor = getLeaf(storm::utility::zero<ValueType>());
        }
        mtbdd_refs_push(thenSuccessor);

        // Create a node representing ITE(currentVar, thenSuccessor, elseSuccessor);
        MTBDD currentVar = mtbdd_makenode(ddVariableIndices[currentLevel], mtbdd_false, mtbdd_true);
        mtbdd_refs_push(thenSuccessor);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
        MTBDD result = mtbdd_ite(currentVar, thenSuccessor, elseSuccessor);
#pragma clang diagnostic pop

        // Dispose of the intermediate results
        mtbdd_refs_pop(3);

        return result;
    }
}

template<typename ValueType>
MTBDD InternalAdd<DdType::Sylvan, ValueType>::getLeaf(double value) {
    return mtbdd_double(value);
}

template<typename ValueType>
MTBDD InternalAdd<DdType::Sylvan, ValueType>::getLeaf(uint_fast64_t value) {
    return mtbdd_int64(value);
}

template<typename ValueType>
MTBDD InternalAdd<DdType::Sylvan, ValueType>::getLeaf(storm::RationalFunction const& value) {
    storm_rational_function_ptr ptr = (storm_rational_function_ptr)(&value);
    return mtbdd_storm_rational_function(ptr);
}

template<typename ValueType>
MTBDD InternalAdd<DdType::Sylvan, ValueType>::getLeaf(storm::RationalNumber const& value) {
    storm_rational_function_ptr ptr = (storm_rational_number_ptr)(&value);
    return mtbdd_storm_rational_number(ptr);
}

template<typename ValueType>
sylvan::Mtbdd InternalAdd<DdType::Sylvan, ValueType>::getSylvanMtbdd() const {
    return sylvanMtbdd;
}

template<typename ValueType>
std::string InternalAdd<DdType::Sylvan, ValueType>::getStringId() const {
    std::stringstream ss;
    ss << this->getSylvanMtbdd().GetMTBDD();
    return ss.str();
}

#else
template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>::InternalAdd() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::operator==(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::operator!=(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator+(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator+=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator*(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator*=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator-(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator-=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::operator/(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType>& InternalAdd<DdType::Sylvan, ValueType>::operator/=(InternalAdd<DdType::Sylvan, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::equals(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::notEquals(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::less(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::lessOrEqual(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greater(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greaterOrEqual(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::pow(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::mod(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::logxy(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::floor() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::ceil() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, ValueType>::sharpenKwekMehlhorn(size_t precision) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::minimum(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::maximum(InternalAdd<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
template<typename TargetValueType>
InternalAdd<DdType::Sylvan, TargetValueType> InternalAdd<DdType::Sylvan, ValueType>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<>
template<>
InternalAdd<DdType::Sylvan, double> InternalAdd<DdType::Sylvan, storm::RationalNumber>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot convert this ADD to the target type.");
}

template<>
template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalAdd<DdType::Sylvan, double>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot convert this ADD to the target type.");
}

template<>
template<>
InternalAdd<DdType::Sylvan, double> InternalAdd<DdType::Sylvan, storm::RationalFunction>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot convert this ADD to the target type.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::sumAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::minAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::minAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::maxAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::maxAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::equalModuloPrecision(InternalAdd<DdType::Sylvan, ValueType> const& other, ValueType const& precision,
                                                                  bool relative) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::swapVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                                             std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::permuteVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                                                std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::multiplyMatrix(
    InternalAdd<DdType::Sylvan, ValueType> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::multiplyMatrix(
    InternalBdd<DdType::Sylvan> const& otherMatrix, std::vector<InternalBdd<DdType::Sylvan>> const& summationDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greater(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::greaterOrEqual(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::less(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::lessOrEqual(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::notZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::constrain(InternalAdd<DdType::Sylvan, ValueType> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::restrict(InternalAdd<DdType::Sylvan, ValueType> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalBdd<DdType::Sylvan> InternalAdd<DdType::Sylvan, ValueType>::getSupport() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getLeafCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getNodeCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getMin() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getMax() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::Sylvan, ValueType>::getValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool InternalAdd<DdType::Sylvan, ValueType>::isConstant() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getIndex() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::Sylvan, ValueType>::getLevel() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::exportToDot(std::string const& filename, std::vector<std::string> const&, bool) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::exportToText(std::string const& filename) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::begin(DdManager<DdType::Sylvan> const& fullDdManager,
                                                                                     InternalBdd<DdType::Sylvan> const& variableCube,
                                                                                     uint_fast64_t numberOfDdVariables,
                                                                                     std::set<storm::expressions::Variable> const& metaVariables,
                                                                                     bool enumerateDontCareMetaVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::end(DdManager<DdType::Sylvan> const& fullDdManager) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
Odd InternalAdd<DdType::Sylvan, ValueType>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalDdManager<DdType::Sylvan> const& InternalAdd<DdType::Sylvan, ValueType>::getInternalDdManager() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                       std::vector<ValueType>& targetVector,
                                                                       std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                       std::vector<uint_fast64_t> const& offsets, std::vector<ValueType>& targetVector,
                                                                       std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::forEach(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::vector<uint64_t> InternalAdd<DdType::Sylvan, ValueType>::decodeGroupLabels(std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                                storm::storage::BitVector const& ddLabelVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::vector<InternalAdd<DdType::Sylvan, ValueType>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::vector<std::pair<InternalAdd<DdType::Sylvan, ValueType>, InternalAdd<DdType::Sylvan, ValueType>>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    InternalAdd<DdType::Sylvan, ValueType> vector, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::vector<std::vector<InternalAdd<DdType::Sylvan, ValueType>>> InternalAdd<DdType::Sylvan, ValueType>::splitIntoGroups(
    std::vector<InternalAdd<DdType::Sylvan, ValueType>> const& vectors, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalAdd<DdType::Sylvan, ValueType>::toMatrixComponents(std::vector<uint_fast64_t> const& rowGroupIndices, std::vector<uint_fast64_t>& rowIndications,
                                                                std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                                Odd const& rowOdd, Odd const& columnOdd, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                                std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool writeValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalAdd<DdType::Sylvan, ValueType>::fromVector(InternalDdManager<DdType::Sylvan> const* ddManager,
                                                                                          std::vector<ValueType> const& values, storm::dd::Odd const& odd,
                                                                                          std::vector<uint_fast64_t> const& ddVariableIndices) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::string InternalAdd<DdType::Sylvan, ValueType>::getStringId() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
#endif

template class InternalAdd<DdType::Sylvan, double>;
template class InternalAdd<DdType::Sylvan, uint_fast64_t>;
template class InternalAdd<DdType::Sylvan, storm::RationalNumber>;
template class InternalAdd<DdType::Sylvan, storm::RationalFunction>;

}  // namespace dd
}  // namespace storm
