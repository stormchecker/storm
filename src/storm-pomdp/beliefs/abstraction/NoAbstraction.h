#pragma once

#include <type_traits>

namespace storm::pomdp::beliefs {

/*!
 * Auxiliary struct used to identify cases where no abstraction of beliefs shall be applied.
 * Inspired by std::nullopt, see https://en.cppreference.com/w/cpp/utility/optional/nullopt_t
 */
struct NoAbstractionType {
    constexpr explicit NoAbstractionType(int) {}
};
inline constexpr NoAbstractionType NoAbstraction{0};

template<class A>
inline constexpr bool isNoAbstraction = std::is_same_v<std::remove_cv_t<A>, NoAbstractionType>;

}  // namespace storm::pomdp::beliefs
