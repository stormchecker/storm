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

#ifndef CPPHOAFPARSER_HOAINTERMEDIATECHECKVALIDITY_H
#define CPPHOAFPARSER_HOAINTERMEDIATECHECKVALIDITY_H

#include "cpphoafparser/consumer/hoa_intermediate.hh"
#include "cpphoafparser/util/dynamic_bitset.hh"
#include "cpphoafparser/util/implicit_edge_helper.hh"
#include "cpphoafparser/util/acceptance_repository_standard.hh"

#include <unordered_set>
#include <cassert>

namespace cpphoafparser {

// TODO Accurately reflect status in the doc!
/**
 * HOAIntermediate that checks that the parsed HOA automaton is well-formed.
 *
 * Among others, checks for
 * <ul>
 * <li>Conformance of stated properties with the automaton structure.
 * <li>Conformance of Acceptance and acc-name headers.
 * <li>Definedness of aliases.
 * <li>Well-formedness of label expressions (only using atomic proposition indizes that are defined).
 * <li>Well-formedness of acceptance (only using acceptance set indizes that are defined).
 * </ul>
 */
class HOAIntermediateCheckValidity : public HOAIntermediate
{
public:

  /** Constructor, providing the `next` HOAConsumer */
  HOAIntermediateCheckValidity(HOAConsumer::ptr next)
    : HOAIntermediate(next),
      implicitEdgeHelper(0) {}

  /**
   * Add (semantically-relevant) headers not defined in the format specification that
   * can be handled.
   */
  template <class InputIterator>
  void setSupportedMiscHeaders(InputIterator first, InputIterator last) {
    supportedMiscHeaders.insert(first, last);
  }
  /**
   * Add (semantically-relevant) header not defined in the format specification that
   * can be handled.
   */
  void addSupportedMiscHeader(const std::string& supportedMiscHeader) {
    supportedMiscHeaders.insert(supportedMiscHeader);
  }

  virtual bool parserResolvesAliases() override {
    return next->parserResolvesAliases();
  }

  virtual void notifyHeaderStart(const std::string& version) override {
    if (version != "v1") {
      throw HOAConsumerException("Can only parse HOA format v1");
    }
    next->notifyHeaderStart(version);
  }

  virtual void setNumberOfStates(unsigned int numberOfStates) override {
    headerAtMostOnce("States");
    this->numberOfStates.reset(new unsigned int(numberOfStates));
    next->setNumberOfStates(numberOfStates);
  }

  virtual void addStartStates(const int_list& stateConjunction) override {
    numberOfStartHeaders++;
    if (stateConjunction.size()>1) {
      hasUniversalBranching = true;
    }

    for (unsigned int state : stateConjunction) {
      checkStateIndex(state);
      startStates.set(state);
    }
    next->addStartStates(stateConjunction);
  }

  virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) override {
    usedHeaders.insert("Alias");

    checkAliasesAreDefined(labelExpr);
    aliases.insert(name);

    gatherLabels(labelExpr, apsInAliases);

    next->addAlias(name, labelExpr);
  }

  virtual void setAPs(const std::vector<std::string>& aps) override {
    headerAtMostOnce("AP");

    numberOfAPs.reset(new unsigned int(aps.size()));

    std::unordered_set<std::string> apSet;
    for (const std::string& ap : aps) {
      if (apSet.insert(ap).second == false) {
        throw HOAConsumerException("Atomic proposition "+ap+" appears more than once in AP-header");
      }
    }

    next->setAPs(aps);
  }

  virtual void setAcceptanceCondition(unsigned int numberOfSets, acceptance_expr::ptr accExpr) override {
    headerAtMostOnce("Acceptance");
    numberOfAcceptanceSets.reset(new unsigned int(numberOfSets));

    checkAcceptanceCondition(accExpr);
    acceptance = accExpr->deepCopy();

    next->setAcceptanceCondition(numberOfSets, accExpr);
  }

  virtual void provideAcceptanceName(const std::string& name, const std::vector<IntOrString>& extraInfo) override {
    headerAtMostOnce("acc-name");
    accName.reset(new std::string(name));

    accExtraInfo = extraInfo;
    next->provideAcceptanceName(name,  extraInfo);
  }


