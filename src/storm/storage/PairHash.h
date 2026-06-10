#ifndef STORM_STORAGE_PAIRHASH_H_
#define STORM_STORAGE_PAIRHASH_H_

#include <boost/functional/hash.hpp>
#include <cstdint>

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

#endif /* STORM_STORAGE_PAIRHASH_H_ */
