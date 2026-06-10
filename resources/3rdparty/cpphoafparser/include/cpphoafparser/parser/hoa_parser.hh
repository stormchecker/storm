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

#ifndef CPPHOAFPARSER_HOAPARSER_H
#define CPPHOAFPARSER_HOAPARSER_H

#include <set>
#include <string>
#include <sstream>

#include "cpphoafparser/parser/hoa_lexer.hh"
#include "cpphoafparser/consumer/hoa_consumer.hh"
#include "cpphoafparser/consumer/hoa_intermediate_check_validity.hh"
#include "cpphoafparser/consumer/hoa_intermediate_resolve_aliases.hh"
#include "cpphoafparser/parser/hoa_parser_exception.hh"


/** @mainpage cpphoafparser API documentation
 *
 * API documentation for the <a href="http://automata.tools/hoa/cpphoafparser/">cpphoafparser</a> library.
 */

/** @namespace cpphoafparser
 * The `cpphoafparser` namespace contains all the classes of the
 * cpphoafparser library.
 */
namespace cpphoafparser {

/**
 * Parser class for parsing HOA files.
 *
 * Provides a static function for parsing a HOA automaton from an input stream,
 * calling into a HOAConsumer for every syntax element encountered during the parse.
 *
 * The parser is implemented as a simple, hand-written recursive-descent parser,
 * with functions corresponding to grammar rules.
 **/
class HOAParser {
public:

  /**
   * Function for parsing a single HOA automaton.
   *
   * On error, will throw HOAParserException, the consumers will generally throw
   * HOAConsumerException.
   *
   * @param in std::istream from which the automaton will be read
   * @param consumer a shared_ptr to the HOAConsumer whose functions will
   *        be called for each element of the HOA automaton
   * @param check_validity Should the validity of the HOA be checked?
   *        These are checks beyond the basic syntactic well-formedness guaranteed by the grammar.
   **/
  static void parse(std::istream& in, HOAConsumer::ptr consumer, bool check_validity=true) {
    if (consumer->parserResolvesAliases()) {
      consumer.reset(new HOAIntermediateResolveAliases(consumer));
    }

    if (check_validity) {
      consumer.reset(new HOAIntermediateCheckValidity(consumer));
    }

    HOAParser parser(in, consumer);
    parser.nextToken();
    parser.Automaton();
  }

private:
  /** The registered consumer */
  HOAConsumer::ptr consumer;
  /** The lexer for tokenizing the input stream */
  HOALexer lexer;

  /** The current token */
  HOALexer::Token token;
  /** true if we are currently in a State: definition */
  bool inState;
  /** the index of the current state*/
  unsigned int currentState;
  /** true if the current state has state labeling */
  bool currentStateHasStateLabel;


  /** Private constructor. */
  HOAParser(std::istream& in, HOAConsumer::ptr consumer) :
    consumer(consumer), lexer(in), inState(false), currentState(0), currentStateHasStateLabel(false) {
  }

  /** Advance to the next token. Handles TOKEN_ABORT */
  void nextToken() {
    token = lexer.nextToken();
    if (token.kind == HOALexer::TOKEN_ABORT) {
      consumer->notifyAbort();
      throw "aborted";
    }
  }

  /** Advances to the next token if it is of the expected kind, otherwise throw an error. */
  void expect(HOALexer::TokenType kind, const std::string& context="") {
    if (token.kind != kind) {
      throw error(HOALexer::Token::forErrorMessage(kind), context);
    }

    // eat token
    nextToken();
  }

  /**
   * Constructs a HOAParserExeption for a syntax error.
   * @param expectedTokenTypes a string detailing which token types were expected
   * @param context optionally, some context for the error message */
  HOAParserException error(const std::string& expectedTokenTypes, const std::string& context="") {
    std::stringstream ss;
    ss << "Syntax error";
    if (context != "") {
      ss << " (while reading " << context << ")";
    }
    ss << ": Expected " << expectedTokenTypes;
    ss << ", got " << HOALexer::Token::forErrorMessage(token);
    ss << "  (line " << std::to_string(token.line) <<  ", col " << std::to_string(token.col) << ")";

    return HOAParserException(ss.str(), token.line, token.col);
  }

  /** Grammar rule for the whole Automaton */
  void Automaton() {
    Header();
    expect(HOALexer::TOKEN_BODY);
    consumer->notifyBodyStart();
    Body();
    expect(HOALexer::TOKEN_END);
    if (inState) {
      consumer->notifyEndOfState(currentState);
    }
    consumer->notifyEnd();
  }