  virtual void setName(const std::string& name) override {
    headerAtMostOnce("name");
    next->setName(name);
  }

  virtual void setTool(const std::string& name, std::shared_ptr<std::string> version) override {
    headerAtMostOnce("tool");
    next->setTool(name, version);
  }

  virtual void addProperties(const std::vector<std::string>& properties) override {
    usedHeaders.insert("properties");

    for (const std::string& property : properties) {
      if (property == "state_labels") {
        property_state_labels = true;
      } else if (property == "trans_labels") {
          property_trans_labels = true;
      } else if (property == "implicit_labels") {
        property_implicit_labels = true;
      } else if (property == "explicit_labels") {
        property_explicit_labels = true;
      } else if (property == "state_acc") {
        property_state_acc = true;
      } else if (property == "trans_acc") {
        property_trans_acc = true;
      } else if (property == "univ_branch") {
       property_univ_branch = true;
      } else if (property == "no_univ_branch") {
        property_no_univ_branch = true;
      } else if (property == "deterministic") {
        property_deterministic = true;
      } else if (property == "complete") {
        property_complete = true;
      } else if (property == "colored") {
        property_colored = true;
      } else {
        // do nothing
      }
    }

    next->addProperties(properties);
  }

  virtual void addMiscHeader(const std::string& name, const std::vector<IntOrString>& content) override {
      usedHeaders.insert(name);
    if (name.at(0) >= 'A' && name.at(0) <= 'Z') {
      // first character is upper case -> if we don't know what to do with this header,
      // raise exception as this may change the semantics of the automaton
      if (supportedMiscHeaders.find(name) != supportedMiscHeaders.end())
      throw HOAConsumerException("Header "+name+" potentially has semantic relevance, but is not supported");
    }
    next->addMiscHeader(name, content);
  }

  virtual void notifyBodyStart() override {
    // check for existence of mandatory headers
    headerIsMandatory("Acceptance");

    // check that all AP indizes in aliases are valid
    if (!numberOfAPs) {
      // there was no AP-header, equivalent to AP: 0
      numberOfAPs.reset(new unsigned int(0));
    }
    std::pair<std::size_t, bool> highestAPIndex = apsInAliases.getHighestSetBit();
    if (highestAPIndex.second && highestAPIndex.first >= *numberOfAPs) {
      throw HOAConsumerException("AP index "
          + std::to_string(highestAPIndex.first)
          + " in some alias definition is out of range (0 - "
          + std::to_string(*numberOfAPs-1)
          +")");
    }

    if (accName) {
      checkAccName();
    }

    // check whether the start states violate properties
    if (property_no_univ_branch && hasUniversalBranching) {
      throw HOAConsumerException("Property 'no_univ_branching' is violated by the start states");
    }
    if (property_deterministic && numberOfStartHeaders != 1) {
      throw HOAConsumerException("Property 'deterministic' is violated by having "+std::to_string(numberOfStartHeaders)+" Start-headers");
    }

    implicitEdgeHelper = ImplicitEdgeHelper(*numberOfAPs);

    next->notifyBodyStart();
  }

  virtual void addState(unsigned int id, std::shared_ptr<std::string> info, label_expr::ptr labelExpr, std::shared_ptr<int_list> accSignature) override {
    checkStateIndex(id);
    if (statesWithDefinition.get(id)) {
      throw HOAConsumerException("State "+std::to_string(id)+" is defined multiple times");
    }
    statesWithDefinition.set(id);
    currentState = id;

    if (accSignature) {
      checkAcceptanceSignature(*accSignature, false);
      currentStateIsColored = (accSignature->size() == 1);
    } else {
      currentStateIsColored = false;
    }

    if (property_colored && *numberOfAcceptanceSets>0 && !currentStateIsColored) {
      if (property_state_acc) {
        // we already know that the automaton is in violation...
        throw HOAConsumerException("State "+std::to_string(id)+" is not colored");
      }
    }

    if (labelExpr) {
      checkLabelExpression(labelExpr);
    }

    // reset flags
    currentStateHasStateLabel = (bool)labelExpr;
    currentStateHasTransitionLabel = false;
    currentStateHasImplicitEdge = false;
    currentStateHasExplicitEdge = false;
    currentStateHasStateAcceptance = (bool)accSignature;
    currentStateHasTransitionAcceptance = false;

    implicitEdgeHelper.startOfState(id);

    next->addState(id, info, labelExpr, accSignature);
  }

