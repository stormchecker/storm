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

#ifndef CPPHOAFPARSER_IMPLICITEDGEHELPER_H
#define CPPHOAFPARSER_IMPLICITEDGEHELPER_H

#include "cpphoafparser/consumer/hoa_consumer_exception.hh"

#include <exception>
#include <string>

namespace cpphoafparser {

/**
 * Helper for keeping track of implicit edges.
 *
 * In `notifyBodyStart()`, call:<br>
 *   `helper = new ImplicitEdgeHelper(numberOfAPs);`
 *
 * In `addState()`, call:<br>
 *   `helper.startOfState(stateId);`
 *
  * In `addEdgeImplicit()`, call:<br>
 *   edgeIndex = helper.nextImplicitEdge();`
 *
 * In `notifyEndOfState()`, call:<br>
 *   `helper.endOfState();`
 */
class ImplicitEdgeHelper
{
public:

  /** Constructor, pass number of atomic propositions */
  ImplicitEdgeHelper(unsigned int numberOfAPs) :
      numberOfAPs(numberOfAPs), stateId(0)
  {
    edgesPerState = ((std::size_t)1) << numberOfAPs;
  }

  /** Return the expected number of edges per state, i.e., 2^|AP|. */
  std::size_t getEdgesPerState()
  {
    return edgesPerState;
  }

  /** Notify the ImplicitEdgeHelper that a new state definition has been encountered.
   * @param newStateId the state index (for error message)
   * @throws std::logic_error if `startOfState()` is called before another state is finished with a call to `endOfState()
   */
  void startOfState(unsigned int newStateId)
  {
    if (inState) {
      throw std::logic_error("ImplicitEdgeHelper: startOfState("
          + std::to_string(newStateId)
          + ") without previous call to endOfState()");
    }

    edgeIndex = 0;
    stateId = newStateId;
    inState = true;
  }

  /** Notify the ImplicitEdgeHelper that the next implicit edge for the current state has been encountered.
   * Return the edge index.
   * @return the index of the current edge
   * @throws HOAConsumerException if the number of edges is more than 2^numberOfAPs
   * @throws std::logic_error if this function is called without a previous call to `startOfState()
   *  */
  std::size_t nextImplicitEdge()
  {
    if (!inState) {
      throw std::logic_error("ImplicitEdgeHelper: nextImplicitEdge() without previous call to startOfState()");
    }

    unsigned long currentEdgeIndex = edgeIndex;
    edgeIndex++;

    if (currentEdgeIndex >= edgesPerState) {
      throw HOAConsumerException("For state "
          +std::to_string(stateId)
          +", more edges than allowed for "
          +std::to_string(numberOfAPs)
          +" atomic propositions");
    }

    return currentEdgeIndex;
  }

  /** Notify the ImplicitEdgeHelper that the current state definition is finished.
   * Checks the number of encountered implicit edges against the expected number.
   * If there are no implicit edges for the current state, that's fine as well.
   * @throws HOAConsumerException if there are more then zero and less then 2^numberOfAPs edges
   * @throws std::logic_error if `endOfState()` is called without previous call to `startOfState()`
   */
  void endOfState()
  {
    if (!inState) {
      throw std::logic_error("ImplicitEdgeHelper: endOfState() without previous call to startOfState()");
    }

    inState = false;

    if (edgeIndex >0 && edgeIndex != edgesPerState) {
      throw HOAConsumerException("For state "
          + std::to_string(stateId)
          + ", less edges than required for "
          + std::to_string(numberOfAPs)
          + " atomic propositions");
    }
  }

private:
  /** The number of atomic propositions */
  unsigned int numberOfAPs;

  /** The index of the current edge */
  std::size_t edgeIndex = 0;

  /** The required number of edges per state */
  std::size_t edgesPerState;

  /** Are we currently in a state definition? */
  bool inState = false;

  /** The current state index (for error message) */
  std::size_t stateId = 0;

};

}
#endif
