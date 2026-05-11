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


#ifndef CPPHOAFPARSER_ACCEPTANCEREPOSITORYSTANDARD_H
#define CPPHOAFPARSER_ACCEPTANCEREPOSITORYSTANDARD_H

#include "cpphoafparser/util/acceptance_repository.hh"

namespace cpphoafparser {

/**
 * An acceptance repository with all the acceptance conditions specified
 * in the format specification.
 */
class AcceptanceRepositoryStandard : public AcceptanceRepository
{
public:

  /**
   * @name acc-name constants
   * String constants for the various acceptance condition names */
  /**@{*/
  const char* ACC_ALL = "all";
  const char* ACC_NONE = "none";

  const char* ACC_BUCHI = "Buchi";
  const char* ACC_COBUCHI = "coBuchi";

  const char* ACC_GENERALIZED_BUCHI = "generalized-Buchi";
  const char* ACC_GENERALIZED_COBUCHI = "generalized-coBuchi";

  const char* ACC_RABIN = "Rabin";
  const char* ACC_STREETT = "Streett";

  const char* ACC_GENERALIZED_RABIN = "generalized-Rabin";

  const char* ACC_PARITY = "parity";
  /**@}*/

  virtual acceptance_expr::ptr getCanonicalAcceptanceExpression(const std::string& accName,
                                                                const std::vector<IntOrString>& extraInfo) override {
    if (accName == ACC_ALL)
      return forAll(extraInfo);
    if (accName == ACC_NONE)
      return forNone(extraInfo);

    if (accName == ACC_BUCHI)
      return forBuchi(extraInfo);
    if (accName == ACC_COBUCHI)
      return forCoBuchi(extraInfo);

    if (accName == ACC_GENERALIZED_BUCHI)
      return forGenBuchi(extraInfo);
    if (accName == ACC_GENERALIZED_COBUCHI)
      return forGenCoBuchi(extraInfo);

    if (accName == ACC_RABIN)
      return forRabin(extraInfo);
    if (accName == ACC_STREETT)
      return forStreett(extraInfo);
    if (accName == ACC_GENERALIZED_RABIN)
          return forGeneralizedRabin(extraInfo);

    if (accName == ACC_PARITY)
          return forParity(extraInfo);

    return acceptance_expr::ptr();
  }

  /** Construct canonical acceptance condition for 'all' */
  acceptance_expr::ptr forAll(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_ALL, extraInfo, 0);

    return acceptance_expr::True();
  }

  /** Construct canonical acceptance condition for 'none' */
  acceptance_expr::ptr forNone(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_ALL, extraInfo, 0);

