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

#ifndef CPPHOAFPARSER_HOAPARSERHELPER_H
#define CPPHOAFPARSER_HOAPARSERHELPER_H

#include <iostream>
#include <string>
#include <sstream>

#include "cpphoafparser/parser/hoa_parser_exception.hh"

namespace cpphoafparser {

/** Helper function for dealing with quoted strings */
class HOAParserHelper {
public:
  /**
   * Outputs a quoted version of the string to the ostream.
   * The string is quoted to conform the to double-quoted
   * string syntax of the HOA format.
   */
  static void print_quoted(std::ostream& out, const std::string& s) {
    std::string::size_type pos = s.find_first_of("\\\"");  // find next \ or "

    if (pos == std::string::npos) {
      // nothing to quote
      out << "\"" << s << "\"";
      return;
    }

    out << "\"";
    std::string::size_type last_pos = 0;
    do {
      out << s.substr(last_pos, pos);   // characters that don't need to be quoted
      out << "\\";                      // add quote
      out << s.substr(pos,1);           // add character

      last_pos = pos+1;
      pos = s.find_first_of("\\\"", last_pos);  // find next \ or "
    } while (pos != std::string::npos);
    if (last_pos < s.length()) {
      out << s.substr(last_pos);      // remaining
    }
    out << "\"";    
  }

  /** Returns a double-quoted version of the string (HOA syntax and quoting rules). */
  static std::string quote(const std::string& s) {
    std::stringstream ss;
    print_quoted(ss, s);
    return ss.str();
  }

  /**
   * Performs unquoting of a double-quoted string (HOA syntax and quoting rules).
   * If the string is not a validly quoted string, a HOAParserException is thrown.
   * @returns the unquoted string
   **/
  static std::string unquote(const std::string& s) {
    if (s.length() == 0 || s.at(0) != '"') {
      throw HOAParserException("String not quoted : " + s);
    }
    if (s.back() != '"') {
      throw HOAParserException("String does not end with quote: " + s);
    }
    std::string::size_type pos = s.find_first_of("\\", 1);  // find first '\'

    if (pos == std::string::npos) {
      // nothing to unquote, just remove outer quotes
      return s.substr(1, s.length()-2);
    }

    std::stringstream ss;
    std::string::size_type last_pos = 1;
    do {
      ss << s.substr(last_pos, pos);   // characters that don't need to be quoted
      if (pos+1 >= s.length()) {
        throw HOAParserException("String ends with \\ character: s");
      }
      ss << s.substr(pos+1,1);         // the quoted character
      last_pos = pos+2;                 // skip quote character
      pos = s.find_first_of("\\\"", last_pos);  // find next '\'
    } while (pos != std::string::npos);

    if (last_pos < s.length()-1) {
      ss << s.substr(last_pos, s.length()-1-last_pos);      // remaining
    }

    return ss.str();
  }

};

}

#endif