  virtual void addEdgeImplicit(unsigned int stateId,
                               const int_list& conjSuccessors,
                               std::shared_ptr<int_list> accSignature) override {
    assert(stateId == currentState);

    for (unsigned int succ : conjSuccessors) {
      checkStateIndexTarget(succ);
    }

    if (conjSuccessors.size() > 1) {
      hasUniversalBranching = true;
    }

    bool edgeIsColored = false;
    if (accSignature) {
      checkAcceptanceSignature(*accSignature, true);
      edgeIsColored = (accSignature->size() == 1);
    }

    if (property_colored && *numberOfAcceptanceSets > 0) {
      if (!currentStateIsColored && !edgeIsColored) {
        throw HOAConsumerException("In state "+std::to_string(stateId)+", there is a transition that is not colored...");
      } else if (currentStateIsColored && edgeIsColored) {
        throw HOAConsumerException("In state "+std::to_string(stateId)+", there is a transition that is colored even though the state is colored already...");
      }
    }

    if (currentStateHasExplicitEdge) {
      throw HOAConsumerException("Can not mix explicit and implicit edge definitions (state "+std::to_string(stateId)+")");
    }
    currentStateHasImplicitEdge = true;

    currentStateHasTransitionLabel = true;
    if (currentStateHasStateLabel) {
      throw new HOAConsumerException("Can not mix state labels and implicit edge definitions (state "+std::to_string(stateId)+")");
    }

    implicitEdgeHelper.nextImplicitEdge();
    next->addEdgeImplicit(stateId, conjSuccessors, accSignature);
  }

  virtual void addEdgeWithLabel(unsigned int stateId,
                               label_expr::ptr labelExpr,
                               const int_list& conjSuccessors,
                               std::shared_ptr<int_list> accSignature) override {
    assert(stateId == currentState);

    for (unsigned int succ : conjSuccessors) {
      checkStateIndexTarget(succ);
    }

    if (conjSuccessors.size() > 1) {
      hasUniversalBranching = true;
    }

    bool edgeIsColored = false;
    if (accSignature) {
      checkAcceptanceSignature(*accSignature, true);
      edgeIsColored = (accSignature->size() == 1);
    }

    if (property_colored && *numberOfAcceptanceSets > 0) {
      if (!currentStateIsColored && !edgeIsColored) {
        throw HOAConsumerException("In state "+std::to_string(stateId)+", there is a transition that is not colored...");
      } else if (currentStateIsColored && edgeIsColored) {
        throw HOAConsumerException("In state "+std::to_string(stateId)+", there is a transition that is colored even though the state is colored already...");
      }
    }

    if (labelExpr) {
      checkLabelExpression(labelExpr);
    }

    if (labelExpr) {
      currentStateHasTransitionLabel = true;
      if (currentStateHasStateLabel) {
        throw HOAConsumerException("Can not mix state and transition labeling (state "+std::to_string(stateId)+")");
      }
    }

    if (currentStateHasImplicitEdge) {
      throw HOAConsumerException("Can not mix explicit and implicit edge definitions (state "+std::to_string(stateId)+")");
    }
    currentStateHasExplicitEdge = true;

    next->addEdgeWithLabel(stateId, labelExpr, conjSuccessors, accSignature);
  }

