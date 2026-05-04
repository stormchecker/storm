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


#ifndef CPPHOAFPARSER_ATOMACCEPTANCE_H
#define CPPHOAFPARSER_ATOMACCEPTANCE_H

#include <memory>
#include <iostream>

namespace cpphoafparser {

/**
 * Atom of an acceptance condition, either Fin(accSet), Fin(!accSet), Inf(accSet) or Inf(!accSet)
 */
class AtomAcceptance {
public:
   /** A shared_ptr wrapping an AtomAcceptance */
  typedef std::shared_ptr<AtomAcceptance> ptr;

  /** The type of the temporal operator (Fin / Inf) */
  enum AtomType {TEMPORAL_FIN, TEMPORAL_INF};

  /**
   * Constructor.
   * @param kind the type of the temporal operator (Fin / Inf)
   * @param accSet the acceptance set index
   * @param negated is the acceptance set negated?
   **/
  AtomAcceptance(AtomType kind, unsigned int accSet, bool negated)
  : kind(kind), accSet(accSet), negated(negated) {
  }

  /** Static constructor for a Fin(accSet) atom. */
  static ptr Fin(unsigned int accSet) {
    return ptr(new AtomAcceptance(TEMPORAL_FIN, accSet, false));
  }

  /** Static constructor for a Fin(!accSet) atom. */
  static ptr FinNot(unsigned int accSet) {
    return ptr(new AtomAcceptance(TEMPORAL_FIN, accSet, true));
  }

  /** Static constructor for an Inf(accSet) atom. */
  static ptr Inf(unsigned int accSet) {
    return ptr(new AtomAcceptance(TEMPORAL_INF, accSet, false));
  }

  /** Static constructor for a Inf(!accSet) atom. */
  static ptr InfNot(unsigned int accSet) {
    return ptr(new AtomAcceptance(TEMPORAL_INF, accSet, true));
  }

  /** Get the temporal operator type of this atom. */
  AtomType getType() const {return kind;}
  /** Get the acceptance set index of this atom. */
  unsigned int getAcceptanceSet() const {return accSet;}
  /** Is the acceptance set negated? */
  bool isNegated() const {return negated;}

  /** Output operator, renders atom in HOA syntax */
  friend std::ostream& operator<<(std::ostream& out, const AtomAcceptance& atom) {
    out << (atom.kind == TEMPORAL_FIN ? "Fin" : "Inf");
    out << "(";
    if (atom.negated) out << "!";
    out << atom.accSet;
    out << ")";
    return out;
  }

  /** Equality operator */
  bool operator==(const AtomAcceptance& other) const {
    return
        this->kind == other.kind &&
        this->accSet == other.accSet &&
        this->negated == other.negated;
  }

private:
  /** The temporal operator of this atom */
  AtomType kind;
  /** The acceptance set index of this atom */
  unsigned int accSet;
  /** Is the acceptance set negated? */
  bool negated;
};

}

#endif