  /** Grammar rule for the HOA header */
  void Header() {
    Format();
    HeaderItems();
  }

  /** Grammar rule for the HOA: header */
  void Format() {
    expect(HOALexer::TOKEN_HOA);
    std::string version = Identifier("version");
    // TODO: check format version
    consumer->notifyHeaderStart(version);
  }

  /** Grammar rule for the remaining header items, returns if there are not more headers */
  void HeaderItems() {
    while (true) {
      switch (token.kind) {
      case HOALexer::TOKEN_STATES: HeaderItemStates(); break;
      case HOALexer::TOKEN_START: HeaderItemStart(); break;
      case HOALexer::TOKEN_AP: HeaderItemAP(); break;
      case HOALexer::TOKEN_ALIAS: HeaderItemAlias(); break;
      case HOALexer::TOKEN_ACCEPTANCE: HeaderItemAcceptance(); break;
      case HOALexer::TOKEN_ACCNAME: HeaderItemAccName(); break;
      case HOALexer::TOKEN_TOOL: HeaderItemTool(); break;
      case HOALexer::TOKEN_NAME: HeaderItemName(); break;
      case HOALexer::TOKEN_PROPERTIES: HeaderItemProperties(); break;	
      case HOALexer::TOKEN_HEADER_NAME: HeaderMiscItem(); break;
      default:
        // not a header, return
        return;
      }
    }
  }

  /** Grammar rule for the States-header */
  void HeaderItemStates() {
    expect(HOALexer::TOKEN_STATES);

    unsigned int states = Integer("number of states in States-header");
    consumer->setNumberOfStates(states);
  }

  /** Grammar rule for the Start-header */
  void HeaderItemStart() {
    expect(HOALexer::TOKEN_START);

    std::vector<unsigned int> stateConjunction;
    unsigned int state = Integer("Start: index of start state");
    stateConjunction.push_back(state);
    while (token.kind == HOALexer::TOKEN_AND) {
      expect(HOALexer::TOKEN_AND);
      state = Integer("Start: index of start state in conjunction");
      stateConjunction.push_back(state);
    }

    consumer->addStartStates(stateConjunction);
  }

  /** Grammar rule for the AP-header */
  void HeaderItemAP() {
    expect(HOALexer::TOKEN_AP);

    unsigned int apCount = Integer("AP: number of atomic propositions");

    std::vector<std::string> apList;
    std::set<std::string> aps;

    while (token.kind == HOALexer::TOKEN_STRING) {
      std::string ap = QuotedString();
      if (aps.find(ap) != aps.end()) {
        throw HOAConsumerException("Atomic proposition \""+ap+"\" is a duplicate!");
      }
      aps.insert(ap);
      apList.push_back(ap);
    }

    if (apList.size() != apCount) {
      throw HOAConsumerException("Number of provided APs (" + std::to_string(apList.size()) + ") does not match number of APs that was specified (" + std::to_string(apCount) + ")");
    }

    consumer->setAPs(apList);
  }

  /** Grammar rule for the Alias-header */
  void HeaderItemAlias() {
    expect(HOALexer::TOKEN_ALIAS);
    std::string aliasName = AliasName();

    HOAConsumer::label_expr::ptr labelExpr = LabelExpr();

    consumer->addAlias(aliasName, labelExpr);
  }

  /** Grammar rule for the Acceptance-header */
  void HeaderItemAcceptance() {
    expect(HOALexer::TOKEN_ACCEPTANCE);
    unsigned int numberOfSets = Integer("Acceptance: number of acceptance sets");

    HOAConsumer::acceptance_expr::ptr accExpr = AcceptanceCondition();

    consumer->setAcceptanceCondition(numberOfSets, accExpr);
  }

  /** Grammar rule for the acc-name-header */
  void HeaderItemAccName() {
    expect(HOALexer::TOKEN_ACCNAME);

    std::string accName =  Identifier("acceptance name");
    std::vector<IntOrString> extraInfo;

    while (true) {
      if (token.kind == HOALexer::TOKEN_IDENT) {
        extraInfo.push_back(IntOrString(Identifier()));
      } else if (token.kind == HOALexer::TOKEN_INT) {
        extraInfo.push_back(IntOrString(Integer()));
      } else if (token.kind == HOALexer::TOKEN_TRUE) {
        extraInfo.push_back(IntOrString("t"));
        expect(HOALexer::TOKEN_TRUE); // munch
      } else if (token.kind == HOALexer::TOKEN_FALSE) {
        extraInfo.push_back(IntOrString("f"));
        expect(HOALexer::TOKEN_FALSE); // munch
      } else {
        break;
      }

      // TODO
      // if (settings == null || !settings.getFlagIgnoreAccName()) {
      //	    consumer.provideAcceptanceName(accName, extraInfo);
      //}
    }

    consumer->provideAcceptanceName(accName, extraInfo);
  }

