#include "storm/adapters/sylvan.h"

#ifdef STORM_HAVE_SYLVAN
namespace storm {
namespace dd {

bool sylvan_bdd_matches_variable_index(BDD node, uint64_t variableIndex, int64_t offset) {
    return !sylvan_isconst(node) && static_cast<uint64_t>(sylvan_var(node) + offset) == variableIndex;
}

bool sylvan_mtbdd_matches_variable_index(MTBDD node, uint64_t variableIndex, int64_t offset) {
    return !mtbdd_isleaf(node) && static_cast<uint64_t>(sylvan_var(node) + offset) == variableIndex;
}

}  // namespace dd
}  // namespace storm
#endif
