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


#ifndef CPPHOAFPARSER_INTORSTRING_H
#define CPPHOAFPARSER_INTORSTRING_H

#include <string>
#include <stdexcept>

#include "cpphoafparser/parser/hoa_parser_helper.hh"

namespace cpphoafparser {

/**
 * Utility class for storing either an integer or a string value.
 * The stored value is immutable.
 */
class IntOrString {
public:
  /** Constructor for storing an integer. */
  IntOrString(unsigned int vInteger) : vInteger(vInteger), vString(nullptr), wasQuoted(false) {}
  /**
   * Constructor for storing a string.
   * @param vString the string to store
   * @param wasQuoted remember that the string was originally quoted
   */
  IntOrString(const std::string& vString, bool wasQuoted = false) : vInteger(0), vString(new std::string(vString)), wasQuoted(wasQuoted) {}

  /** Returns true if the stored value is a string */
  bool isString() const {return (bool)vString;}

  /** Returns true if the stored value is an integer */
  bool isInteger() const {return !isString();}

  /**
   * Returns whether this string was originally quoted.
   * May only be called if `isString() == true`.
   **/
  bool wasStringQuoted() const {
    if (!isString()) {throw std::logic_error("Illegal access");}
    return wasQuoted;
  }

  /**
   * Returns (a const reference to) the stored string.
   * May only be called if `isString() == true`.
   **/
  const std::string& getString() const {
    if (!isString()) {throw std::logic_error("Illegal access");}
    return *vString;
  }

  /**
   * Returns the stored integer.
   * May only be called if `isInteger() == true`.
   **/
  unsigned int getInteger() const {
    if (!isInteger()) {throw std::logic_error("Illegal access");}
    return vInteger;
  }

  /**
   * Stream output operator for IntOrString.
   * If the stored value is a string that is marked as having been quoted,
   * will quote the string before output.
   */
  friend std::ostream& operator<<(std::ostream& out, const IntOrString& intOrString) {
    if (intOrString.isInteger()) {
      out << intOrString.getInteger();
    } else {
      if (intOrString.wasQuoted) {
        HOAParserHelper::print_quoted(out, intOrString.getString());
      } else {
        out << intOrString.getString();
      }
    }
    return out;
  }

private:
  /** The stored integer value */
  unsigned int vInteger;
  /** The stored string value */
  std::shared_ptr<std::string> vString;
  /** Was this string originally quoted? */
  bool wasQuoted;
};

}
#endif
