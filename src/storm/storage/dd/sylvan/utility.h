#pragma once

#include "storm-config.h"

#ifdef STORM_HAVE_SYLVAN
#include <boost/functional/hash.hpp>

#include "storm/adapters/sylvan.h"

namespace storm {
namespace dd {

struct SylvanMTBDDPairHash {
    std::size_t operator()(std::pair<MTBDD, MTBDD> const& pair) const {
        std::hash<MTBDD> hasher;
        std::size_t seed = hasher(pair.first);
        boost::hash_combine(seed, hasher(pair.second));
        return seed;
    }
};

struct SylvanMTBDDPairLess {
    std::size_t operator()(std::pair<MTBDD, MTBDD> const& a, std::pair<MTBDD, MTBDD> const& b) const {
        if (a.first < b.first) {
            return true;
        } else if (a.first == b.first && a.second < b.second) {
            return true;
        }
        return false;
    }
};

}  // namespace dd
}  // namespace storm
#endif