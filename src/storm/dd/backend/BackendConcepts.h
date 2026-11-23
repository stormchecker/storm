#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include "storm/dd/DdType.h"
#include "storm/dd/Odd.h"
#include "storm/expressions/Expression.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/storage/BitVector.h"

/*!
 * The contract of a DD backend.
 *
 * A backend provides full specializations the concepts below for
 * - InternalDdManager<T>
 * - InternalBdd<T>
 * - InternalAdd<T, ValueType>
 * - AddIterator<T, ValueType>
 *
 * Some operations could in principle be derived from other operations. Currently, they are implemented natively.
 *
 * The member semantics are documented in the CUDD/Sylvan headers.
 * The generic layer (Bdd.cpp, Add.cpp, DdManager.cpp) defines what is relied upon.
 */
namespace storm {
namespace dd {

/*!
 * Contract of InternalBdd<T>.
 * `AddType` is the matching InternalAdd<T, ValueType>, `ManagerType` the matching InternalDdManager.
 */
template<typename BddType, typename AddType, typename ManagerType, typename ValueType>
concept InternalBddBackend =
    std::copyable<BddType> && std::default_initializable<BddType> && std::equality_comparable<BddType> &&
    requires(BddType const& b, BddType const& other, BddType& mb, std::vector<BddType> const& vars, AddType const& a, std::vector<uint_fast64_t> const& indices,
             storm::dd::Odd const& odd, std::function<bool(uint_fast64_t)> const& filter, std::string const& file, std::vector<std::string> const& names,
             storm::expressions::ExpressionManager& em, storm::storage::BitVector const& bits, storm::storage::BitVector& outBits,
             std::vector<ValueType> const& src, std::vector<ValueType>& dst) {
        // Construction.
        { BddType::fromVector(std::declval<ManagerType const*>(), odd, indices, filter) } -> std::same_as<BddType>;

        // Boolean connectives.
        { b || other } -> std::same_as<BddType>;
        { mb |= other } -> std::same_as<BddType&>;
        { b && other } -> std::same_as<BddType>;
        { mb &= other } -> std::same_as<BddType&>;
        { b.iff(other) } -> std::same_as<BddType>;
        { b.exclusiveOr(other) } -> std::same_as<BddType>;
        { b.implies(other) } -> std::same_as<BddType>;
        { !b } -> std::same_as<BddType>;
        { mb.complement() } -> std::same_as<BddType&>;
        { b.ite(other, other) } -> std::same_as<BddType>;
        { b.ite(a, a) } -> std::same_as<AddType>;

        // Quantification, restriction, relational operations. Cubes are passed as BDDs.
        { b.existsAbstract(other) } -> std::same_as<BddType>;
        { b.existsAbstractRepresentative(other) } -> std::same_as<BddType>;
        { b.universalAbstract(other) } -> std::same_as<BddType>;
        { b.constrain(other) } -> std::same_as<BddType>;
        { b.restrict(other) } -> std::same_as<BddType>;
        { b.swapVariables(vars, vars) } -> std::same_as<BddType>;
        { b.relationalProduct(other, vars, vars) } -> std::same_as<BddType>;
        { b.getSupport() } -> std::same_as<BddType>;
        // The following could be derived from other operations
        { b.andExists(other, other) } -> std::same_as<BddType>;
        { b.inverseRelationalProduct(other, vars, vars) } -> std::same_as<BddType>;
        { b.inverseRelationalProductWithExtendedRelation(other, vars, vars) } -> std::same_as<BddType>;

        // Inspection.
        { b.getNonZeroCount(uint_fast64_t{}) } -> std::same_as<uint_fast64_t>;
        { b.getLeafCount() } -> std::same_as<uint_fast64_t>;
        { b.getNodeCount() } -> std::same_as<uint_fast64_t>;
        { b.isOne() } -> std::same_as<bool>;
        { b.isZero() } -> std::same_as<bool>;
        { b.getIndex() } -> std::same_as<uint_fast64_t>;
        { b.getLevel() } -> std::same_as<uint_fast64_t>;

        // Export.
        b.exportToDot(file, names, bool{});
        b.exportToText(file);

        // Conversion to ADD and to explicit data structures (all driven by an ODD).
        { b.template toAdd<ValueType>() } -> std::same_as<AddType>;
        { std::hash<BddType>()(b) } -> std::convertible_to<std::size_t>;
        // The following could be derived from other operations
        { b.toVector(odd, indices) } -> std::same_as<storm::storage::BitVector>;
        {
            b.toExpression(em)
        } -> std::same_as<std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>>>;
        { b.createOdd(indices) } -> std::same_as<storm::dd::Odd>;
        b.template filterExplicitVector<ValueType>(odd, indices, src, dst);
        b.filterExplicitVector(odd, indices, bits, outBits);
        { b.splitIntoGroups(indices) } -> std::same_as<std::vector<BddType>>;
    };

/*!
 * Contract of InternalAdd<T, ValueType>.
 * `BddType` is the matching InternalBdd, `ManagerType` the matching InternalDdManager, `IteratorType` the matching AddIterator and `GenericManagerType` the
 * generic DdManager of the same DdType.
 */
template<typename AddType, typename BddType, typename ManagerType, typename ValueType, typename IteratorType, typename GenericManagerType>
concept InternalAddBackend =
    std::copyable<AddType> && std::default_initializable<AddType> && std::equality_comparable<AddType> &&
    requires(AddType const& a, AddType const& other, AddType& ma, BddType const& bdd, std::vector<BddType> const& vars, ValueType const& value,
             ValueType const& precision, std::vector<uint_fast64_t> const& indices, std::vector<uint_fast64_t> const& offsets, storm::dd::Odd const& odd,
             storm::storage::BitVector const& bits, std::vector<ValueType>& target, std::vector<ValueType> const& vec,
             std::function<ValueType(ValueType const&, ValueType const&)> const& op, std::function<void(uint_fast64_t, ValueType const&)> const& visitor,
             std::string const& file, std::vector<std::string> const& names, GenericManagerType const& manager,
             std::set<storm::expressions::Variable> const& metaVariables) {
        // Arithmetic.
        { a + other } -> std::same_as<AddType>;
        { ma += other } -> std::same_as<AddType&>;
        { a * other } -> std::same_as<AddType>;
        { ma *= other } -> std::same_as<AddType&>;
        { a - other } -> std::same_as<AddType>;
        { ma -= other } -> std::same_as<AddType&>;
        { a / other } -> std::same_as<AddType>;
        { ma /= other } -> std::same_as<AddType&>;
        { a.pow(other) } -> std::same_as<AddType>;
        { a.mod(other) } -> std::same_as<AddType>;
        { a.logxy(other) } -> std::same_as<AddType>;
        { a.floor() } -> std::same_as<AddType>;
        { a.ceil() } -> std::same_as<AddType>;
        { a.minimum(other) } -> std::same_as<AddType>;
        { a.maximum(other) } -> std::same_as<AddType>;

        // Comparisons (result is a BDD).
        { a.equals(other) } -> std::same_as<BddType>;
        { a.notEquals(other) } -> std::same_as<BddType>;
        { a.less(other) } -> std::same_as<BddType>;
        { a.lessOrEqual(other) } -> std::same_as<BddType>;
        { a.greater(other) } -> std::same_as<BddType>;
        { a.greaterOrEqual(other) } -> std::same_as<BddType>;
        { a.less(value) } -> std::same_as<BddType>;
        { a.lessOrEqual(value) } -> std::same_as<BddType>;
        { a.greater(value) } -> std::same_as<BddType>;
        { a.greaterOrEqual(value) } -> std::same_as<BddType>;
        { a.notZero() } -> std::same_as<BddType>;
        { a.equalModuloPrecision(other, precision, bool{}) } -> std::same_as<bool>;

        // Abstraction and variable manipulation.
        { a.sumAbstract(bdd) } -> std::same_as<AddType>;
        { a.minAbstract(bdd) } -> std::same_as<AddType>;
        { a.maxAbstract(bdd) } -> std::same_as<AddType>;
        { a.minAbstractRepresentative(bdd) } -> std::same_as<BddType>;
        { a.maxAbstractRepresentative(bdd) } -> std::same_as<BddType>;
        { a.swapVariables(vars, vars) } -> std::same_as<AddType>;
        { a.permuteVariables(vars, vars) } -> std::same_as<AddType>;
        { a.multiplyMatrix(other, vars) } -> std::same_as<AddType>;
        { a.multiplyMatrix(bdd, vars) } -> std::same_as<AddType>;
        { a.constrain(other) } -> std::same_as<AddType>;
        { a.restrict(other) } -> std::same_as<AddType>;
        { a.getSupport() } -> std::same_as<BddType>;

        // Inspection.
        { a.getNonZeroCount(uint_fast64_t{}) } -> std::same_as<uint_fast64_t>;
        { a.getLeafCount() } -> std::same_as<uint_fast64_t>;
        { a.getNodeCount() } -> std::same_as<uint_fast64_t>;
        { a.getMin() } -> std::same_as<ValueType>;
        { a.getMax() } -> std::same_as<ValueType>;
        { a.getValue() } -> std::same_as<ValueType>;
        { a.isOne() } -> std::same_as<bool>;
        { a.isZero() } -> std::same_as<bool>;
        { a.isConstant() } -> std::same_as<bool>;
        { a.getIndex() } -> std::same_as<uint_fast64_t>;
        { a.getLevel() } -> std::same_as<uint_fast64_t>;
        { a.getStringId() } -> std::same_as<std::string>;

        // Export.
        a.exportToDot(file, names, bool{});
        a.exportToText(file);

        // Iteration over (valuation, value) pairs.
        { a.begin(manager, bdd, uint_fast64_t{}, metaVariables, bool{}) } -> std::same_as<IteratorType>;
        { a.end(manager) } -> std::same_as<IteratorType>;

        // Conversion to and from explicit data structures (all driven by an ODD).
        { a.getInternalDdManager() } -> std::convertible_to<ManagerType const&>;
        // The following could be derived from other operations
        { AddType::fromVector(std::declval<ManagerType const*>(), vec, odd, indices) } -> std::same_as<AddType>;
        { a.createOdd(indices) } -> std::same_as<storm::dd::Odd>;
        a.composeWithExplicitVector(odd, indices, target, op);
        a.composeWithExplicitVector(odd, indices, offsets, target, op);
        a.forEach(odd, indices, visitor);
        { a.splitIntoGroups(indices) } -> std::same_as<std::vector<AddType>>;
        { a.decodeGroupLabels(indices, bits) } -> std::same_as<std::vector<uint64_t>>;
    };

/*!
 * Contract of InternalDdManager<T>.
 * `BddType` is the matching InternalBdd, `AddType` the matching InternalAdd<T, ValueType>, `EnvironmentType` the backend's environment type
 * (storm::DdEnvironmentSelector<T>::type).
 */
template<typename ManagerType, typename BddType, typename AddType, typename EnvironmentType, typename ValueType>
concept InternalDdManagerBackend =
    std::constructible_from<ManagerType, EnvironmentType const&> && requires(ManagerType const& m, ManagerType& mm, std::function<void()> const& f) {
        { m.getBddOne() } -> std::same_as<BddType>;
        { m.getBddZero() } -> std::same_as<BddType>;
        { m.template getAddOne<ValueType>() } -> std::same_as<AddType>;
        { m.template getAddZero<ValueType>() } -> std::same_as<AddType>;
        { m.template getConstant<ValueType>(ValueType{}) } -> std::same_as<AddType>;
        { mm.createDdVariables(uint_fast64_t{}, std::declval<boost::optional<uint_fast64_t> const&>()) } -> std::same_as<std::vector<BddType>>;
        { m.supportsOrderedInsertion() } -> std::same_as<bool>;
        { m.getNumberOfDdVariables() } -> std::same_as<uint_fast64_t>;
        m.execute(f);
        // Could be derived from other operations
        { m.getBddEncodingLessOrEqualThan(uint_fast64_t{}, std::declval<BddType const&>(), uint_fast64_t{}) } -> std::same_as<BddType>;

        // These are present on every backend, but only functional if the corresponding trait is set.
        mm.allowDynamicReordering(bool{});
        { m.isDynamicReorderingAllowed() } -> std::same_as<bool>;
        mm.triggerReordering();
        m.debugCheck();
    };

/*!
 * Contract of AddIterator<T, ValueType>: a single-pass input iterator over (valuation, value) pairs.
 */
template<typename IteratorType>
concept AddIteratorBackend = std::default_initializable<IteratorType> && std::movable<IteratorType> && !std::copy_constructible<IteratorType> &&
                             requires(IteratorType& it, IteratorType const& cit) {
                                 ++it;
                                 *cit;
                                 { cit == cit } -> std::convertible_to<bool>;
                                 { cit != cit } -> std::convertible_to<bool>;
                             };

}  // namespace dd
}  // namespace storm