  /** Grammar rule for the tool-header */
  void HeaderItemTool() {
    expect(HOALexer::TOKEN_TOOL);

    std::string tool = QuotedString();
    std::shared_ptr<std::string> version;

    if (token.kind == HOALexer::TOKEN_STRING) {
      version.reset(new std::string(QuotedString()));
    }

    consumer->setTool(tool, version);
  }

  /** Grammar rule for the name-header */
  void HeaderItemName() {
    expect(HOALexer::TOKEN_NAME);

    std::string name = QuotedString();

    consumer->setName(name);
  }

  /** Grammar rule for the properties-header */
  void HeaderItemProperties() {
    expect(HOALexer::TOKEN_PROPERTIES);

    std::vector<std::string> properties;
    while (true) {
      if (token.kind == HOALexer::TOKEN_IDENT) {
        std::string property = Identifier();
        properties.push_back(property);
      } else if (token.kind == HOALexer::TOKEN_TRUE) {
        // t does not have the special boolean meaning here, back to string
        properties.push_back("t");
        expect(HOALexer::TOKEN_TRUE); // eat
      } else if (token.kind == HOALexer::TOKEN_FALSE) {
        // f does not have the special boolean meaning here, back to string
        properties.push_back("f");
        expect(HOALexer::TOKEN_FALSE); // eat
      } else {
        // no more properties...
        break;
      }
    }

    consumer->addProperties(properties);
  }

  /** Grammar rule for a misc header (not known from the format specification) */
  void HeaderMiscItem() {
    std::string headerName = token.vString;
    headerName = headerName.substr(0, headerName.length()-1);
    expect(HOALexer::TOKEN_HEADER_NAME);

    std::vector<IntOrString> content;

    while (true) {
      if (token.kind == HOALexer::TOKEN_INT) {
        content.push_back(Integer());
      } else if (token.kind == HOALexer::TOKEN_IDENT) {
        content.push_back(Identifier());
      } else if (token.kind == HOALexer::TOKEN_STRING) {
        content.push_back(IntOrString(QuotedString(), true));
      } else if (token.kind == HOALexer::TOKEN_TRUE) {
        // t does not have the special boolean meaning here, back to string
        content.push_back(IntOrString("t", false));
        expect(HOALexer::TOKEN_TRUE); // eat
      } else if (token.kind == HOALexer::TOKEN_FALSE) {
        // f does not have the special boolean meaning here, back to string
        content.push_back(IntOrString("f", false));
        expect(HOALexer::TOKEN_FALSE); // eat
      } else {
        break;
      }
    }

    consumer->addMiscHeader(headerName, content);
  }

  /** Grammar rule for the automaton body */
  void Body() {
    while (true) {
      switch (token.kind) {
      case HOALexer::TOKEN_STATE:
        StateName();
        break;
      case HOALexer::TOKEN_END:
        return;
      case HOALexer::TOKEN_EOF:
        return;
      default:
        if (inState) {
          Edge();
        } else {
          throw error("either State: or --END--");
        }
      }
    }
  }

  /** Grammar rule for the State definition */
  void StateName() {
    expect(HOALexer::TOKEN_STATE);

    HOAConsumer::label_expr::ptr labelExpr;
    std::shared_ptr<std::string> stateComment;
    std::shared_ptr<HOAConsumer::int_list> accSignature;

    if (token.kind == HOALexer::TOKEN_LBRACKET) {
      labelExpr = Label();
    }

    unsigned int state = Integer();     // name of the state
    if (token.kind == HOALexer::TOKEN_STRING) {
      stateComment.reset(new std::string(QuotedString())); // state comment
    }

    if (token.kind == HOALexer::TOKEN_LCURLY) {
      accSignature = AcceptanceSignature();
    }

    if (inState) {
      consumer->notifyEndOfState(currentState);
    }

    consumer->addState(state, stateComment, labelExpr, accSignature);

    // store global information:
    inState = true;
    currentState = state;
    currentStateHasStateLabel = (bool)(labelExpr);
  }

