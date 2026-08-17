#pragma once

#include "storm-config.h"

#ifdef STORM_HAVE_CUDD

#include <boost/functional/hash.hpp>

// Include the C++-interface of CUDD.
#include "cuddObj.hh"

namespace storm {
namespace dd {

struct CuddPointerPairHash {
    std::size_t operator()(std::pair<DdNode const*, DdNode const*> const& pair) const {
        std::hash<DdNode const*> hasher;
        std::size_t seed = hasher(pair.first);
        boost::hash_combine(seed, hasher(pair.second));
        return seed;
    }
};

}  // namespace dd
}  // namespace storm
#endif