  virtual void notifyEndOfState(unsigned int stateId) override {
    implicitEdgeHelper.endOfState();

    // check for property violations
    if (property_state_labels && currentStateHasTransitionLabel) {
      throw HOAConsumerException("Property 'state-labels' is violated by having transition labels in state "+std::to_string(stateId));
    }
    if (property_trans_labels && currentStateHasStateLabel) {
      throw HOAConsumerException("Property 'trans-labels' is violated by having a state label in state "+std::to_string(stateId));

    }
    if (property_implicit_labels && currentStateHasExplicitEdge) {
      throw HOAConsumerException("Property 'implicit-label' is violated by having a label expression on a transition in state "+std::to_string(stateId));
    }
    if (property_explicit_labels && currentStateHasImplicitEdge) {
      throw HOAConsumerException("Property 'explicit-label' is violated by having implicit transition(s) in state "+std::to_string(stateId));
    }
    if (property_state_acc && currentStateHasTransitionAcceptance) {
      throw HOAConsumerException("Property 'state-acc' is violated by having transition acceptance in state "+std::to_string(stateId));
    }
    if (property_trans_acc && currentStateHasStateAcceptance) {
      throw HOAConsumerException("Property 'trans-acc' is violated by having state acceptance in state "+std::to_string(stateId));
    }
    if (property_no_univ_branch && hasUniversalBranching) {
      throw HOAConsumerException("Property 'no-univ-branch' is violated by having universal branching in state "+std::to_string(stateId));
    }

    // TODO: deterministic
    //  for implicit edges, this is done via the ImplicitEdgeHelper
    //  for explicit edges, check would need to keep track of overlapping label expressions

    // TODO: complete
    //  for implicit edges, this is done via the ImplicitEdgeHelper
    //  for explicit edges, check would need to keep track of overlapping label expressions

    next->notifyEndOfState(stateId);
  }

  virtual void notifyEnd() override {
    // check sanity of state definitions and target states
    checkStates();

    if (property_univ_branch && !hasUniversalBranching) {
      throw HOAConsumerException("Property 'univ-branch' is violated by not having universal branching in the automaton");
    }

    next->notifyEnd();
  }

  virtual void notifyAbort() override {
    next->notifyAbort();
  }

protected:

  // ----------------------------------------------------------------------------

  /** Send a warning */
  void doWarning(const std::string& warning)
  {
    next->notifyWarning(warning);
  }

  // ----------------------------------------------------------------------------

  /** Check that a mandatory header has been seen */
  void headerIsMandatory(const std::string& name)
  {
    if (usedHeaders.find(name) == usedHeaders.end()) {
      throw HOAConsumerException("Mandatory header "+name+" is missing");
    }
  }

  /** Check that a 'once' header has not been seen multiple times */
  void headerAtMostOnce(const std::string& headerName)
  {
    if (usedHeaders.insert(headerName).second == false) {
      throw HOAConsumerException("Header "+headerName+" occurs multiple times, but is allowed only once.");
    }
  }

  /** Check that a states index is in range */
  void checkStateIndex(unsigned int index)
  {
    if (numberOfStates) {
      if (index >= *numberOfStates) {
        throw HOAConsumerException("State index "
            + std::to_string(index)
            + " is out of range (0 - "
            + std::to_string(*numberOfStates-1)
            + ")");
      }
    }
  }

  /** Checks that the target state index of a transition is valid */
  void checkStateIndexTarget(unsigned int index)
  {
    if (numberOfStates) {
      if (index >= *numberOfStates) {
        throw HOAConsumerException("State index "
            + std::to_string(index)
            + " is out of range (0 - "
            + std::to_string(*numberOfStates-1)
            + ")");
      }
    }
    targetStatesOfTransitions.set(index);
  }

