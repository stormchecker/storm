#pragma once

#include <boost/container/flat_set.hpp>
#include <sstream>

namespace storm {
namespace storage {

/*!
 * Redefinition of flat_set was needed, because from Boost 1.70 on the default allocator is set to void.
 */
template<typename Key>
using FlatSet = boost::container::flat_set<Key, std::less<Key>, boost::container::new_allocator<Key>>;

/*!
 * Output vector as string.
 *
 * @param vector Vector to output.
 * @return String containing the representation of the vector.
 */
template<typename ValueType>
std::string toString(FlatSet<ValueType> const& set) {
    std::stringstream stream;
    stream << "flatset  { ";
    if (!set.empty()) {
        for (auto const& entry : set) {
            stream << entry << " ";
        }
    }
    stream << " }";
    return stream.str();
}

}  // namespace storage
}  // namespace storm
