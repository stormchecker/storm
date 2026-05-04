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

#ifndef CPPHOAFPARSER_HOAPARSEREXCEPTION_H
#define CPPHOAFPARSER_HOAPARSEREXCEPTION_H

#include <stdexcept>
#include <cassert>

namespace cpphoafparser {

/** Exception thrown to indicate an error during parsing, mostly for syntax errors. */
class HOAParserException : public std::runtime_error {
public:
  /** Constructor for simple error message. */
  HOAParserException(const std::string& what) :
    std::runtime_error(what), hasLocation(false), line(0), col(0) {}

  /** Constructor for error message with location (line/column) information. */
  HOAParserException(const std::string& what, int line, int col) :
    std::runtime_error(what), hasLocation(true), line(line), col(col) {}
  
  bool getHasLocation() const {
      return hasLocation;
  }
  
  /** @pre getHasLocation returns true */
  bool getLine() const {
      assert(getHasLocation());
      return line;
  }
  
  /** @pre getHasLocation returns true */
  bool getCol() const {
      assert(getHasLocation());
      return col;
  }
  
private:
  /** True if we have location information */
  bool hasLocation;
  /** The line number */
  unsigned int line;
  /** The column number */
  unsigned int col;
};

}

#endif