  /** Check that the states have been properly defined */
  void checkStates()
  {
    bool haveComplainedAboutMissingStates = false;

    // if numberOfStates is set, check that all states are in range
    if (numberOfStates) {
      // the states with a definition
      std::pair<std::size_t, bool> highestStateIndex = statesWithDefinition.getHighestSetBit();
      if (highestStateIndex.second && highestStateIndex.first >= *numberOfStates) {
        throw HOAConsumerException("State index "
            + std::to_string(highestStateIndex.first)
            + " is out of range (0 - "
            + std::to_string(*numberOfStates-1)
            + ")");
      }

      // the states occurring as targets
      highestStateIndex = targetStatesOfTransitions.getHighestSetBit();
      if (highestStateIndex.second && highestStateIndex.first >= *numberOfStates) {
        throw HOAConsumerException("State index "
            + std::to_string(highestStateIndex.first)
            + " (target in a transition) is out of range (0 - "
            + std::to_string(*numberOfStates-1)
            +")");
      }

      // the start states
      highestStateIndex = startStates.getHighestSetBit();
      if (highestStateIndex.second && highestStateIndex.first >= *numberOfStates) {
        throw HOAConsumerException("State index "
            + std::to_string(highestStateIndex.first)
            + " (start state) is out of range (0 - "
            + std::to_string(*numberOfStates-1)
            +")");
      }

      if (statesWithDefinition.cardinality() != *numberOfStates) {
        std::size_t missing = *numberOfStates - statesWithDefinition.cardinality();
        doWarning("There are "+std::to_string(missing)+" states without definition");
        haveComplainedAboutMissingStates = true;
      }
    }

    dynamic_bitset targetsButNoDefinition(targetStatesOfTransitions);
    targetsButNoDefinition.andNot(statesWithDefinition);
    if (!targetsButNoDefinition.isEmpty() && !haveComplainedAboutMissingStates) {
      doWarning("There are "
          + std::to_string(targetsButNoDefinition.cardinality())
          + " states that are targets of transitions but that have no definition");
      haveComplainedAboutMissingStates = true;
    }

    dynamic_bitset startStatesButNoDefinition(startStates);
    startStatesButNoDefinition.andNot(statesWithDefinition);
    if (!startStatesButNoDefinition.isEmpty() && !haveComplainedAboutMissingStates) {
      doWarning("There are "
          + std::to_string(startStatesButNoDefinition.cardinality())
          + " states that are start states but that have no definition");
      haveComplainedAboutMissingStates = true;
    }


    if (haveComplainedAboutMissingStates && property_colored && *numberOfAcceptanceSets > 0) {
      // states without definition are not colored
      throw HOAConsumerException("An automaton with property 'colored' can not have states missing a definition");
     }
  }

  /**
   * Check that the acceptance condition is well-formed.
   * In particular, that no boolean negation occurs and
   * that the referenced acceptance sets are defined.
   */
  void checkAcceptanceCondition(acceptance_expr::ptr accExpr)
  {
    assert (numberOfAcceptanceSets);

    switch (accExpr->getType()) {
    case acceptance_expr::EXP_TRUE:
    case acceptance_expr::EXP_FALSE:
      return;
    case acceptance_expr::EXP_AND:
    case acceptance_expr::EXP_OR:
      checkAcceptanceCondition(accExpr->getLeft());
      checkAcceptanceCondition(accExpr->getRight());
      return;
    case acceptance_expr::EXP_NOT:
      throw HOAConsumerException("Acceptance condition contains boolean negation, not allowed");
    case acceptance_expr::EXP_ATOM:
      unsigned int acceptanceSet = accExpr->getAtom().getAcceptanceSet();
      if (acceptanceSet >= *numberOfAcceptanceSets) {
        throw HOAConsumerException("Acceptance condition contains acceptance set with index "
            + std::to_string(acceptanceSet)
            + ", valid range is 0 - "
            + std::to_string(*numberOfAcceptanceSets-1));
      }
      return;
    }
    throw std::logic_error("Unknown operator in acceptance condition: "+accExpr->toString());
  }

  /**
   * Check that an acceptance signature is well-formed.
   * In particular, that the referenced acceptance sets are defined.
   */
  void checkAcceptanceSignature(const int_list& accSignature, bool inTransition)
  {
    for (unsigned int acceptanceSet : accSignature) {
      if (acceptanceSet >= *numberOfAcceptanceSets) {
        throw HOAConsumerException("Acceptance signature "
            + (inTransition ? std::string("(in transition) ") : std::string(""))
            + "for state index "
            + std::to_string(currentState)
            + " contains acceptance set with index "
            + std::to_string(acceptanceSet)
            + ", valid range is 0 - "
            + std::to_string(*numberOfAcceptanceSets-1));
      }
    }
  }

