#pragma once

#include <cstdint>

#include "storm-config.h"

#ifdef STORM_HAVE_SYLVAN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wzero-length-array"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wdeprecated-register"
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#pragma GCC system_header  // Only way to suppress some warnings atm.

#include "sylvan_obj.hpp"
#include "sylvan_storm_rational_function.h"
#include "sylvan_storm_rational_number.h"

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

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#endif