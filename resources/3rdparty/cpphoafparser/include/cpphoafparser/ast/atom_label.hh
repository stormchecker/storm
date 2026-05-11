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

#ifndef CPPHOAFPARSER_ATOMLABEL_HH
#define CPPHOAFPARSER_ATOMLABEL_HH

#include <memory>
#include <string>
#include <iostream>

namespace cpphoafparser {

/**
 * Atom of a label expression (either an atomic proposition index or an alias reference)
 */
class AtomLabel {
public:
  /** A shared_ptr wrapping an AtomLabel */
  typedef std::shared_ptr<AtomLabel> ptr;

  /** Static constructor for an atomic proposition atom */
  static AtomLabel::ptr createAPIndex(unsigned int apIndex) {
    return AtomLabel::ptr(new AtomLabel(apIndex));
  }

  /** Static constructor for an alias reference (name has to be without leading @) */
  static AtomLabel::ptr createAlias(const std::string name) {
    return AtomLabel::ptr(new AtomLabel(name));
  }

  /** Copy constructor. */
  AtomLabel(const AtomLabel& other) : apIndex(0), aliasName(nullptr) {
    if (other.isAlias()) {
      aliasName = std::shared_ptr<std::string>(new std::string(*other.aliasName));
    } else {
      apIndex = other.apIndex;
    }
  }

  /** Returns true if this atom is an alias reference */
  bool isAlias() const {return (bool)aliasName;}

  /**
   * Returns the alias name (for an alias reference atom).
   * May only be called if `isAlias() == true`.
   */
  const std::string& getAliasName() const {
    if (!isAlias()) {throw std::logic_error("Illegal access");}
    return *aliasName;
  }

  /**
   * Returns the atomic proposition index (for an AP atom).
   * May only be called if `isAlias() == false`.
   */
  unsigned int getAPIndex() const {
    if (isAlias()) {throw std::logic_error("Illegal access");}
    return apIndex;
  }

  /** Output operator, renders in HOA syntax */
  friend std::ostream& operator<<(std::ostream& out, const AtomLabel& atom) {
    if (atom.isAlias()) {
      out << "@" << *atom.aliasName;
    } else {
      out << atom.apIndex;
    }
    return out;
  }  

  /** Equality operator. */
  bool operator==(const AtomLabel& other) const {
    if (isAlias()) {
      return other.isAlias() && getAliasName() == other.getAliasName();
    } else {
      return !other.isAlias() && getAPIndex() == other.getAPIndex();
    }
  }


private:
  /** The AP index (if applicable) */
  unsigned int apIndex;
  /** The aliasName (empty pointer if AP atom) */
  std::shared_ptr<std::string> aliasName;

  /** Private constructor (AP atom) */
  AtomLabel(unsigned int apIndex) : apIndex(apIndex), aliasName(nullptr) {}
  /** Private constructor (alias reference atom) */
  AtomLabel(const std::string& name) : apIndex(0), aliasName(new std::string(name)) {}
};

}

#endif
