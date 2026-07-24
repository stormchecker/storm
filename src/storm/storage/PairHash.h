#pragma once

#include <boost/functional/hash.hpp>

namespace std {
template<>
struct hash<std::pair<uint_fast64_t, uint_fast64_t>> {
    std::size_t operator()(std::pair<uint_fast64_t, uint_fast64_t> const& key) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, key.first);
        boost::hash_combine(seed, key.second);
        return seed;
    }
};
}  // namespace std