  /**
   * Check that all alias references in a label expression are
   * properly defined
   */
  void checkAliasesAreDefined(label_expr::ptr expr)
  {
    switch (expr->getType()) {
    case label_expr::EXP_TRUE:
    case label_expr::EXP_FALSE:
      return;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      checkAliasesAreDefined(expr->getLeft());
      checkAliasesAreDefined(expr->getRight());
      return;
    case label_expr::EXP_NOT:
      checkAliasesAreDefined(expr->getLeft());
      return;
    case label_expr::EXP_ATOM:
      if (expr->getAtom().isAlias()) {
        if (aliases.find(expr->getAtom().getAliasName()) != aliases.end()) {
          throw HOAConsumerException("Alias @"+expr->getAtom().getAliasName()+" is not defined");
        }
      }
      return;
    }
    throw std::logic_error("Unknown operator in label expression: "+expr->toString());
  }

  /**
   * Traverse the expression and, for every label encountered, set the corresponding bit in the
   * `result` bitset.
   * @param expr the expression
   * @param[out] result a bitset where the additional bits corresponding to APs are set
   */
  void gatherLabels(label_expr::ptr expr, dynamic_bitset& result) {
    switch (expr->getType()) {
    case label_expr::EXP_TRUE:
    case label_expr::EXP_FALSE:
      return;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      gatherLabels(expr->getLeft(), result);
      gatherLabels(expr->getRight(), result);
      return;
    case label_expr::EXP_NOT:
      gatherLabels(expr->getLeft(), result);
      return;
    case label_expr::EXP_ATOM:
      if (!expr->getAtom().isAlias()) {
        result.set(expr->getAtom().getAPIndex());
      }
      return;
    }
    throw std::logic_error("Unknown operator in label expression: "+expr->toString());
  }

  /** Check a label expression for well-formedness */
  void checkLabelExpression(label_expr::ptr expr)
  {
    switch (expr->getType()) {
    case label_expr::EXP_TRUE:
    case label_expr::EXP_FALSE:
      return;
    case label_expr::EXP_AND:
    case label_expr::EXP_OR:
      checkLabelExpression(expr->getLeft());
      checkLabelExpression(expr->getRight());
      return;
    case label_expr::EXP_NOT:
      checkLabelExpression(expr->getLeft());
      return;
    case label_expr::EXP_ATOM:
      if (expr->getAtom().isAlias()) {
        if (aliases.find(expr->getAtom().getAliasName()) == aliases.end()) {
          throw HOAConsumerException("Alias @"+expr->getAtom().getAliasName()+" is not defined");
        }
      } else {
        assert(numberOfAPs);
        unsigned int apIndex = expr->getAtom().getAPIndex();
        if (apIndex >= *numberOfAPs) {
          if (*numberOfAPs == 0) {
            throw HOAConsumerException("AP index "+std::to_string(apIndex)+" in expression is out of range (no APs): "+expr->toString());
          } else {
            throw HOAConsumerException("AP index "
                + std::to_string(apIndex)
                + " in expression is out of range (from 0 to "
                + std::to_string(*numberOfAPs-1)
                + "): "
                + expr->toString());
          }
        }
      }
      return;
    }
    throw std::logic_error("Unknown operator in label expression: "+expr->toString());
  }

  // ----------------------------------------------------------------------------

  /**
   * Check that the canonical expression for an acc-name (if known) matches that
   * given by the Acceptance header.
   */
  void checkAccName(){
    AcceptanceRepository::ptr repository(new AcceptanceRepositoryStandard());


    acceptance_expr::ptr canonical;
    try {
      canonical = repository->getCanonicalAcceptanceExpression(*accName, accExtraInfo);
      if (!(bool)canonical) {
        // acceptance name is not known
        return;
      }
    } catch (AcceptanceRepository::IllegalArgumentException& e) {
      throw HOAConsumerException(e.what());
    }

  assert((bool)acceptance);

  //std::cerr << "Canonical: " << *canonical << std::endl;
  //std::cerr << "Acceptance: " << *acceptance << std::endl;

  if (!acceptance_expr::areSyntacticallyEqual(acceptance, canonical)) {
    throw HOAConsumerException(std::string("The acceptance given by the Acceptance and by the acc-name headers do not match syntactically:")
        + "\nFrom Acceptance-header: "+acceptance->toString()
        + "\nCanonical expression for acc-name-header: "+canonical->toString());
  }
}

// ----------------- member variables

protected:
  /** A set of headers that are supported beyond the standard headers of the format */
  std::unordered_set<std::string> supportedMiscHeaders;