  /** Grammar rule for an automaton edge */
  void Edge() {
    HOAConsumer::label_expr::ptr labelExpr;
    std::shared_ptr<HOAConsumer::int_list> conjStates;
    std::shared_ptr<HOAConsumer::int_list> accSignature;

    if (token.kind == HOALexer::TOKEN_LBRACKET) {
      labelExpr = Label();
    }

    conjStates = StateConjunction("edge");

    if (token.kind == HOALexer::TOKEN_LCURLY) {
      accSignature = AcceptanceSignature();
    }

    if (labelExpr || currentStateHasStateLabel) {
      consumer->addEdgeWithLabel(currentState, labelExpr, *conjStates, accSignature);
    } else {
      consumer->addEdgeImplicit(currentState, *conjStates, accSignature);
    }
  }

  /**
   * Grammar rule for a state conjunction
   * @param context contextual information for error messages
   */
  std::shared_ptr<HOAConsumer::int_list> StateConjunction(const std::string& context) {
    std::shared_ptr<HOAConsumer::int_list> stateConjunction (new HOAConsumer::int_list());

    unsigned int state = Integer(context);
    stateConjunction->push_back(state);
    while (token.kind == HOALexer::TOKEN_AND) {
      expect(HOALexer::TOKEN_AND);
      state = Integer(context);
      stateConjunction->push_back(state);
    }
    return stateConjunction;
  }

  /** Grammar rule for a [label-expr] */
  HOAConsumer::label_expr::ptr Label() {
    HOAConsumer::label_expr::ptr result;
    expect(HOALexer::TOKEN_LBRACKET);
    result = LabelExpr();
    expect(HOALexer::TOKEN_RBRACKET);
    return result;
  }

  /** Grammar rule for an acceptance signature */
  std::shared_ptr<HOAConsumer::int_list> AcceptanceSignature() {
    std::shared_ptr<HOAConsumer::int_list> result(new HOAConsumer::int_list());

    expect(HOALexer::TOKEN_LCURLY);
    while (token.kind == HOALexer::TOKEN_INT) {
      unsigned int accSet = Integer();
      result->push_back(accSet);
    }
    expect(HOALexer::TOKEN_RCURLY);

    return result;
  }

  /** Grammar rule for an acceptance condition expression (handle disjunction)*/
  HOAConsumer::acceptance_expr::ptr AcceptanceCondition() {
    HOAConsumer::acceptance_expr::ptr left = AcceptanceConditionAnd();
    while (token.kind == HOALexer::TOKEN_OR) {
      expect(HOALexer::TOKEN_OR);

      HOAConsumer::acceptance_expr::ptr right = AcceptanceConditionAnd();
      left = left | right;
    }
    return left;
  }

  /** Grammar rule for conjunction in an acceptance condition */
  HOAConsumer::acceptance_expr::ptr AcceptanceConditionAnd() {
    HOAConsumer::acceptance_expr::ptr left = AcceptanceConditionAtom();
    while (token.kind == HOALexer::TOKEN_AND) {
      expect(HOALexer::TOKEN_AND);

      HOAConsumer::acceptance_expr::ptr right = AcceptanceConditionAtom();
      left = left & right;
    }
    return left;
  }

  /** Grammar rule for the atoms in an acceptance condition */
  HOAConsumer::acceptance_expr::ptr AcceptanceConditionAtom() {
    HOAConsumer::acceptance_expr::ptr result;

    switch (token.kind) {
    case HOALexer::TOKEN_LPARENTH:
      expect(HOALexer::TOKEN_LPARENTH);
      result = AcceptanceCondition();
      expect(HOALexer::TOKEN_RPARENTH);
      return result;
    case HOALexer::TOKEN_TRUE:
      expect(HOALexer::TOKEN_TRUE);
      result.reset(new HOAConsumer::acceptance_expr(true));
      return result;
    case HOALexer::TOKEN_FALSE:
      expect(HOALexer::TOKEN_FALSE);
      result.reset(new HOAConsumer::acceptance_expr(false));
      return result;
    case HOALexer::TOKEN_IDENT:
      result.reset(new HOAConsumer::acceptance_expr(AcceptanceConditionTemporalOperator()));
      return result;
    default:
      throw error("acceptance condition");
    }
  }

