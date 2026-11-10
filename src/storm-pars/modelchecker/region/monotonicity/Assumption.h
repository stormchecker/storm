#pragma once

#include <cstdint>
#include <ostream>

namespace storm {
namespace analysis {

/*!
 * The relation a candidate assumption states between two states. Keeping this a dedicated
 * two-valued type makes it impossible to represent (and later interpret inconsistently) any
 * relation other than "state1 > state2" or "state1 == state2".
 */
enum class AssumptionRelation { Greater, Equal };

/*!
 * A candidate relation between two states of a reachability order: either state1 > state2
 * (Greater) or state1 == state2 (Equal).
 */
struct Assumption {
    uint64_t state1;
    uint64_t state2;
    AssumptionRelation relation;
};

std::ostream& operator<<(std::ostream& out, Assumption const& assumption);

}  // namespace analysis
}  // namespace storm
