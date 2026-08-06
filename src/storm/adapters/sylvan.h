#pragma once

#include <cstdint>

#include "storm-config.h"

#ifdef STORM_HAVE_SYLVAN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"

#include "sylvan_obj.hpp"
#include "sylvan_storm_rational_function.h"
#include "sylvan_storm_rational_number.h"

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

#define cas(ptr, old, new) (__sync_bool_compare_and_swap((ptr), (old), (new)))
#define ATOMIC_READ(x) (*(volatile decltype(x) *)&(x))

namespace storm {
namespace dd {

/*!
 * Retrieves whether the topmost variable in the BDD is the one with the given index.
 *
 * @param node The top node of the BDD.
 * @param variableIndex The variable index.
 * @param offset An offset that is applied to the index of the top variable in the BDD.
 * @return True iff the BDD's top variable has the given index.
 */
bool sylvan_bdd_matches_variable_index(BDD node, uint64_t variableIndex, int64_t offset = 0);

/*!
 * Retrieves whether the topmost variable in the MTBDD is the one with the given index.
 *
 * @param node The top node of the BDD.
 * @param variableIndex The variable index.
 * @param offset An offset that is applied to the index of the top variable in the BDD.
 * @return True iff the BDD's top variable has the given index.
 */
bool sylvan_mtbdd_matches_variable_index(MTBDD node, uint64_t variableIndex, int64_t offset = 0);

}  // namespace dd
}  // namespace storm

#endif
