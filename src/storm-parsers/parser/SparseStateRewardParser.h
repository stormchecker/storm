#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace storm {
namespace parser {

/*!
 * A class providing the functionality to parse a the state rewards of a model.
 */
template<typename ValueType = double>
class SparseStateRewardParser {
   public:
    /*!
     *	Reads a state reward file and puts the result in a state reward vector.
     *
     *	@param stateCount The number of states.
     *	@param filename The path and name of the state reward file.
     *	@return The created state reward vector.
     */
    static std::vector<ValueType> parseSparseStateReward(uint_fast64_t stateCount, std::string const& filename);
};

}  // namespace parser
}  // namespace storm
