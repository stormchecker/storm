#pragma once

#include <string>
#include <vector>

namespace storm::dft {

// Forward declaration
namespace storage {
template<typename ValueType>
class DFT;
}

namespace parser {

/*!
 * Parser for BE order from text file.
 */
template<typename ValueType>
class BEOrderParser {
   public:
    /*!
     * Parse BE order from given file.
     * The BE order is given as a single-line whitespace-separated list of BE names.
     *
     * @param filename File.
     * @param dft DFT.
     *
     * @return List of BE ids corresponding to the BEs in the given DFT.
     */
    static std::vector<size_t> parseBEOrder(std::string const& filename, storm::dft::storage::DFT<ValueType> const& dft);
};

}  // namespace parser
}  // namespace storm::dft
