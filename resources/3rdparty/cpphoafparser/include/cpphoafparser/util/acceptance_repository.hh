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


#ifndef CPPHOAFPARSER_ACCEPTANCEREPOSITORY_H
#define CPPHOAFPARSER_ACCEPTANCEREPOSITORY_H

#include <stdexcept>
#include <memory>
#include "cpphoafparser/consumer/hoa_consumer.hh"

namespace cpphoafparser {

/**
 * Virtual base class defining the interface to a repository of known acceptance names.
 * This can be used to construct the canonical acceptance expression for a given acc-name header,
 * i.e., the acceptance name (with the extra parameters) for known acceptance names.
 */
class AcceptanceRepository
{
public:
  /** A shared_ptr wrapping an AcceptanceRepository */
  typedef std::shared_ptr<AcceptanceRepository> ptr;
  /** An acceptance condition expression */
  typedef HOAConsumer::acceptance_expr acceptance_expr;

  /** This exception is thrown if the extra arguments of an acc-name are malformed */
  class IllegalArgumentException : public std::runtime_error {
  public:
    /** Constructor */
    IllegalArgumentException(const std::string& s) : std::runtime_error(s) {}
  };

  /** Destructor */
  virtual ~AcceptanceRepository() {}

  /**
   * For a given acc-name header, construct the corresponding canonical acceptance expression.
   * If the acc-name is not known, returns an empty pointer.
   *
   * @param accName the acceptance name, as passed in an acc-name header
   * @param extraInfo extra info, as passed in an acc-name header
   * @return the canonical acceptance expression for this name, empty pointer if not known
   * @throws IllegalParameterException if the acceptance name is known, but there is an error with the extraInfo
   */
  virtual acceptance_expr::ptr getCanonicalAcceptanceExpression(const std::string& accName, const std::vector<IntOrString>& extraInfo) = 0;

};

}

#endif