  /** Grammar rule for a temporal operator (Fin/Inf) in an acceptance condition */
  AtomAcceptance::ptr AcceptanceConditionTemporalOperator() {
    AtomAcceptance::AtomType atomType = AtomAcceptance::TEMPORAL_FIN;
    bool negated = false;
    unsigned int accSetIndex;

    std::string temporalOperator = Identifier();

    if (temporalOperator == "Fin") {
      atomType = AtomAcceptance::TEMPORAL_FIN;
    } else if (temporalOperator == "Inf") {
      atomType = AtomAcceptance::TEMPORAL_INF;
    } else {
      throw error("either 'Fin' or 'Inf'", "acceptance condition");
    }

    expect(HOALexer::TOKEN_LPARENTH, "acceptance condition");
    if (token.kind == HOALexer::TOKEN_NOT) {
      expect(HOALexer::TOKEN_NOT);
      negated = true;
    }
    accSetIndex = Integer("acceptance set index");
    expect(HOALexer::TOKEN_RPARENTH, "acceptance condition");

    return AtomAcceptance::ptr(new AtomAcceptance(atomType, accSetIndex, negated));
  }

  /** Grammar rule for a label expression (handle disjunction) */
  HOAConsumer::label_expr::ptr LabelExpr() {
    HOAConsumer::label_expr::ptr left = LabelExprAnd();
    while (token.kind == HOALexer::TOKEN_OR) {
      expect(HOALexer::TOKEN_OR);

      HOAConsumer::label_expr::ptr right = LabelExprAnd();
      left = left | right;
    }
    return left;
  }

  /** Grammar rule for a label expression (handle conjunction) */
  HOAConsumer::label_expr::ptr LabelExprAnd() {
    HOAConsumer::label_expr::ptr left = LabelExprAtom();
    while (token.kind == HOALexer::TOKEN_AND) {
      expect(HOALexer::TOKEN_AND);

      HOAConsumer::label_expr::ptr right = LabelExprAtom();
      left = left & right;
    }
    return left;
  }

  /** Grammar rule for a label expression (handle atoms) */
  HOAConsumer::label_expr::ptr LabelExprAtom() {
    HOAConsumer::label_expr::ptr result;

    switch (token.kind) {
    case HOALexer::TOKEN_LPARENTH:
      expect(HOALexer::TOKEN_LPARENTH);
      result = LabelExpr();
      expect(HOALexer::TOKEN_RPARENTH);
      return result;
    case HOALexer::TOKEN_TRUE:
      expect(HOALexer::TOKEN_TRUE);
      result.reset(new HOAConsumer::label_expr(true));
      return result;
    case HOALexer::TOKEN_FALSE:
      expect(HOALexer::TOKEN_FALSE);
      result.reset(new HOAConsumer::label_expr(false));
      return result;
    case HOALexer::TOKEN_NOT:
      expect(HOALexer::TOKEN_NOT);
      result = LabelExprAtom();
      return !result;
    case HOALexer::TOKEN_INT: {
      unsigned int apIndex = Integer();
      result.reset(new HOAConsumer::label_expr(AtomLabel::createAPIndex(apIndex)));
      return result;
    }
    case HOALexer::TOKEN_ALIAS_NAME: {
      std::string aliasName = AliasName();
      result.reset(new HOAConsumer::label_expr(AtomLabel::createAlias(aliasName)));
      return result;
    }
    default:
      throw error("label expression");
    }
  }

  /** Grammar rule for a quoted string */
  std::string QuotedString(const std::string& context="") {
    if (token.kind != HOALexer::TOKEN_STRING) {
      expect(HOALexer::TOKEN_STRING, context);
    }

    std::string result = token.vString;
    // eat token
    nextToken();

    result = HOAParserHelper::unquote(result);

    return result;
  }

  /** Grammar rule for a HOA identifier */
  std::string Identifier(const std::string& context="") {
    if (token.kind != HOALexer::TOKEN_IDENT) {
      expect(HOALexer::TOKEN_IDENT, context);
    }

    std::string result = token.vString;
    // eat token
    nextToken();

    return result;
  }

  /** Grammar rule for an @@alias-name. Returns the name without the leading @@. */
  std::string AliasName() {
    if (token.kind != HOALexer::TOKEN_ALIAS_NAME) {
      expect(HOALexer::TOKEN_ALIAS_NAME);
    }

    std::string result = token.vString;
    // eat token
    nextToken();

    // eat @
    result = result.substr(1);

    return result;
  }

  /** Grammar rule for an unsigned integer */
  unsigned int Integer(const std::string& context="") {
    if (token.kind != HOALexer::TOKEN_INT) {
      expect(HOALexer::TOKEN_INT, context);
    }

    unsigned int result = token.vInteger;
    // eat token
    nextToken();

    return result;
  }
};

}

#endif