  /** The header names that have occurred so far in the automaton definition */
  std::unordered_set<std::string> usedHeaders;

  /** The number of states that have been specified in the header (optional) */
  std::shared_ptr<unsigned int > numberOfStates;

  /** The number of acceptance sets (mandatory) */
  std::shared_ptr<unsigned int> numberOfAcceptanceSets;

  /** The set of states for which addState has been called */
  dynamic_bitset statesWithDefinition;

  /** The set of states that occur as target states of some transition */
  dynamic_bitset targetStatesOfTransitions;

  /** The set of states that are start states */
  dynamic_bitset startStates;

  /** The set of alias names that have been defined (Alias-header) */
  std::unordered_set<std::string> aliases;
  /** Atomic propositions that are referenced in some alias definition */
  dynamic_bitset apsInAliases;

  /** The number of atomic propositions */
  std::shared_ptr<unsigned int> numberOfAPs;

  /** The acc-name (optional) */
  std::shared_ptr<std::string> accName;
  /** extraInfo parameters for acc-name */
  std::vector<IntOrString> accExtraInfo;

  /** The acceptance condition */
  acceptance_expr::ptr acceptance;

  /** The current state */
  std::size_t currentState = 0;
  /** Does the current state have a state label? */
  bool currentStateHasStateLabel = false;
  /** Does the current state have transitions with transition label? */
  bool currentStateHasTransitionLabel = false;
  /** Does the current state have implicit edges? */
  bool currentStateHasImplicitEdge = false;
  /** Does the current state have edges with explicit labels? */
  bool currentStateHasExplicitEdge = false;
  /** Does the current state have state acceptance? */
  bool currentStateHasStateAcceptance = false;
  /** Does the current state have transition acceptance? */
  bool currentStateHasTransitionAcceptance = false;
  /** Is the current state colored ? */
  bool currentStateIsColored = false;


  /** The implicit edge helper */
  ImplicitEdgeHelper implicitEdgeHelper;

  // properties: set to true if the given property is asserted by the HOA automaton
  /** hints that the automaton uses only state labels */
  bool property_state_labels = false;
  /** hints that the automaton uses only transition labels */
  bool property_trans_labels = false;
  /** hints that the automaton uses only implicit transitions labels */
  bool property_implicit_labels = false;
  /** hints that the automaton uses only explicit transitions labels */
  bool property_explicit_labels = false;
  /** hints that the automaton uses only state-based acceptance specifications */
  bool property_state_acc = false;
  /** hints that the automaton uses only transition-based acceptance specifications */
  bool property_trans_acc = false;
  /** hints that the automaton uses universal branching for at least one transition or for the initial state */
  bool property_univ_branch = false;
  /** hints that the automaton does not use universal branching */
  bool property_no_univ_branch = false;
  /** hints that the automaton is deterministic, i.e., it has at most one initial state, and the outgoing transitions of each state have disjoint labels (note that this also applies in the presence of universal branching) */
  bool property_deterministic = false;
  /** hints that the automaton is complete, i.e., it has at least one state, and the transition function is total */
  bool property_complete = false;
  /** hints that each transition (or each state, for state-based acceptance) of the automaton belongs to exactly one acceptance set; this is typically the case in parity automata */
  bool property_colored = false;

  /** The number of Start-definitions (more than 1 = non-deterministic start states) */
  int numberOfStartHeaders = 0;
  /** Has this automaton universal branching? */
  bool hasUniversalBranching = false;
};

}

#endif
