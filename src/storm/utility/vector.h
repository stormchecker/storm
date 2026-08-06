#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <functional>
#include <iosfwd>
#include <numeric>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {
namespace vector {

template<typename ValueType>
struct VectorHash {
    size_t operator()(std::vector<ValueType> const& vec) const {
        std::hash<ValueType> hasher;
        std::size_t seed = 0;
        for (ValueType const& element : vec) {
            seed ^= hasher(element) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

/*!
 * Finds the given element in the given vector.
 * If the vector does not contain the element, it is inserted (at the end of vector).
 * Either way, the returned value will be the position of the element inside the vector,
 *
 * @note old indices to other elements remain valid, as the vector will not be sorted.
 *
 * @param vector The vector in which the element is searched and possibly insert
 * @param element The element that will be searched for (or inserted)
 *
 * @return The position at which the element is located
 */
template<class T>
std::size_t findOrInsert(std::vector<T>& vector, T&& element) {
    std::size_t position = std::find(vector.begin(), vector.end(), element) - vector.begin();
    if (position == vector.size()) {
        vector.emplace_back(std::move(element));
    }
    return position;
}

template<typename T>
void setAllValues(std::vector<T>& vec, storm::storage::BitVector const& positions, T const& positiveValue = storm::utility::one<T>(),
                  T const& negativeValue = storm::utility::zero<T>()) {
    if (positions.getNumberOfSetBits() * 2 > positions.size()) {
        vec.resize(positions.size(), positiveValue);
        uint64_t index = positions.getNextUnsetIndex(0);
        while (index < positions.size()) {
            vec[index] = negativeValue;
            index = positions.getNextUnsetIndex(index + 1);
        }
    } else {
        vec.resize(positions.size(), negativeValue);
        for (uint64_t index : positions) {
            vec[index] = positiveValue;
        }
    }
}

/*!
 * Sets the provided values at the provided positions in the given vector.
 *
 * @param vector The vector in which the values are to be set.
 * @param positions The positions at which the values are to be set.
 * @param values The values that are to be set.
 */
template<class T>
void setVectorValues(std::vector<T>& vector, storm::storage::BitVector const& positions, std::vector<T> const& values) {
    STORM_LOG_ASSERT(positions.size() <= vector.size(), "We cannot set positions that have not been initialized");
    STORM_LOG_ASSERT(positions.getNumberOfSetBits() <= values.size(), "The number of selected positions (" << positions.getNumberOfSetBits()
                                                                                                           << ") exceeds the size of the input vector ("
                                                                                                           << values.size() << ").");
    uint_fast64_t oldPosition = 0;
    for (auto position : positions) {
        vector[position] = values[oldPosition++];
    }
}

/*!
 * Sets the provided value at the provided positions in the given vector.
 * Note that the vector must be at least as long.
 *
 * @param vector The vector in which the value is to be set.
 * @param positions The positions at which the value is to be set.
 * @param value The value that is to be set.
 */
template<class T>
void setVectorValues(std::vector<T>& vector, storm::storage::BitVector const& positions, T value) {
    STORM_LOG_ASSERT(positions.size() <= vector.size(), "We cannot set positions that have not been initialized");
    for (auto position : positions) {
        vector[position] = value;
    }
}

template<typename T>
void setNonzeroIndices(std::vector<T> const& vec, storm::storage::BitVector& bv) {
    STORM_LOG_ASSERT(bv.size() == vec.size(), "Bitvector size should match vector size");
    for (uint64_t i = 0; i < vec.size(); ++i) {
        if (!storm::utility::isZero(vec[i])) {
            bv.set(i, true);
        }
    }
}

/*!
 * Iota function as a helper for efficient creating a range in a vector.
 * See also http://stackoverflow.com/questions/11965732/set-stdvectorint-to-a-range
 * @see buildVectorForRange
 */
template<class OutputIterator, class Size, class Assignable>
void iota_n(OutputIterator first, Size n, Assignable value) {
    std::generate_n(first, n, [&value]() { return value++; });
}

/*!
 * Constructs a vector [min, min+1, ...., max-1]
 */
template<typename T>
inline std::vector<T> buildVectorForRange(T min, T max) {
    STORM_LOG_ASSERT(min <= max, "Invalid range.");
    T diff = max - min;
    std::vector<T> v;
    v.reserve(diff);
    iota_n(std::back_inserter(v), diff, min);
    return v;
}

/*!
 * Returns a list of indices such that the first index refers to the highest entry of the given vector,
 *  the second index refers to the entry with the second highest value, ...
 * Example:  v={3,8,4,5} yields res={1,3,2,0}
 */
template<typename T>
std::vector<uint_fast64_t> getSortedIndices(std::vector<T> const& v) {
    std::vector<uint_fast64_t> res = buildVectorForRange<uint_fast64_t>(0, v.size());
    std::sort(res.begin(), res.end(), [&v](uint_fast64_t index1, uint_fast64_t index2) { return v[index1] > v[index2]; });
    return res;
}

/*!
 * Returns true iff every element in the given vector is unique, i.e., there are no i,j with i!=j and v[i]==v[j].
 */
template<typename T>
bool isUnique(std::vector<T> const& v) {
    if (v.size() < 2) {
        return true;
    }
    auto sortedIndices = getSortedIndices(v);
    auto indexIt = sortedIndices.begin();
    T const* previous = &v[*indexIt];
    for (++indexIt; indexIt != sortedIndices.end(); ++indexIt) {
        T const& current = v[*indexIt];
        if (current == *previous) {
            return false;
        }
        previous = &current;
    }
    return true;
}

template<typename T, typename Comparator>
bool compareElementWise(std::vector<T> const& left, std::vector<T> const& right, Comparator comp = std::less<T>()) {
    STORM_LOG_ASSERT(left.size() == right.size(), "Expected that vectors for comparison have equal size");
    return std::equal(left.begin(), left.end(), right.begin(), comp);
}

/*!
 * Selects the elements from a vector at the specified positions and writes them consecutively into another vector.
 * @param vector The vector into which the selected elements are to be written.
 * @param positions The positions at which to select the elements from the values vector.
 * @param values The vector from which to select the elements.
 */
template<class T>
void selectVectorValues(std::vector<T>& vector, storm::storage::BitVector const& positions, std::vector<T> const& values) {
    STORM_LOG_ASSERT(positions.getNumberOfSetBits() <= vector.size(), "The number of selected positions (" << positions.getNumberOfSetBits()
                                                                                                           << ") exceeds the size of the target vector ("
                                                                                                           << vector.size() << ").");
    STORM_LOG_ASSERT(positions.size() == values.size(),
                     "Size mismatch of the positions vector (" << positions.size() << ") and the values vector (" << values.size() << ").");
    auto targetIt = vector.begin();
    for (auto position : positions) {
        *targetIt = values[position];
        ++targetIt;
    }
}

/*!
 * Selects groups of elements from a vector at the specified positions and writes them consecutively into another vector.
 *
 * @param vector The vector into which the selected elements are to be written.
 * @param positions The positions of the groups of elements that are to be selected.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the values vector.
 * @param values The vector from which to select groups of elements.
 */
template<class T>
void selectVectorValues(std::vector<T>& vector, storm::storage::BitVector const& positions, std::vector<uint_fast64_t> const& rowGrouping,
                        std::vector<T> const& values) {
    auto targetIt = vector.begin();
    for (auto position : positions) {
        for (uint_fast64_t i = rowGrouping[position]; i < rowGrouping[position + 1]; ++i, ++targetIt) {
            *targetIt = values[i];
        }
    }
}

/*!
 * Selects one element out of each row group and writes it to the target vector.
 *
 * @param vector The target vector to which the values are written.
 * @param rowGroupToRowIndexMapping A mapping from row group indices to an offset that specifies which of the values to
 * take from the row group.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the values vector.
 * @param values The vector from which to select the values.
 */
template<class T>
void selectVectorValues(std::vector<T>& vector, std::vector<uint_fast64_t> const& rowGroupToRowIndexMapping, std::vector<uint_fast64_t> const& rowGrouping,
                        std::vector<T> const& values) {
    auto targetIt = vector.begin();
    for (uint_fast64_t i = 0; i < vector.size(); ++i, ++targetIt) {
        *targetIt = values[rowGrouping[i] + rowGroupToRowIndexMapping[i]];
    }
}

/*!
 * Selects values from a vector at the specified sequence of indices and writes them into another vector
 *
 * @param vector The vector into which the selected elements are written.
 * @param indexSequence a sequence of indices at which the desired values can be found
 * @param values the values from which to select
 */
template<class T>
void selectVectorValues(std::vector<T>& vector, std::vector<uint_fast64_t> const& indexSequence, std::vector<T> const& values) {
    STORM_LOG_ASSERT(indexSequence.size() <= vector.size(),
                     "The number of selected positions (" << indexSequence.size() << ") exceeds the size of the target vector (" << vector.size() << ").");

    for (uint_fast64_t vectorIndex = 0; vectorIndex < vector.size(); ++vectorIndex) {
        vector[vectorIndex] = values[indexSequence[vectorIndex]];
    }
}

/*!
 * Selects values from a vector at the specified positions and writes them into another vector as often as given by
 * the size of the corresponding group of elements.
 *
 * @param vector The vector into which the selected elements are written.
 * @param positions The positions at which to select the values.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the values vector. This
 * implicitly defines the number of times any element is written to the output vector.
 */
template<class T>
void selectVectorValuesRepeatedly(std::vector<T>& vector, storm::storage::BitVector const& positions, std::vector<uint_fast64_t> const& rowGrouping,
                                  std::vector<T> const& values) {
    auto targetIt = vector.begin();
    for (auto position : positions) {
        for (uint_fast64_t i = rowGrouping[position]; i < rowGrouping[position + 1]; ++i, ++targetIt) {
            *targetIt = values[position];
        }
    }
}

/*!
 * Subtracts the given vector from the constant one-vector and writes the result to the input vector.
 *
 * @param vector The vector that is to be subtracted from the constant one-vector.
 */
template<class T>
void subtractFromConstantOneVector(std::vector<T>& vector) {
    for (auto& element : vector) {
        element = storm::utility::one<T>() - element;
    }
}

template<class T>
void addFilteredVectorGroupsToGroupedVector(std::vector<T>& target, std::vector<T> const& source, storm::storage::BitVector const& filter,
                                            std::vector<uint_fast64_t> const& rowGroupIndices) {
    auto targetIt = target.begin();
    for (auto group : filter) {
        auto it = source.cbegin() + rowGroupIndices[group];
        auto ite = source.cbegin() + rowGroupIndices[group + 1];
        for (; it != ite; ++targetIt, ++it) {
            *targetIt += *it;
        }
    }
}

/*!
 * Adds the source vector to the target vector in a way such that the i-th entry is added to all elements of
 * the i-th row group in the target vector.
 *
 * @param target The target ("row grouped") vector.
 * @param source The source vector.
 * @param rowGroupIndices A vector representing the row groups in the target vector.
 */
template<class T>
void addVectorToGroupedVector(std::vector<T>& target, std::vector<T> const& source, std::vector<uint_fast64_t> const& rowGroupIndices) {
    auto targetIt = target.begin();
    auto sourceIt = source.cbegin();
    auto sourceIte = source.cend();
    auto rowGroupIt = rowGroupIndices.cbegin();

    for (; sourceIt != sourceIte; ++sourceIt) {
        uint_fast64_t current = *rowGroupIt;
        ++rowGroupIt;
        uint_fast64_t next = *rowGroupIt;
        for (; current < next; ++targetIt, ++current) {
            *targetIt += *sourceIt;
        }
    }
}

/*!
 * Adds the source vector to the target vector in a way such that the i-th selected entry is added to all
 * elements of the i-th row group in the target vector.
 *
 * @param target The target ("row grouped") vector.
 * @param source The source vector.
 * @param rowGroupIndices A vector representing the row groups in the target vector.
 */
template<class T>
void addFilteredVectorToGroupedVector(std::vector<T>& target, std::vector<T> const& source, storm::storage::BitVector const& filter,
                                      std::vector<uint_fast64_t> const& rowGroupIndices) {
    auto targetIt = target.begin();
    for (auto group : filter) {
        uint_fast64_t current = rowGroupIndices[group];
        uint_fast64_t next = rowGroupIndices[group + 1];
        for (; current < next; ++current, ++targetIt) {
            *targetIt += source[group];
        }
    }
}

/*!
 * Applies the given operation pointwise on the two given vectors and writes the result to the third vector.
 * To obtain an in-place operation, the third vector may be equal to any of the other two vectors.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand.
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType, class Operation>
void applyPointwiseTernary(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand, std::vector<OutValueType>& target,
                           Operation f = Operation()) {
    auto firstIt = firstOperand.begin();
    auto firstIte = firstOperand.end();
    auto secondIt = secondOperand.begin();
    auto targetIt = target.begin();
    while (firstIt != firstIte) {
        *targetIt = f(*firstIt, *secondIt, *targetIt);
        ++targetIt;
        ++firstIt;
        ++secondIt;
    }
}

/*!
 * Applies the given operation pointwise on the two given vectors and writes the result to the third vector.
 * To obtain an in-place operation, the third vector may be equal to any of the other two vectors.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand.
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType, class Operation>
void applyPointwise(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand, std::vector<OutValueType>& target,
                    Operation f = Operation()) {
    std::transform(firstOperand.begin(), firstOperand.end(), secondOperand.begin(), target.begin(), f);
}

/*!
 * Applies the given function pointwise on the given vector.
 *
 * @param operand The vector to which to apply the function.
 * @param target The target vector.
 * @param function The function to apply.
 */
template<class InValueType, class OutValueType, class Operation>
void applyPointwise(std::vector<InValueType> const& operand, std::vector<OutValueType>& target, Operation f = Operation()) {
    std::transform(operand.begin(), operand.end(), target.begin(), f);
}

/*!
 * Adds the two given vectors and writes the result to the target vector.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType>
void addVectors(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand, std::vector<OutValueType>& target) {
    applyPointwise<InValueType1, InValueType2, OutValueType, std::plus<>>(firstOperand, secondOperand, target);
}

/*!
 * Subtracts the two given vectors and writes the result to the target vector.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType>
void subtractVectors(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand, std::vector<OutValueType>& target) {
    applyPointwise<InValueType1, InValueType2, OutValueType, std::minus<>>(firstOperand, secondOperand, target);
}

/*!
 * Multiplies the two given vectors (pointwise) and writes the result to the target vector.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType>
void multiplyVectorsPointwise(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand,
                              std::vector<OutValueType>& target) {
    applyPointwise<InValueType1, InValueType2, OutValueType, std::multiplies<>>(firstOperand, secondOperand, target);
}

/*!
 * Divides the two given vectors (pointwise) and writes the result to the target vector.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @param target The target vector.
 */
template<class InValueType1, class InValueType2, class OutValueType>
void divideVectorsPointwise(std::vector<InValueType1> const& firstOperand, std::vector<InValueType2> const& secondOperand, std::vector<OutValueType>& target) {
    applyPointwise<InValueType1, InValueType2, OutValueType, std::divides<>>(firstOperand, secondOperand, target);
}

/*!
 * Multiplies each element of the given vector with the given factor and writes the result into the vector.
 *
 * @param target The operand and target vector.
 * @param factor The scaling factor
 */
template<class ValueType1, class ValueType2>
void scaleVectorInPlace(std::vector<ValueType1>& target, ValueType2 const& factor) {
    applyPointwise<ValueType1, ValueType1>(target, target, [&](ValueType1 const& argument) -> ValueType1 { return argument * factor; });
}

/*!
 * Computes x:= x + a*y, i.e., adds each element of the first vector and (the corresponding element of the second vector times the given factor) and writes the
 * result into the first vector.
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @param factor The factor for the elements of the second operand
 */
template<class InValueType1, class InValueType2, class InValueType3>
void addScaledVector(std::vector<InValueType1>& firstOperand, std::vector<InValueType2> const& secondOperand, InValueType3 const& factor) {
    applyPointwise<InValueType1, InValueType2, InValueType1>(
        firstOperand, secondOperand, firstOperand, [&](InValueType1 const& val1, InValueType2 const& val2) -> InValueType1 { return val1 + (factor * val2); });
}

/*!
 * Computes the dot product (aka scalar product) and returns the result
 *
 * @param firstOperand The first operand.
 * @param secondOperand The second operand
 * @return firstOperand*secondOperand
 */
template<class T>
T dotProduct(std::vector<T> const& firstOperand, std::vector<T> const& secondOperand) {
    return std::inner_product(firstOperand.begin(), firstOperand.end(), secondOperand.begin(), storm::utility::zero<T>());
}

/*!
 * Retrieves a bit vector containing all the indices for which the value at this position makes the given
 * function evaluate to true.
 *
 * @param values The vector of values.
 * @param function The function that selects some elements.
 * @return The resulting bit vector.
 */
template<class T>
storm::storage::BitVector filter(std::vector<T> const& values, std::function<bool(T const& value)> const& function) {
    storm::storage::BitVector result(values.size(), false);

    uint_fast64_t currentIndex = 0;
    for (auto const& value : values) {
        if (function(value)) {
            result.set(currentIndex, true);
        }
        ++currentIndex;
    }

    return result;
}

/*!
 * Retrieves a bit vector containing all the indices for which the value at this position is greater than
 * zero
 *
 * @param values The vector of values.
 * @return The resulting bit vector.
 */
template<class T>
storm::storage::BitVector filterGreaterZero(std::vector<T> const& values) {
    return filter<T>(values, [](T const& value) -> bool { return value > storm::utility::zero<T>(); });
}

/*!
 * Retrieves a bit vector containing all the indices for which the value at this position is equal to zero
 *
 * @param values The vector of values.
 * @return The resulting bit vector.
 */
template<class T>
storm::storage::BitVector filterZero(std::vector<T> const& values) {
    return filter<T>(values, storm::utility::isZero<T>);
}

/*!
 * Retrieves a bit vector containing all the indices for which the value at this position is equal to one
 *
 * @param values The vector of values.
 * @return The resulting bit vector.
 */
template<class T>
storm::storage::BitVector filterOne(std::vector<T> const& values) {
    return filter<T>(values, storm::utility::isOne<T>);
}

/*!
 * Retrieves a bit vector containing all the indices for which the value at this position is equal to one
 *
 * @param values The vector of values.
 * @return The resulting bit vector.
 */
template<class T>
storm::storage::BitVector filterInfinity(std::vector<T> const& values) {
    return filter<T>(values, storm::utility::isInfinity<T>);
}

/**
 * Sum the entries from values that are set to one in the filter vector.
 * @param values
 * @param filter
 * @return The sum of the values with a corresponding one in the filter.
 */
template<typename VT>
VT sum_if(std::vector<VT> const& values, storm::storage::BitVector const& filter) {
    STORM_LOG_ASSERT(values.size() == filter.size(), "Vector sizes mismatch.");
    VT sum = storm::utility::zero<VT>();
    for (auto pos : filter) {
        sum += values[pos];
    }
    return sum;
}

/**
 * Computes the maximum of the entries from the values that are selected by the (non-empty) filter.
 * @param values The values in which to search.
 * @param filter The filter to use.
 * @return The maximum over the selected values.
 */
template<typename VT>
VT max_if(std::vector<VT> const& values, storm::storage::BitVector const& filter) {
    STORM_LOG_ASSERT(values.size() == filter.size(), "Vector sizes mismatch.");
    STORM_LOG_ASSERT(!filter.empty(), "Empty selection.");

    auto it = filter.begin();
    auto ite = filter.end();

    VT current = values[*it];
    ++it;

    for (; it != ite; ++it) {
        current = std::max(values[*it], current);
    }
    return current;
}

/**
 * Computes the minimum of the entries from the values that are selected by the (non-empty) filter.
 * @param values The values in which to search.
 * @param filter The filter to use.
 * @return The minimum over the selected values.
 */
template<typename VT>
VT min_if(std::vector<VT> const& values, storm::storage::BitVector const& filter) {
    STORM_LOG_ASSERT(values.size() == filter.size(), "Vector sizes mismatch.");
    STORM_LOG_ASSERT(!filter.empty(), "Empty selection.");

    auto it = filter.begin();
    auto ite = filter.end();

    VT current = values[*it];
    ++it;

    for (; it != ite; ++it) {
        current = std::min(values[*it], current);
    }
    return current;
}

/*!
 * Reduces the given source vector by selecting an element according to the given filter out of each row group.
 *
 * @param source The source vector which is to be reduced.
 * @param target The target vector into which a single element from each row group is written.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the values vector.
 * @param filter A function that compares two elements v1 and v2 according to some filter criterion. This function must
 * return true iff v1 is supposed to be taken instead of v2.
 * @param choices If non-null, this vector is used to store the choices made during the selection.
 */
template<class T, class Filter>
void reduceVector(std::vector<T> const& source, std::vector<T>& target, std::vector<uint_fast64_t> const& rowGrouping, std::vector<uint_fast64_t>* choices) {
    Filter f;
    typename std::vector<T>::iterator targetIt = target.begin();
    typename std::vector<T>::iterator targetIte = target.end();
    typename std::vector<uint_fast64_t>::const_iterator rowGroupingIt = rowGrouping.begin();
    typename std::vector<T>::const_iterator sourceIt = source.begin();
    typename std::vector<T>::const_iterator sourceIte;
    typename std::vector<uint_fast64_t>::iterator choiceIt;
    if (choices) {
        choiceIt = choices->begin();
    }

    // Variables for correctly tracking choices (only update if new choice is strictly better).
    T oldSelectedChoiceValue;
    uint64_t selectedChoice;

    uint64_t currentRow = 0;
    for (; targetIt != targetIte; ++targetIt, ++rowGroupingIt, ++choiceIt) {
        // Only traverse elements if the row group is non-empty.
        if (*rowGroupingIt != *(rowGroupingIt + 1)) {
            *targetIt = *sourceIt;

            if (choices) {
                selectedChoice = 0;
                if (*choiceIt == 0) {
                    oldSelectedChoiceValue = *targetIt;
                }
            }

            ++sourceIt;
            ++currentRow;

            for (sourceIte = source.begin() + *(rowGroupingIt + 1); sourceIt != sourceIte; ++sourceIt, ++currentRow) {
                if (choices && *rowGroupingIt + *choiceIt == currentRow) {
                    oldSelectedChoiceValue = *sourceIt;
                }

                if (f(*sourceIt, *targetIt)) {
                    *targetIt = *sourceIt;
                    if (choices) {
                        selectedChoice = std::distance(source.begin(), sourceIt) - *rowGroupingIt;
                    }
                }
            }

            if (choices && f(*targetIt, oldSelectedChoiceValue)) {
                *choiceIt = selectedChoice;
            }
        } else {
            *choiceIt = 0;
            *targetIt = storm::utility::zero<T>();
        }
    }
}

/*!
 * Reduces the given source vector by selecting the smallest element out of each row group.
 *
 * @param source The source vector which is to be reduced.
 * @param target The target vector into which a single element from each row group is written.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the source vector.
 * @param choices If non-null, this vector is used to store the choices made during the selection.
 */
template<class T>
void reduceVectorMin(std::vector<T> const& source, std::vector<T>& target, std::vector<uint_fast64_t> const& rowGrouping,
                     std::vector<uint_fast64_t>* choices = nullptr) {
    reduceVector<T, storm::utility::ElementLess<T>>(source, target, rowGrouping, choices);
}

/*!
 * Reduces the given source vector by selecting the largest element out of each row group.
 *
 * @param source The source vector which is to be reduced.
 * @param target The target vector into which a single element from each row group is written.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the source vector.
 * @param choices If non-null, this vector is used to store the choices made during the selection.
 */
template<class T>
void reduceVectorMax(std::vector<T> const& source, std::vector<T>& target, std::vector<uint_fast64_t> const& rowGrouping,
                     std::vector<uint_fast64_t>* choices = nullptr) {
    reduceVector<T, storm::utility::ElementGreater<T>>(source, target, rowGrouping, choices);
}

/*!
 * Reduces the given source vector by selecting either the smallest or the largest out of each row group.
 *
 * @param dir If true, select the smallest, else select the largest.
 * @param source The source vector which is to be reduced.
 * @param target The target vector into which a single element from each row group is written.
 * @param rowGrouping A vector that specifies the begin and end of each group of elements in the source vector.
 * @param choices If non-null, this vector is used to store the choices made during the selection.
 */
template<class T>
void reduceVectorMinOrMax(storm::solver::OptimizationDirection dir, std::vector<T> const& source, std::vector<T>& target,
                          std::vector<uint_fast64_t> const& rowGrouping, std::vector<uint_fast64_t>* choices = nullptr) {
    if (dir == storm::solver::OptimizationDirection::Minimize) {
        reduceVectorMin(source, target, rowGrouping, choices);
    } else {
        reduceVectorMax(source, target, rowGrouping, choices);
    }
}

/*!
 * Compares the given elements and determines whether they are equal modulo the given precision. The provided flag
 * additionaly specifies whether the error is computed in relative or absolute terms.
 *
 * @param val1 The first value to compare.
 * @param val2 The second value to compare.
 * @param precision The precision up to which the elements are compared.
 * @param relativeError If set, the error is computed relative to the second value.
 * @return True iff the elements are considered equal.
 */
template<class T>
bool equalModuloPrecision(T const& val1, T const& val2, T const& precision, bool relativeError = true) {
    if (relativeError) {
        if (storm::utility::isZero<T>(val1)) {
            return storm::utility::isZero(val2);
        }
        T relDiff = (val1 - val2) / val1;
        if (storm::utility::abs(relDiff) > precision) {
            return false;
        }
    } else {
        T diff = val1 - val2;
        if (storm::utility::abs(diff) > precision) {
            return false;
        }
    }
    return true;
}

// Specializiation for double as the relative check for doubles very close to zero is not meaningful.
template<>
inline bool equalModuloPrecision(double const& val1, double const& val2, double const& precision, bool relativeError) {
    if (relativeError) {
        if (storm::utility::isAlmostZero(val2)) {
            return storm::utility::isAlmostZero(val1);
        }
        double relDiff = (val1 - val2) / val1;
        if (storm::utility::abs(relDiff) > precision) {
            return false;
        }
    } else {
        double diff = val1 - val2;
        if (storm::utility::abs(diff) > precision) {
            return false;
        }
    }
    return true;
}

/*!
 * Compares the two vectors and determines whether they are equal modulo the provided precision. Depending on whether the
 * flag is set, the difference between the vectors is computed relative to the value or in absolute terms.
 *
 * @param vectorLeft The first vector of the comparison.
 * @param vectorRight The second vector of the comparison.
 * @param precision The precision up to which the vectors are to be checked for equality.
 * @param relativeError If set, the difference between the vectors is computed relative to the value or in absolute terms.
 */
template<class T>
bool equalModuloPrecision(std::vector<T> const& vectorLeft, std::vector<T> const& vectorRight, T const& precision, bool relativeError) {
    STORM_LOG_ASSERT(vectorLeft.size() == vectorRight.size(), "Lengths of vectors does not match.");

    auto leftIt = vectorLeft.begin();
    auto leftIte = vectorLeft.end();
    auto rightIt = vectorRight.begin();
    for (; leftIt != leftIte; ++leftIt, ++rightIt) {
        if (!equalModuloPrecision(*leftIt, *rightIt, precision, relativeError)) {
            return false;
        }
    }

    return true;
}

/*!
 * Compares the two vectors at the specified positions and determines whether they are equal modulo the provided
 * precision. Depending on whether the flag is set, the difference between the vectors is computed relative to the value
 * or in absolute terms.
 *
 * @param vectorLeft The first vector of the comparison.
 * @param vectorRight The second vector of the comparison.
 * @param precision The precision up to which the vectors are to be checked for equality.
 * @param positions A vector representing a set of positions at which the vectors are compared.
 * @param relativeError If set, the difference between the vectors is computed relative to the value or in absolute terms.
 */
template<class T>
bool equalModuloPrecision(std::vector<T> const& vectorLeft, std::vector<T> const& vectorRight, storm::storage::BitVector const& positions, T const& precision,
                          bool relativeError) {
    STORM_LOG_ASSERT(vectorLeft.size() == vectorRight.size(), "Lengths of vectors does not match.");

    for (auto position : positions) {
        if (!equalModuloPrecision(vectorLeft[position], vectorRight[position], precision, relativeError)) {
            return false;
        }
    }

    return true;
}

/*!
 * Compares the two vectors at the specified positions and determines whether they are equal modulo the provided
 * precision. Depending on whether the flag is set, the difference between the vectors is computed relative to the value
 * or in absolute terms.
 *
 * @param vectorLeft The first vector of the comparison.
 * @param vectorRight The second vector of the comparison.
 * @param precision The precision up to which the vectors are to be checked for equality.
 * @param positions A vector representing a set of positions at which the vectors are compared.
 * @param relativeError If set, the difference between the vectors is computed relative to the value or in absolute terms.
 */
template<class T>
bool equalModuloPrecision(std::vector<T> const& vectorLeft, std::vector<T> const& vectorRight, std::vector<uint_fast64_t> const& positions, T const& precision,
                          bool relativeError) {
    STORM_LOG_ASSERT(vectorLeft.size() == vectorRight.size(), "Lengths of vectors does not match.");

    for (uint_fast64_t position : positions) {
        if (!equalModuloPrecision(vectorLeft[position], vectorRight[position], precision, relativeError)) {
            return false;
        }
    }

    return true;
}

template<class T>
T maximumElementAbs(std::vector<T> const& vector) {
    T res = storm::utility::zero<T>();
    for (auto const& element : vector) {
        res = std::max(res, storm::utility::abs(element));
    }
    return res;
}

template<class T>
T maximumElementDiff(std::vector<T> const& vectorLeft, std::vector<T> const& vectorRight) {
    T maxDiff = storm::utility::zero<T>();
    auto leftIt = vectorLeft.begin();
    auto leftIte = vectorLeft.end();
    auto rightIt = vectorRight.begin();
    for (; leftIt != leftIte; ++leftIt, ++rightIt) {
        T diff = *leftIt - *rightIt;
        T possDiff = storm::utility::abs(diff);
        maxDiff = maxDiff < possDiff ? possDiff : maxDiff;
    }
    return maxDiff;
}

template<class T>
T computeSquaredNorm2Difference(std::vector<T> const& b1, std::vector<T> const& b2) {
    STORM_LOG_ASSERT(b1.size() == b2.size(), "Vector sizes mismatch.");

    T result = storm::utility::zero<T>();

    auto b1It = b1.begin();
    auto b1Ite = b1.end();
    auto b2It = b2.begin();

    for (; b1It != b1Ite; ++b1It, ++b2It) {
        result += storm::utility::pow<T>(*b1It - *b2It, 2);
    }

    return result;
}

/*!
 * Takes the input vector and ensures that all entries conform to the bounds.
 */
template<typename ValueType>
void clip(std::vector<ValueType>& x, boost::optional<ValueType> const& lowerBound, boost::optional<ValueType> const& upperBound) {
    for (auto& entry : x) {
        if (lowerBound && entry < lowerBound.get()) {
            entry = lowerBound.get();
        } else if (upperBound && entry > upperBound.get()) {
            entry = upperBound.get();
        }
    }
}

/*!
 * Takes the input vector and ensures that all entries conform to the bounds.
 */
template<typename ValueType>
void clip(std::vector<ValueType>& x, ValueType const& bound, bool boundFromBelow) {
    for (auto& entry : x) {
        if (boundFromBelow && entry < bound) {
            entry = bound;
        } else if (!boundFromBelow && entry > bound) {
            entry = bound;
        }
    }
}

/*!
 * Takes the input vector and ensures that all entries conform to the bounds.
 */
template<typename ValueType>
void clip(std::vector<ValueType>& x, std::vector<ValueType> const& bounds, bool boundFromBelow) {
    auto boundsIt = bounds.begin();
    for (auto& entry : x) {
        if (boundFromBelow && entry < *boundsIt) {
            entry = *boundsIt;
        } else if (!boundFromBelow && entry > *boundsIt) {
            entry = *boundsIt;
        }
        ++boundsIt;
    }
}

/*!
 * Takes the given offset vector and applies the given contraint. That is, it produces another offset vector that contains
 * the relative offsets of the entries given by the constraint.
 *
 * @param offsetVector The offset vector to constrain.
 * @param constraint The constraint to apply to the offset vector.
 * @return An offset vector that contains all selected relative offsets.
 */
template<class T>
std::vector<T> getConstrainedOffsetVector(std::vector<T> const& offsetVector, storm::storage::BitVector const& constraint) {
    // Reserve the known amount of slots for the resulting vector.
    std::vector<uint_fast64_t> subVector(constraint.getNumberOfSetBits() + 1);
    uint_fast64_t currentRowCount = 0;
    uint_fast64_t currentIndexCount = 1;

    // Set the first element as this will clearly begin at offset 0.
    subVector[0] = 0;

    // Loop over all states that need to be kept and copy the relative indices of the nondeterministic choices over
    // to the resulting vector.
    for (auto index : constraint) {
        subVector[currentIndexCount] = currentRowCount + offsetVector[index + 1] - offsetVector[index];
        currentRowCount += offsetVector[index + 1] - offsetVector[index];
        ++currentIndexCount;
    }

    // Put a sentinel element at the end.
    subVector[constraint.getNumberOfSetBits()] = currentRowCount;

    return subVector;
}

/*!
 * Converts the given vector to the given ValueType
 * Assumes that both, TargetType and SourceType are numeric
 * @return the resulting vector
 */
template<typename TargetType, typename SourceType>
std::vector<TargetType> convertNumericVector(std::vector<SourceType> const& oldVector) {
    std::vector<TargetType> resultVector;
    resultVector.reserve(oldVector.size());
    for (auto const& oldValue : oldVector) {
        resultVector.push_back(storm::utility::convertNumber<TargetType>(oldValue));
    }
    return resultVector;
}

/*!
 * Converts the given vector to the given ValueType
 * Assumes that both, TargetType and SourceType are numeric
 *
 * @param inputVector the vector that needs to be converted
 * @param targetVector the vector where the result is written to. Should have the same size as inputVector
 *
 * @note this does not allocate new memory
 */
template<typename TargetType, typename SourceType>
void convertNumericVector(std::vector<SourceType> const& inputVector, std::vector<TargetType>& targetVector) {
    assert(inputVector.size() == targetVector.size());
    applyPointwise(inputVector, targetVector, [](SourceType const& v) { return storm::utility::convertNumber<TargetType>(v); });
}

/*!
 * Converts the given vector to the given ValueType
 */
template<typename NewValueType, typename ValueType>
std::vector<NewValueType> toValueType(std::vector<ValueType> const& oldVector) {
    std::vector<NewValueType> resultVector;
    resultVector.reserve(oldVector.size());
    for (auto const& oldValue : oldVector) {
        resultVector.push_back(static_cast<NewValueType>(oldValue));
    }
    return resultVector;
}

template<typename ValueType, typename TargetValueType>
typename std::enable_if<std::is_same<ValueType, storm::RationalNumber>::value, std::pair<std::vector<TargetValueType>, ValueType>>::type toIntegralVector(
    std::vector<ValueType> const& vec) {
    // Collect the numbers occurring in the input vector
    std::set<ValueType> occurringNonZeroNumbers;
    for (auto const& v : vec) {
        if (!storm::utility::isZero(v)) {
            occurringNonZeroNumbers.insert(v);
        }
    }

    // Compute the scaling factor
    ValueType factor;
    if (occurringNonZeroNumbers.empty()) {
        factor = storm::utility::one<ValueType>();
    } else if (occurringNonZeroNumbers.size() == 1) {
        factor = *occurringNonZeroNumbers.begin();
    } else {
        // Obtain the least common multiple of the denominators of the occurring numbers.
        // We can then multiply the numbers with the lcm to obtain integers.
        auto numberIt = occurringNonZeroNumbers.begin();
        ValueType lcm = storm::utility::asFraction(*numberIt).second;
        for (++numberIt; numberIt != occurringNonZeroNumbers.end(); ++numberIt) {
            lcm = carl::lcm(lcm, storm::utility::asFraction(*numberIt).second);
        }
        // Multiply all values with the lcm. To reduce the range of considered integers, we also obtain the gcd of the results.
        numberIt = occurringNonZeroNumbers.begin();
        ValueType gcd = *numberIt * lcm;
        for (++numberIt; numberIt != occurringNonZeroNumbers.end(); ++numberIt) {
            gcd = carl::gcd(gcd, static_cast<ValueType>(*numberIt * lcm));
        }

        factor = gcd / lcm;
    }

    // Build the result
    std::vector<TargetValueType> result;
    result.reserve(vec.size());
    for (auto const& v : vec) {
        ValueType vScaled = v / factor;
        STORM_LOG_ASSERT(storm::utility::isInteger(vScaled), "Resulting number '(" << v << ")/(" << factor << ") = " << vScaled << "' is not integral.");
        result.push_back(storm::utility::convertNumber<TargetValueType, ValueType>(vScaled));
    }
    return std::make_pair(std::move(result), std::move(factor));
}

template<typename ValueType, typename TargetValueType>
typename std::enable_if<!std::is_same<ValueType, storm::RationalNumber>::value, std::pair<std::vector<TargetValueType>, ValueType>>::type toIntegralVector(
    std::vector<ValueType> const& vec) {
    // TODO: avoid converting back and forth
    auto rationalNumberVec = convertNumericVector<storm::RationalNumber>(vec);
    auto rationalNumberResult = toIntegralVector<storm::RationalNumber, TargetValueType>(rationalNumberVec);

    return std::make_pair(std::move(rationalNumberResult.first), storm::utility::convertNumber<ValueType>(rationalNumberResult.second));
}

template<typename Type>
std::vector<Type> filterVector(std::vector<Type> const& in, storm::storage::BitVector const& filter) {
    std::vector<Type> result;
    result.reserve(filter.getNumberOfSetBits());
    for (auto index : filter) {
        result.push_back(in[index]);
    }
    STORM_LOG_ASSERT(result.size() == filter.getNumberOfSetBits(), "Result does not match.");
    return result;
}

template<typename Type>
void filterVectorInPlace(std::vector<Type>& v, storm::storage::BitVector const& filter) {
    STORM_LOG_ASSERT(v.size() == filter.size(), "The filter size does not match the size of the input vector");
    uint_fast64_t size = v.size();
    // we can start our work at the first index where the filter has value zero
    uint_fast64_t firstUnsetIndex = filter.getNextUnsetIndex(0);
    if (firstUnsetIndex < size) {
        auto vIt = v.begin() + firstUnsetIndex;
        for (uint_fast64_t index = filter.getNextSetIndex(firstUnsetIndex + 1); index != size; index = filter.getNextSetIndex(index + 1)) {
            *vIt = std::move(v[index]);
            ++vIt;
        }
        v.resize(vIt - v.begin());
        v.shrink_to_fit();
    }
    STORM_LOG_ASSERT(v.size() == filter.getNumberOfSetBits(), "Result does not match.");
}

/*!
 * Gets as input a vector with size n and a BitVector with size m>=n and exactly n set bits.
 * The function modifies the input vector in place such that the resulting vector has size m and contains the original values at the positions where the
 * BitVector has a set bit. The remaining positions are filled with the default value.
 * @tparam T
 * @param vector input and output vector
 * @param positions the positions at which the output vector will have the values from the input vector
 * @param defaultValue the value to set for all other values
 *
 * @note This is the inverse operation to filterVectorInPlace: blowUpVectorInPlace(v, bv); filterVectorInPlace(v, bv); results in the original vector v
 *
 */
template<class T>
void blowUpVectorInPlace(std::vector<T>& vector, storm::storage::BitVector const& positions, T const& defaultValue = storm::utility::zero<T>()) {
    STORM_LOG_ASSERT(vector.size() == positions.getNumberOfSetBits(), "The number of selected positions (" << positions.getNumberOfSetBits()
                                                                                                           << ") must match the size of the input vector ("
                                                                                                           << vector.size() << ").");
    auto readPos = vector.size();
    vector.resize(positions.size(), defaultValue);
    auto writePos = positions.getStartOfZeroSequenceBefore(positions.size());
    while (writePos > 0) {
        --writePos;
        if (positions.get(writePos)) {
            --readPos;
            if (readPos != writePos) {
                STORM_LOG_ASSERT(readPos < writePos, "We shall not write to positions that we later still have to read from.");
                vector[writePos] = vector[readPos];
            }
        } else {
            vector[writePos] = defaultValue;
        }
    }
}

template<typename T>
bool hasNegativeEntry(std::vector<T> const& v) {
    return std::any_of(v.begin(), v.end(), [](T value) { return value < storm::utility::zero<T>(); });
}

template<typename T>
bool hasPositiveEntry(std::vector<T> const& v) {
    return std::any_of(v.begin(), v.end(), [](T value) { return value > storm::utility::zero<T>(); });
}

template<typename T>
bool hasNonZeroEntry(std::vector<T> const& v) {
    return std::any_of(v.begin(), v.end(), [](T value) { return !storm::utility::isZero(value); });
}

template<typename T>
bool hasZeroEntry(std::vector<T> const& v) {
    return std::any_of(v.begin(), v.end(), [](T value) { return storm::utility::isZero(value); });
}

template<typename T>
bool hasInfinityEntry(std::vector<T> const& v) {
    return std::any_of(v.begin(), v.end(), [](T value) { return storm::utility::isInfinity(value); });
}

template<typename T>
std::vector<T> applyInversePermutation(std::vector<uint64_t> const& inversePermutation, std::vector<T> const& source) {
    std::vector<T> result;
    result.reserve(source.size());
    for (uint64_t sourceIndex : inversePermutation) {
        result.push_back(source[sourceIndex]);
    }
    return result;
}

template<typename T>
std::vector<T> applyInversePermutationToGroupedVector(std::vector<uint64_t> const& inversePermutation, std::vector<T> const& source,
                                                      std::vector<uint64_t> const& rowGroupIndices) {
    STORM_LOG_ASSERT(inversePermutation.size() == rowGroupIndices.size() - 1, "Inverse permutation and row group indices do not match.");
    std::vector<T> result;
    result.reserve(source.size());
    for (auto sourceGroupIndex : inversePermutation) {
        for (uint64_t sourceIndex = rowGroupIndices[sourceGroupIndex]; sourceIndex < rowGroupIndices[sourceGroupIndex + 1]; ++sourceIndex) {
            result.push_back(source[sourceIndex]);
        }
    }
    STORM_LOG_ASSERT(result.size() == source.size(), "result has unexpected length.");
    return result;
}

/*!
 * Output vector as string.
 *
 * @param vector Vector to output.
 * @return String containing the representation of the vector.
 */
template<typename ValueType>
std::string toString(std::vector<ValueType> const& vector) {
    std::stringstream stream;
    stream << "vector (" << vector.size() << ") [ ";
    if (!vector.empty()) {
        for (uint_fast64_t i = 0; i < vector.size() - 1; ++i) {
            stream << vector[i] << ", ";
        }
        stream << vector.back();
    }
    stream << " ]";
    return stream.str();
}

}  // namespace vector
}  // namespace utility
}  // namespace storm
