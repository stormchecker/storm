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

#ifndef CPPHOAFPARSER_HOACONSUMERPRINT_H
#define CPPHOAFPARSER_HOACONSUMERPRINT_H

#include <iostream>

#include "cpphoafparser/consumer/hoa_consumer.hh"
#include "cpphoafparser/parser/hoa_parser_helper.hh"

namespace cpphoafparser {

/**
 * A HOAConsumer implementation that provides printing functionality.
 *
 * Outputs the HOA automaton corresponding to the function calls on the given output stream.
 * Can be used as a "pretty-printer", as it will output the various syntax elements with
 * a consistent layout of white-space and line breaks.
 */

class HOAConsumerPrint : public HOAConsumer {
public:

  /** Constructor, providing a reference to the output stream */
  HOAConsumerPrint(std::ostream& out) : out(out) {}

  virtual bool parserResolvesAliases() override {
    return false;
  }

  virtual void notifyHeaderStart(const std::string& version) override {
    out << "HOA: " << version << std::endl;
  }

  virtual void setNumberOfStates(unsigned int numberOfStates) override {
    out << "States: " << numberOfStates << std::endl;
  }

  virtual void addStartStates(const int_list& stateConjunction) override {
    out << "Start: ";
    bool first = true;
    for (unsigned int state : stateConjunction) {
      if (!first) out << " & ";
      first=false;
      out << state;
    }
    out << std::endl;
  }

  virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) override {
    out << "Alias: @" << name << " " << *labelExpr << std::endl;
  }

  virtual void setAPs(const std::vector<std::string>& aps) override {
    out << "AP: " << aps.size();
    for (const std::string& ap : aps) {
      out << " ";
      HOAParserHelper::print_quoted(out, ap);
    }
    out << std::endl;
  }

  virtual void setAcceptanceCondition(unsigned int numberOfSets, acceptance_expr::ptr accExpr) override {
    out << "Acceptance: " << numberOfSets << " " << *accExpr << std::endl;
  }

  virtual void provideAcceptanceName(const std::string& name, const std::vector<IntOrString>& extraInfo) override {
    out << "acc-name: " << name;
    for (const IntOrString& extra : extraInfo) {
      out << " " << extra;
    }
    out << std::endl;
  }

  virtual void setName(const std::string& name) override {
    out << "name: ";
    HOAParserHelper::print_quoted(out, name);
    out << std::endl;
  }

  virtual void setTool(const std::string& name, std::shared_ptr<std::string> version) override {
    out << "tool: ";
    HOAParserHelper::print_quoted(out, name);
    if (version) {
      out << " ";
      HOAParserHelper::print_quoted(out, *version);
    }
    out << std::endl;
  }

  virtual void addProperties(const std::vector<std::string>& properties) override {
    out << "properties:";
    for (const std::string& property : properties) {
      out << " " << property;
    }
    out << std::endl;
  }

  virtual void addMiscHeader(const std::string& name, const std::vector<IntOrString>& content) override {
    out << name << ":";
    for (const IntOrString& extra : content) {
      out << " " << extra;
    }
    out << std::endl;
  }

  virtual void notifyBodyStart() override {
    out << "--BODY--" << std::endl;
  }

  virtual void addState(unsigned int id,
                        std::shared_ptr<std::string> info,
                        label_expr::ptr labelExpr,
                        std::shared_ptr<int_list> accSignature) override {
    out << "State: ";
    if (labelExpr) {
      out << "[" << *labelExpr << "] ";
    }
    out << id;
    if (info) {
      out << " ";
      HOAParserHelper::print_quoted(out, *info);
    }
    if (accSignature) {
      out << " {";
      bool first = true;
      for (unsigned int acc : *accSignature) {
        if (!first) out << " ";
        first = false;
        out << acc;
      }
      out << "}";
    }
    out << std::endl;
  }

  virtual void addEdgeImplicit(unsigned int stateId,
                               const int_list& conjSuccessors,
                               std::shared_ptr<int_list> accSignature) override {
    bool first = true;
    for (unsigned int succ : conjSuccessors) {
      if (!first) out << "&";
      first = false;
      out << succ;
    }
    if (accSignature) {
      out << " {";
      first = true;
      for (unsigned int acc : *accSignature) {
        if (!first) out << " ";
        first = false;
        out << acc;
      }
      out << "}";
    }
    out << std::endl;
  }

  virtual void addEdgeWithLabel(unsigned int stateId,
                                label_expr::ptr labelExpr,
                                const int_list& conjSuccessors,
                                std::shared_ptr<int_list> accSignature) override {
    if (labelExpr) {
      out << "[" << *labelExpr << "] ";
    }

    bool first = true;
    for (unsigned int succ : conjSuccessors) {
      if (!first) out << "&";
      first = false;
      out << succ;
    }

    if (accSignature) {
      out << " {";
      first = true;
      for (unsigned int acc : *accSignature) {
        if (!first) out << " ";
        first = false;
        out << acc;
      }
      out << "}";
    }
    out << std::endl;
  }

  virtual void notifyEndOfState(unsigned int stateId) override {
    // nothing to do
  }

  virtual void notifyEnd() override {
    out << "--END--" << std::endl;
    out.flush();
  }

  virtual void notifyAbort() override {
    out << "--ABORT--" << std::endl;
    out.flush();
  }

  virtual void notifyWarning(const std::string& warning) override {
    std::cerr << "Warning: " << warning << std::endl;
  }

private:
  /** Reference to the output stream */
  std::ostream& out;
};

}

#endif
