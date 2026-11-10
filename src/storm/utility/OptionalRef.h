#pragma once

#include <memory>
#include <type_traits>

#include "storm/utility/macros.h"

namespace storm {

/*!
 * Helper to prevent OptionalRef's to rvalue types
 * @see https://en.cppreference.com/w/cpp/utility/functional/reference_wrapper#Possible_implementation
 */

namespace optionalref_detail {
template<class T>
constexpr T& FUN(T& t) noexcept {
    return t;
}
template<class T>
void FUN(T&&) = delete;
}  // namespace optionalref_detail

/*!
 * Auxiliary struct used to identify OptionalRefs that do not contain a reference.
 * Inspired by std::nullopt, see https://en.cppreference.com/w/cpp/utility/optional/nullopt_t
 */
struct NullRefType {
    constexpr explicit NullRefType(int) {}
};
inline constexpr NullRefType NullRef{0};

/*!
 * Helper class that optionally holds a reference to an object of type T.
 * This mimics the interface of std::optional, except that an OptionalRef never takes ownership of an object.
 *
 * @warning An OptionalRef becomes invalid if the lifetime of the referenced object ends prematurely.
 *
 * @note A possible use case is the implementation of optional function arguments, where std::optional would trigger a deep copy of the object.
 *       For example, `foo(storm::OptionalRef<T const> bar = storm::NullRef)` instead of `foo(T const& bar)`
 *
 * @note This class does not provide an operator= as this is prone to errors: should this re-bind the reference or call T::operator= ?
 *       Instead, the `reset` method can be used.
 *
 * @tparam T The type of the referenced object
 */
template<class T>
class OptionalRef {
    static_assert(!std::is_reference_v<T>, "Type of OptionalRef should not be a reference type itself.");

   public:
    using type = T;

    /*!
     * Creates a non-initialized reference.
     */
    OptionalRef() : ptr(nullptr) {}

    /*!
     * Creates a non-initialized reference
     */
    OptionalRef(NullRefType /*NullRef*/) : ptr(nullptr) {}

    /*!
     * Creates a reference to the provided object
     * @param obj the object this will be a reference to
     * @note Exploits template argument deduction so that the class template can be derived from expressions like `OptionalRef(foo)` (even if foo is of type
     * e.g. T&)
     */
    template<class U, class = decltype(optionalref_detail::FUN<T>(std::declval<U>()),
                                       std::enable_if_t<!std::is_same_v<OptionalRef, std::remove_cv_t<std::remove_reference_t<U>>>>())>
    constexpr OptionalRef(U&& u) noexcept(noexcept(optionalref_detail::FUN<T>(std::forward<U>(u))))
        : ptr(std::addressof(optionalref_detail::FUN<T>(std::forward<U>(u)))) {}

    /*!
     * Creates a copy of the given OptionalRef. `this` and `other` will both reference the same object
     */
    OptionalRef(OptionalRef<T> const& other) = default;

    /*!
     * Move constructs this OptionalRef from another one.
     */
    OptionalRef(OptionalRef<T>&& other) = default;

    /*!
     * Deleted assignment operator (see class description)
     */
    OptionalRef& operator=(OptionalRef const& other) = delete;

    /*!
     * Yields true iff this contains a reference
     */
    operator bool() const {
        return ptr != nullptr;
    }

    /*!
     * Yields true iff this contains a reference
     */
    bool has_value() const {
        return ptr != nullptr;
    }

    /*!
     * Accesses the contained reference (if any)
     * @pre this must contain a reference.
     */
    T& operator*() {
        STORM_LOG_ASSERT(has_value(), "OptionalRef operator* called but no object is referenced by this.");
        return *ptr;
    }

    /*!
     * Accesses the contained reference (if any)
     * @pre this must contain a reference.
     */
    T const& operator*() const {
        STORM_LOG_ASSERT(has_value(), "OptionalRef operator* called but no object is referenced by this.");
        return *ptr;
    }

    /*!
     * Accesses the contained reference (if any)
     * @pre this must contain a reference.
     */
    T& value() {
        STORM_LOG_ASSERT(has_value(), "OptionalRef value() called but no object is referenced by this.");
        return *ptr;
    }

    /*!
     * Accesses the contained reference (if any)
     * @pre this must contain a reference.
     */
    T const& value() const {
        STORM_LOG_ASSERT(has_value(), "OptionalRef value called but no object is referenced by this.");
        return *ptr;
    }

    /*!
     * Returns the contained reference (if any). Otherwise, the provided default value is returned.
     */
    T& value_or(T& defaultValue) {
        return has_value() ? value() : defaultValue;
    }

    /*!
     * Returns the contained reference (if any). Otherwise, the provided default value is returned.
     */
    T const& value_or(T const& defaultValue) const {
        return has_value() ? value() : defaultValue;
    }

    /*!
     * Yields a pointer to the referenced object (if any)
     * @pre this must contain a reference.
     */
    T* operator->() {
        STORM_LOG_ASSERT(has_value(), "OptionalRef operator-> called but no object is referenced by this.");
        return ptr;
    }

    /*!
     * Yields a pointer to the referenced object (if any)
     * @pre this must contain a reference.
     */
    T const* operator->() const {
        STORM_LOG_ASSERT(has_value(), "OptionalRef operator-> called but no object is referenced by this.");
        return ptr;
    }

    /*!
     * Unsets the reference. `has_value()` yields false after calling this.
     */
    void reset() {
        ptr = nullptr;
    }

    /*!
     * Unsets the reference. `has_value()` yields false after calling this.
     */
    void reset(NullRefType const&) {
        ptr = nullptr;
    }

    /*!
     * Rebinds the reference. `has_value()` yields true after calling this.
     */
    void reset(T& t) {
        ptr = std::addressof(t);
    }

   private:
    T* ptr;  /// A pointer to the referenced object. Nullptr if no object is referenced.
};

/// deduction guides
template<class T>
OptionalRef(T&) -> OptionalRef<T>;

}  // namespace storm
