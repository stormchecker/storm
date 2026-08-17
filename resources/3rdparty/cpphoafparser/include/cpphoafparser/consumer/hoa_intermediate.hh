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

#ifndef CPPHOAFPARSER_HOAINTERMEDIATE_H
#define CPPHOAFPARSER_HOAINTERMEDIATE_H

#include "cpphoafparser/consumer/hoa_consumer.hh"

namespace cpphoafparser {

/**
 * The HOAIntermediate class provides a mechanism to chain
 * multiple HOAConsumer together.
 * Implementing the HOAConsumer interface, the default behavior
 * is to simply propagate method calls to the `next` consumer.
 *
 * By overriding functions, this behavior can be customized, e.g.,
 * validating constraints on the input (HOAIntermediateCheckValidity) or
 * performing on-the-fly transformations (HOAIntermediateResolveAliases).
  */

class HOAIntermediate : public HOAConsumer {
public:

  /** Constructor, providing the `next` HOAConsumer */
  HOAIntermediate(HOAConsumer::ptr next) : next(next) {}

  virtual bool parserResolvesAliases() override {
    return next->parserResolvesAliases();
  }

  virtual void notifyHeaderStart(const std::string& version) override {
    next->notifyHeaderStart(version);
  }

  virtual void setNumberOfStates(unsigned int numberOfStates) override {
    next->setNumberOfStates(numberOfStates);
  }

  virtual void addStartStates(const int_list& stateConjunction) override {
    next->addStartStates(stateConjunction);
  }

  virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) override {
    next->addAlias(name, labelExpr);
  }

  virtual void setAPs(const std::vector<std::string>& aps) override {
    next->setAPs(aps);
  }

  virtual void setAcceptanceCondition(unsigned int numberOfSets, acceptance_expr::ptr accExpr) override {
    next->setAcceptanceCondition(numberOfSets, accExpr);
  }

  virtual void provideAcceptanceName(const std::string& name, const std::vector<IntOrString>& extraInfo) override {
    next->provideAcceptanceName(name, extraInfo);
  }

  virtual void setName(const std::string& name) override {
    next->setName(name);
  }

  virtual void setTool(const std::string& name, std::shared_ptr<std::string> version) override {
    next->setTool(name, version);
  }

  virtual void addProperties(const std::vector<std::string>& properties) override {
    next->addProperties(properties);
  }

  virtual void addMiscHeader(const std::string& name, const std::vector<IntOrString>& content) override {
    next->addMiscHeader(name, content);
  }

  virtual void notifyBodyStart() override {
    next->notifyBodyStart();
  }

  virtual void addState(unsigned int id,
                        std::shared_ptr<std::string> info,
                        label_expr::ptr labelExpr,
                        std::shared_ptr<int_list> accSignature) override {
    next->addState(id, info, labelExpr, accSignature);
  }

  virtual void addEdgeImplicit(unsigned int stateId,
                               const int_list& conjSuccessors,
                               std::shared_ptr<int_list> accSignature) override {
    next->addEdgeImplicit(stateId, conjSuccessors, accSignature);
  }

  virtual void addEdgeWithLabel(unsigned int stateId,
                                label_expr::ptr labelExpr,
                                const int_list& conjSuccessors,
                                std::shared_ptr<int_list> accSignature) override {
    next->addEdgeWithLabel(stateId, labelExpr, conjSuccessors, accSignature);
  }

  virtual void notifyEndOfState(unsigned int stateId) override {
    next->notifyEndOfState(stateId);
  }

  virtual void notifyEnd() override {
    next->notifyEnd();
  }

  virtual void notifyAbort() override {
    next->notifyAbort();
  }

  virtual void notifyWarning(const std::string& warning) override {
    next->notifyWarning(warning);
  }

protected:
  /** The next consumer */
  HOAConsumer::ptr next;
};

}

#endif
