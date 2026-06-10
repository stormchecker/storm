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

#ifndef CPPHOAFPARSER_HOAINTERMEDIATERESOLVEALIASES_H
#define CPPHOAFPARSER_HOAINTERMEDIATERESOLVEALIASES_H

#include <unordered_map>

#include "cpphoafparser/consumer/hoa_intermediate.hh"

namespace cpphoafparser {

/**
 * A HOAIntermediate that resolves aliases on-the-fly.
 *
 * Stores the definition of aliases from the header and resolves
 * any aliases in label expressions before passing the events to
 * the next consumer.
 */
class HOAIntermediateResolveAliases : public HOAIntermediate
{
public:
  /** Constructor, providing the `next` HOAConsumer */
  HOAIntermediateResolveAliases(HOAConsumer::ptr next) : HOAIntermediate(next) {}

  /** Store alias definition */
  virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) override {
    if (aliases.find(name) != aliases.end()) {
      throw new HOAConsumerException("Alias "+name+" is defined multiple times!");
    }

    if (containsAliases(labelExpr)) {
      // check that all the aliases in the expression are already defined
      checkAliasDefinedness(labelExpr);

      // resolve aliases in the expression
      labelExpr = resolveAliases(labelExpr);
    }

    aliases[name]=labelExpr;
  }

  /** Resolve alias references in state label, pass on to next HOAConsumer */
  virtual void addState(unsigned int id,
                        std::shared_ptr<std::string> info,
                        label_expr::ptr labelExpr,
                        std::shared_ptr<int_list> accSignature) override {
    if (labelExpr && containsAliases(labelExpr)) {
      labelExpr = resolveAliases(labelExpr);
    }
    next->addState(id, info, labelExpr, accSignature);
  }

  /** Resolve alias references in edge label, pass on to next HOAConsumer */
  virtual void addEdgeWithLabel(unsigned int stateId,
                                label_expr::ptr labelExpr,
                                const int_list& conjSuccessors,
                                std::shared_ptr<int_list> accSignature) override {
    if (labelExpr && containsAliases(labelExpr)) {
      labelExpr = resolveAliases(labelExpr);
    }

    next->addEdgeWithLabel(stateId, labelExpr, conjSuccessors, accSignature);
  }

private:
  /** Map for mapping alias names to alias definitions */
  std::unordered_map<std::string, label_expr::ptr> aliases;

  /** Returns `true` if the label expression contains an alias reference */
  bool containsAliases(label_expr::ptr labelExpr) {
    switch (labelExpr->getType()) {
    case label_expr::EXP_FALSE:
    case label_expr::EXP_TRUE:
      return false;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      if (containsAliases(labelExpr->getLeft())) return true;
      if (containsAliases(labelExpr->getRight())) return true;
      return false;
    case label_expr::EXP_NOT:
      if (containsAliases(labelExpr->getLeft())) return true;
      return false;
    case label_expr::EXP_ATOM:
      return labelExpr->getAtom().isAlias();
    }
    throw HOAConsumerException("Unhandled boolean expression type");
  }

  /**
   * Checks that all alias references occuring in the label expression are defined.
   * If that is not the case, a HOAConsumerExeption is thrown.
   */
  void checkAliasDefinedness(label_expr::ptr labelExpr) {
    switch (labelExpr->getType()) {
    case label_expr::EXP_FALSE:
    case label_expr::EXP_TRUE:
      return;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      checkAliasDefinedness(labelExpr->getLeft());
      checkAliasDefinedness(labelExpr->getRight());
      return;
    case label_expr::EXP_NOT:
      checkAliasDefinedness(labelExpr->getLeft());
      return;
    case label_expr::EXP_ATOM:
      if (labelExpr->getAtom().isAlias()) {
        const std::string& aliasName = labelExpr->getAtom().getAliasName();
        if (aliases.find(aliasName) == aliases.end()) {
          throw HOAConsumerException("Expression "+labelExpr->toString()+" uses undefined alias @"+aliasName);
        }
      }
      return;
    }
    throw HOAConsumerException("Unhandled boolean expression type");

  }

  /**
   * Returns a label expression, with all alias references resolved.
   */
  label_expr::ptr resolveAliases(label_expr::ptr labelExpr) {
    switch (labelExpr->getType()) {
    case label_expr::EXP_TRUE:
    case label_expr::EXP_FALSE:
      return labelExpr;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      return label_expr::ptr(new label_expr(labelExpr->getType(),
                                            resolveAliases(labelExpr->getLeft()),
                                            resolveAliases(labelExpr->getRight())));
    case label_expr::EXP_NOT:
      return label_expr::ptr(new label_expr(labelExpr->getType(),
                                            resolveAliases(labelExpr->getLeft()),
                                            nullptr));
    case label_expr::EXP_ATOM:
      if (!labelExpr->getAtom().isAlias()) {
        return labelExpr;
      } else {
        auto it = aliases.find(labelExpr->getAtom().getAliasName());
        if (it == aliases.end()) {
          throw HOAConsumerException("Can not resolve alias @"+labelExpr->getAtom().getAliasName());
        }

        label_expr::ptr resolved = it->second;
        if (containsAliases(resolved)) {
          resolved = resolveAliases(resolved);
        }

        return resolved;
      }
    }
    throw HOAConsumerException("Unhandled boolean expression type");
  }

};

}
#endif
