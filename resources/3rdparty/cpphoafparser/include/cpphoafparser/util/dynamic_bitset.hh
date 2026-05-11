//==============================================================================
//
//  Copyright (c) 2015-
//  Authors:
//  * Joachim Klein <klein@tcs.inf.tu-dresden.de>
//  * David Mueller <david.mueller@tcs.inf.tu-dresden.de>
//
//------------------------------------------------------------------------------
//
//  This file is part of the cpphoafparser library,
//      http://automata.tools/hoa/cpphoafparser/
//
//  The cpphoafparser library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  The cpphoafparser library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
//==============================================================================

#ifndef CPPHOAFPARSER_DYNAMICBITSET_H
#define CPPHOAFPARSER_DYNAMICBITSET_H

#include <vector>

namespace cpphoafparser {

/**
 * A dynamic bitset that grows automatically.
 * Inherits the functionality of std::vector<bool>, adding
 * convenience functions.
 **/
class dynamic_bitset : public std::vector<bool> {
public:

  /**
   * Set the bit at the given `index` to `value`.
   * It `index` is larger than the highest set bit,
   * the intermediate bits are set to `false`.
   */
  void set(std::size_t index, bool value=true) {
    if (index >= size()) {
      resize(index+1, false);
    }

    at(index) = value;
  }

  /**
   * Get the bit at the given `index`.
   * If `index` is beyond the highest set bit
   * then return false.
   */
  bool get(std::size_t index) const {
    if (index >= size()) {
      return false;
    }

    return at(index);
  }

  /**
   * Returns the index of the highest set bit.
   * The actual return value is a pair, where the first
   * element is the index of the highest set bit (if there
   * are any) and the second element being false
   * if there is no set bit at all.
   */
  std::pair<std::size_t,bool> getHighestSetBit() const {
    if (size() == 0) {
      return std::make_pair(0, false);
    }
    for (std::size_t index = size(); index > 0; index--) {
      if ((*this)[index-1]) {
        return std::make_pair(index-1, true);
      }
    }
    return std::make_pair(0, false);
  }

  /**
   * Returns the cardinality (the number of set bits) of this
   * bitset.
   */
  std::size_t cardinality() const {
    std::size_t result = 0;
    for (bool b : *this) {
      if (b) result++;
    }
    return result;
  }

  /**
   * Perform andNot operator on this bitset.
   * After calling this function a bit is set in this bitset
   * if it was set before and if the corresponding bit in
   * `other` is not set.
   */
  void andNot(const dynamic_bitset& other) {
    for (std::size_t index = 0; index < size() && index < other.size(); index++) {
      at(index) = at(index) && !other.at(index);
    }
  }

  /** Returns `true` if there are not set bits */
  bool isEmpty() const {
    for (bool b : *this) {
      if (b) return false;
    }
    return true;
  }

};

}
#endif