    return acceptance_expr::False();
  }

  /** Construct canonical acceptance condition for 'Buchi' */
  acceptance_expr::ptr forBuchi(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_BUCHI, extraInfo, 0);

    return acceptance_expr::Atom(AtomAcceptance::Inf(0));
  }

  /** Construct canonical acceptance condition for 'coBuchi' */
  acceptance_expr::ptr forCoBuchi(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_COBUCHI, extraInfo, 0);

    return acceptance_expr::Atom(AtomAcceptance::Fin(0));
  }

  /** Construct canonical acceptance condition for 'generalized-Buchi' */
  acceptance_expr::ptr forGenBuchi(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_GENERALIZED_BUCHI, extraInfo, 1);
    unsigned int numberOfInf = extraInfoToIntegerList(ACC_GENERALIZED_BUCHI, extraInfo).at(0);

    if (numberOfInf == 0) {
      return acceptance_expr::True();
    }

    acceptance_expr::ptr result;
    for (unsigned int i = 0; i < numberOfInf; i++) {
      acceptance_expr::ptr inf = acceptance_expr::Atom(AtomAcceptance::Inf(i));

      if (i == 0) {
        result = inf;
      } else {
        result = result & inf;
      }
    }

    return result;
  }

  /** Construct canonical acceptance condition for 'generalized-co-Buchi' */
  acceptance_expr::ptr forGenCoBuchi(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_GENERALIZED_COBUCHI, extraInfo, 1);
    unsigned int numberOfFin = extraInfoToIntegerList(ACC_GENERALIZED_COBUCHI, extraInfo).at(0);

    if (numberOfFin == 0) {
      return acceptance_expr::False();
    }

    acceptance_expr::ptr result;
    for (unsigned int i = 0; i < numberOfFin; i++) {
      acceptance_expr::ptr fin = acceptance_expr::Atom(AtomAcceptance::Fin(i));

      if (i == 0) {
        result = fin;
      } else {
        result = result & fin;
      }
    }

    return result;
  }

  /** Construct canonical acceptance condition for 'Rabin' */
  acceptance_expr::ptr forRabin(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_RABIN, extraInfo, 1);
    unsigned int numberOfPairs = extraInfoToIntegerList(ACC_RABIN, extraInfo).at(0);

    if (numberOfPairs == 0) {
      return acceptance_expr::False();
    }

    acceptance_expr::ptr result;
    for (unsigned int i = 0; i < numberOfPairs; i++) {
      acceptance_expr::ptr fin = acceptance_expr::Atom(AtomAcceptance::Fin(2*i));
      acceptance_expr::ptr inf = acceptance_expr::Atom(AtomAcceptance::Inf(2*i+1));

      acceptance_expr::ptr pair = fin & inf;

      if (i == 0) {
        result = pair;
      } else {
        result = result | pair;
      }
    }

    return result;
  }

  /** Construct canonical acceptance condition for 'Streett' */
  acceptance_expr::ptr forStreett(const std::vector<IntOrString>& extraInfo) {
    checkNumberOfArguments(ACC_STREETT, extraInfo, 1);
    unsigned int numberOfPairs = extraInfoToIntegerList(ACC_STREETT, extraInfo).at(0);

    if (numberOfPairs == 0) {
      return acceptance_expr::True();
    }

    acceptance_expr::ptr result;
    for (unsigned int i = 0; i < numberOfPairs; i++) {
      acceptance_expr::ptr fin = acceptance_expr::Atom(AtomAcceptance::Fin(2*i));
      acceptance_expr::ptr inf = acceptance_expr::Atom(AtomAcceptance::Inf(2*i+1));

      acceptance_expr::ptr pair = fin | inf;

      if (i == 0) {
        result = pair;
      } else {
        result = result & pair;
      }
    }

    return result;
  }

  /** Construct canonical acceptance condition for 'generalized-Rabin' */
  acceptance_expr::ptr forGeneralizedRabin(const std::vector<IntOrString>& extraInfo) {
    std::vector<unsigned int> parameters = extraInfoToIntegerList(ACC_GENERALIZED_RABIN, extraInfo);

    if (parameters.size() == 0) {
      throw IllegalArgumentException(std::string("Acceptance ")+ACC_GENERALIZED_RABIN+" needs at least one argument");
    }

    unsigned int numberOfPairs = parameters.at(0);
    if (parameters.size() != numberOfPairs + 1) {
      throw IllegalArgumentException(std::string("Acceptance ")+ACC_GENERALIZED_RABIN+" with " + std::to_string(numberOfPairs) +" generalized pairs: There is not exactly one argument per pair");
    }

    acceptance_expr::ptr result;
    int currentIndex = 0;
    for (unsigned int i = 0; i < numberOfPairs; i++) {
      unsigned int numberOfInf = parameters.at(i+1);

      acceptance_expr::ptr fin = acceptance_expr::Atom(AtomAcceptance::Fin(currentIndex++));
      acceptance_expr::ptr pair = fin;
      for (unsigned int j = 0; j< numberOfInf; j++) {
        acceptance_expr::ptr inf = acceptance_expr::Atom(AtomAcceptance::Inf(currentIndex++));
        pair = pair & inf;
      }

      if (i == 0) {
        result = pair;
      } else {
        result = result | pair;
      }
    }

    return result;
  }

  /** Construct canonical acceptance condition for 'parity' */
    acceptance_expr::ptr forParity(const std::vector<IntOrString>& extraInfo) {
      checkNumberOfArguments(ACC_PARITY, extraInfo, 3);

    bool min = false;
    bool even = false;

    const std::string& minMax = extraInfoToString(ACC_PARITY, extraInfo, 0);
    if (minMax == "min") {
      min = true;
    } else if (minMax == "max") {
      min = false;
    } else {
      throw IllegalArgumentException(std::string("For acceptance ") + ACC_PARITY +", the first argument has to be either 'min' or 'max'");
    }
    const std::string& evenOdd = extraInfoToString(ACC_PARITY, extraInfo, 1);
    if (evenOdd == "even") {
      even = true;
    } else if (evenOdd == "odd") {
      even = false;
    } else {
      throw IllegalArgumentException(std::string("For acceptance ") + ACC_PARITY +", the second argument has to be either 'even' or 'odd'");
    }

    unsigned int colors;
    if (!(extraInfo.at(2).isInteger())) {
      throw IllegalArgumentException(std::string("For acceptance ") + ACC_PARITY + ", the third argument has to be the number of colors");
    }
    colors = extraInfo.at(2).getInteger();

    if (colors == 0) {
      if ( min &&  even) return BooleanExpression<AtomAcceptance>::True();
      if (!min &&  even) return BooleanExpression<AtomAcceptance>::False();
      if ( min && !even) return BooleanExpression<AtomAcceptance>::False();
      if (!min && !even) return BooleanExpression<AtomAcceptance>::True();
    }

    acceptance_expr::ptr result;

    bool reversed = min;
    bool infOnOdd = !even;

    for (unsigned int i = 0; i < colors; i++) {
      unsigned int color = (reversed ? colors-i-1 : i);

      bool produceInf;
      if (color % 2 == 0) {
        produceInf = !infOnOdd;
      } else {
        produceInf = infOnOdd;
      }

      acceptance_expr::ptr node;
      if (produceInf) {
        node.reset(new BooleanExpression<AtomAcceptance>(AtomAcceptance::Inf(color)));
      } else {
        node.reset(new BooleanExpression<AtomAcceptance>(AtomAcceptance::Fin(color)));
      }

      if (result) {
        if (produceInf) {
          // Inf always with |
          result = node | result;
        } else {
          // Fin always with &
          result = node & result;
        }
      } else {
        result = node;
      }
    }

    return result;
  }

  private:

  /**
   * Convert an extra info vector to an integer list.
   * @param accName the name of the acceptance condition (for error messages)
   * @param extraInfo the vector
   */
  std::vector<unsigned int> extraInfoToIntegerList(const std::string& accName, const std::vector<IntOrString>& extraInfo)
  {
    std::vector<unsigned int> result;
    for (IntOrString i : extraInfo) {
      if (!(i.isInteger())) {
        throw IllegalArgumentException("For acceptance " + accName + ", all arguments have to be integers");
      }

      result.push_back(i.getInteger());
    }

    return result;
  }

  /** Extract a string from extraInfo at a given index, throw exception otherwise.
   * @param accName the name of the acceptance condition (for error messages)
   * @param extraInfo the vector
   * @param index the index into the extraInfo vector
   */
  const std::string& extraInfoToString(const std::string& accName, const std::vector<IntOrString>& extraInfo, unsigned int index) {
    if (!extraInfo.at(index).isString()) {
      throw IllegalArgumentException(std::string("Argument ")+std::to_string(index-1)+" for acceptance " + accName +" has to be a string!");
    }
    return extraInfo.at(index).getString();
  }

  /**
   * Check that the number of arguments in the extraInfo vector is as expected.
   * If that's not the case, throw an IllegalArgumentException.
   * @param accName the acceptance condition name (for error message)
   * @param extraInfo the extraInfo vector
   * @param expectedNumberOfArguments the expected number of elements in the extraInfo vector
   */
  void checkNumberOfArguments(const std::string& accName, const std::vector<IntOrString>& extraInfo, unsigned int expectedNumberOfArguments)
  {
    if (expectedNumberOfArguments != extraInfo.size()) {
      throw IllegalArgumentException("For acceptance "
          + accName
          + ", expected "
          + std::to_string(expectedNumberOfArguments)
          + " arguments, got "
          + std::to_string(extraInfo.size()));
    }
  }

};

}

#endif
