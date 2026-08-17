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

#ifndef CPPHOAFPARSER_HOAINTERMEDIATE_TRACE_H
#define CPPHOAFPARSER_HOAINTERMEDIATE_TRACE_H

#include <iostream>

#include "cpphoafparser/consumer/hoa_intermediate.hh"

namespace cpphoafparser {

/**
 * Traces the function calls to HOAConsumer, prints function name and arguments to stream.
 */
class HOAIntermediateTrace : public HOAIntermediate {
public:

  /** Constructor, providing the `next` HOAConsumer */
  HOAIntermediateTrace(HOAConsumer::ptr next) : HOAIntermediate(next), out(std::cout) {}

  /** Constructor, providing the `next` HOAConsumer */
  HOAIntermediateTrace(HOAConsumer::ptr next, std::ostream& out) : HOAIntermediate(next), out(out) {}

  virtual bool parserResolvesAliases() override {
    return next->parserResolvesAliases();
  }

  virtual void notifyHeaderStart(const std::string& version) override {
    traceFunction("notifyHeaderStart");
    traceArgument("version", version);

    next->notifyHeaderStart(version);
  }

  virtual void setNumberOfStates(unsigned int numberOfStates) override {
    traceFunction("setNumberOfStates");
    traceArgument("numberOfStates", numberOfStates);

    next->setNumberOfStates(numberOfStates);
  }

  virtual void addStartStates(const int_list& stateConjunction) override {
    traceFunction("addStartStates");
    traceArgument("stateConjunction", stateConjunction);

    next->addStartStates(stateConjunction);
  }

  virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) override {
    traceFunction("addAlias");
    traceArgument("labelExpr", labelExpr);

    next->addAlias(name, labelExpr);
  }

  virtual void setAPs(const std::vector<std::string>& aps) override {
    traceFunction("setAPs");
    traceArgument("aps", aps);

    next->setAPs(aps);
  }

  virtual void setAcceptanceCondition(unsigned int numberOfSets, acceptance_expr::ptr accExpr) override {
    traceFunction("setAcceptanceCondition");
    traceArgument("numberOfSets", numberOfSets);
    traceArgument("accExpr", accExpr);

    next->setAcceptanceCondition(numberOfSets, accExpr);
  }

  virtual void provideAcceptanceName(const std::string& name, const std::vector<IntOrString>& extraInfo) override {
    traceFunction("setAcceptanceCondition");
    traceArgument("name", name);
    traceArgument("extraInfo", extraInfo);

    next->provideAcceptanceName(name, extraInfo);
  }

  virtual void setName(const std::string& name) override {
    traceFunction("setName");
    traceArgument("name", name);

    next->setName(name);
  }

  virtual void setTool(const std::string& name, std::shared_ptr<std::string> version) override {
    traceFunction("setTool");
    traceArgument("name", name);
    traceArgument("version", version);

    next->setTool(name, version);
  }

  virtual void addProperties(const std::vector<std::string>& properties) override {
    traceFunction("addProperties");
    traceArgument("properties", properties);

    next->addProperties(properties);
  }

  virtual void addMiscHeader(const std::string& name, const std::vector<IntOrString>& content) override {
    traceFunction("addMiscHeader");
    traceArgument("content", content);

    next->addMiscHeader(name, content);
  }

  virtual void notifyBodyStart() override {
    traceFunction("notifyBodyStart");

    next->notifyBodyStart();
  }

  virtual void addState(unsigned int id,
                        std::shared_ptr<std::string> info,
                        label_expr::ptr labelExpr,
                        std::shared_ptr<int_list> accSignature) override {
    traceFunction("addState");
    traceArgument("id", id);
    traceArgument("info", info);
    traceArgument("labelExpr", labelExpr);
    traceArgument("accSignature", accSignature);

    next->addState(id, info, labelExpr, accSignature);
  }

  virtual void addEdgeImplicit(unsigned int stateId,
                               const int_list& conjSuccessors,
                               std::shared_ptr<int_list> accSignature) override {
    traceFunction("addEdgeImplicit");
    traceArgument("stateId", stateId);
    traceArgument("conjSuccessors", conjSuccessors);
    traceArgument("accSignature", accSignature);

    next->addEdgeImplicit(stateId, conjSuccessors, accSignature);
  }

  virtual void addEdgeWithLabel(unsigned int stateId,
                                label_expr::ptr labelExpr,
                                const int_list& conjSuccessors,
                                std::shared_ptr<int_list> accSignature) override {
    traceFunction("addEdgeWithLabel");
    traceArgument("stateId", stateId);
    traceArgument("labelExpr", labelExpr);
    traceArgument("conjSuccessors", conjSuccessors);
    traceArgument("accSignature", accSignature);

    next->addEdgeWithLabel(stateId, labelExpr, conjSuccessors, accSignature);
  }

  virtual void notifyEndOfState(unsigned int stateId) override {
    traceFunction("notifyEndOfState");
    traceArgument("stateId", stateId);

    next->notifyEndOfState(stateId);
  }

  virtual void notifyEnd() override {
    traceFunction("notifyEnd");

    next->notifyEnd();
  }

  virtual void notifyAbort() override {
    traceFunction("notifyAbort");

    next->notifyAbort();
  }

  virtual void notifyWarning(const std::string& warning) override {
    traceFunction("notifyWarning");
    traceArgument("warning", warning);

    next->notifyWarning(warning);
  }

protected:
  /** The output stream */
  std::ostream& out;

  /** Trace function call */
  void traceFunction(const std::string& function) {
    out << "=> " << function << std::endl;
  }

  /** Trace argument (string) */
  void traceArgument(const std::string& name, const std::string& value) {
    out << "      " << name << " = " << value << std::endl;
  }

  /** Trace argument (int) */
  void traceArgument(const std::string& name, unsigned int value) {
    out << "      " << name << " = " << value << std::endl;
  }

  /** Trace argument (BooleanExpression) */
  template <typename Atom>
  void traceArgument(const std::string& name, const BooleanExpression<Atom>& expr) {
    out << "      " << name << " = " << expr << std::endl;
  }

  /** Trace argument (vector) */
  template <typename T>
  void traceArgument(const std::string& name, const std::vector<T>& list) {
    out << "      " << name << " = ";
    out << "[";
    bool first= true;
    for (const T& element : list) {
      if (!first) out << ","; else first = false;
      out << element;
    }
    out << "]" << std::endl;
  }

  /** Trace argument (shared_ptr) */
  template <typename O>
  void traceArgument(const std::string& name, typename std::shared_ptr<O> o) {
    if ((bool)o) {
      traceArgument(name, *o);
    } else {
      traceArgument(name, "null");
    }
  }

};

}

#endif
