#pragma once

#include <cstdint>
#include <ostream>

#include "storm/storage/expressions/BinaryRelationType.h"

namespace storm {
namespace analysis {

/*!
 * A candidate relation between two states of a reachability order: either state1 > state2
 * (Greater) or state1 == state2 (Equal). Only these two relation types are supported.
 */
struct Assumption {
    uint64_t state1;
    uint64_t state2;
    storm::expressions::RelationType relation;
};

std::ostream& operator<<(std::ostream& out, Assumption const& assumption);

}  // namespace analysis
}  // namespace storm
