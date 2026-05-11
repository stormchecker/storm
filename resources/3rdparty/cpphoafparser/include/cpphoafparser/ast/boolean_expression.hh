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

#ifndef CPPHOAFPARSER_BOOLEANEXPRESSION_H
#define CPPHOAFPARSER_BOOLEANEXPRESSION_H

#include <memory>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace cpphoafparser {

/**
 * This represents (a node of) an abstract syntax tree
 * of a boolean expression, parametrized with the type
 * of leaf nodes (atoms).
 *
 * The nodes are designed to be immutable, which allows
 * sharing of subexpression in a safe way between multiple
 * trees.
 *
 * For unary operator (NOT), the child is stored as the
 * left child of the not.
 *
 * With AtomLabel, this represents a label expression
 * over atomic propositions, with AtomAcceptance an
 * expression of Fin/Inf acceptance conditions.
 *
 * @tparam <Atoms> The atoms (leaf nodes) in the abstract syntax tree.
 */
template <typename Atoms>
class BooleanExpression {
public:
  /** A shared_ptr to a node in this AST */
  typedef std::shared_ptr< BooleanExpression<Atoms> > ptr;
  /** A shared_ptr to an atom (leaf node) in this AST */
  typedef std::shared_ptr< Atoms> atom_ptr;

  /** The node types of this AST */
  enum OperatorType {
    EXP_AND,
    EXP_OR,
    EXP_NOT,
    EXP_TRUE,
    EXP_FALSE,
    EXP_ATOM
  };

  /** Get the node type for this node */
  OperatorType getType() {
    return kind;
  }

  /** Static constructor for a node representing a TRUE leaf */
  static ptr True() {return ptr(new BooleanExpression(true));}
  /** Static constructor for a node representing a FALSE leaf */
  static ptr False() {return ptr(new BooleanExpression(false));}
  /** Static constructor for a node representing an atom leaf */
  static ptr Atom(atom_ptr atom) {return ptr(new BooleanExpression(atom));}

  /**
   * Constructor for a node, providing the type and left and right child nodes.
   *
   * For unary operators, the `right` node should be an empty pointer.
   **/
  BooleanExpression(OperatorType kind, ptr left, ptr right) :
    kind(kind), left(left), right(right), atom(nullptr) {
  }

  /**
   * Constructor for a TRUE/FALSE leaf node.
   */
  BooleanExpression(bool value) : 
    left(nullptr), right(nullptr), atom(nullptr) {
    if (value) {
      kind = EXP_TRUE;
    } else {
      kind = EXP_FALSE;
    }
  }

  /** Constructor for an atom node */
  BooleanExpression(atom_ptr atom) :
    kind(EXP_ATOM), left(nullptr), right(nullptr), atom(atom) {
  }

  /** Constructor for an atom node (copies atom) */
  BooleanExpression(const Atoms& atom) :
    kind(EXP_ATOM), left(nullptr), right(nullptr), atom(new Atoms(atom)) {
  }

  /** Perform a deep copy (recursive) of this AST and return the result */
  ptr deepCopy() {
    switch (kind) {
    case EXP_AND:
      return ptr(new BooleanExpression(EXP_AND, left->deepCopy(), right->deepCopy()));
    case EXP_OR:
      return ptr(new BooleanExpression(EXP_OR, left->deepCopy(), right->deepCopy()));
    case EXP_NOT: 
      return ptr(new BooleanExpression(EXP_NOT, left->deepCopy(), nullptr));
    case EXP_TRUE:
      return True();
    case EXP_FALSE:
      return False();
    case EXP_ATOM:
      return ptr(new BooleanExpression(*atom));
    }
    throw std::logic_error("Unsupported operator");
  }

  /** Get the left child node (might be an empty pointer) */
  ptr getLeft() const {return left;}
  /** Get the right child node (might be an empty pointer) */
  ptr getRight() const {return right;}
  /** Get the atom for an EXP_ATOM node. May only be called if `isAtom() == true` */
  const Atoms& getAtom() const {
    if (!isAtom()) throw std::logic_error("Illegal access");
    return *atom;
  }

  /** Returns true if this node is an EXP_AND node */
  bool isAND() const {return kind==EXP_AND;}
  /** Returns true if this node is an EXP_OR node */
  bool isOR() const {return kind==EXP_OR;}
  /** Returns true if this node is an EXP_NOT node */
  bool isNOT() const {return kind==EXP_NOT;}
  /** Returns true if this node is an EXP_TRUE node */
  bool isTRUE() const {return kind==EXP_TRUE;}
  /** Returns true if this node is an EXP_FALSE node */
  bool isFALSE() const {return kind==EXP_FALSE;}
  /** Returns true if this node is an EXP_ATOM node */
  bool isAtom() const {return kind==EXP_ATOM;}

  /** Conjunction operator */
  friend ptr operator&(ptr left, ptr right) {
    return ptr(new BooleanExpression<Atoms>(EXP_AND, left, right));
  }

  /** Disjunction operator */
  friend ptr operator|(ptr left, ptr right) {
    return ptr(new BooleanExpression<Atoms>(EXP_OR, left, right));
  }

  /** Negation operator */
  friend ptr operator!(ptr other) {
    return ptr(new BooleanExpression<Atoms>(EXP_NOT, other, nullptr));
  }

  /** Output operator, renders in HOA syntax */
  friend std::ostream& operator<<(std::ostream& out, const BooleanExpression<Atoms>& expr) {
    switch (expr.kind) {
    case EXP_AND: {
      bool paren = expr.left->needsParentheses(EXP_AND);
      if (paren) out << "(";
      out << *expr.left;
      if (paren) out << ")";

      out << " & ";

      paren = expr.right->needsParentheses(EXP_AND);
      if (paren) out << "(";
      out << *expr.right;
      if (paren) out << ")";
      return out;
    }
    case EXP_OR: {
      bool paren = expr.left->needsParentheses(EXP_OR);
      if (paren) out << "(";
      out << *expr.left;
      if (paren) out << ")";

      out << " | ";

      paren = expr.right->needsParentheses(EXP_OR);
      if (paren) out << "(";
      out << *expr.right;
      if (paren) out << ")";
      return out;
    }
    case EXP_NOT: {
      bool paren = expr.left->needsParentheses(EXP_NOT);
      out << "!";
      if (paren) out << "(";
      out << *expr.left;
      if (paren) out << ")";
      return out;
    }
    case EXP_TRUE:
      out << "t";
      return out;
    case EXP_FALSE:
      out << "f";
      return out;
    case EXP_ATOM:
      out << *(expr.atom);
      return out;
    }
    throw std::logic_error("Unhandled operator");
  }

  /** Return a string representation of this AST (HOA syntax) */
  std::string toString() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  /**
   * Returns `true` if `expr1` and `expr2` are syntactically equal.
   * Two AST are syntactically equal if the trees match and the
   * atoms are equal.
   */
  static bool areSyntacticallyEqual(ptr expr1, ptr expr2)
  {
    if (!expr1.get() || !expr2.get()) return false;
    if (expr1->getType() != expr2->getType()) return false;

    switch (expr1->getType()) {
    case EXP_TRUE:
    case EXP_FALSE:
      return true;
    case EXP_AND:
    case EXP_OR:
      if (!areSyntacticallyEqual(expr1->getLeft(), expr2->getLeft())) return false;
      if (!areSyntacticallyEqual(expr1->getRight(), expr2->getRight())) return false;
      return true;
    case EXP_NOT:
      if (!areSyntacticallyEqual(expr1->getLeft(), expr2->getLeft())) return false;
      return true;
    case EXP_ATOM:
      return expr1->getAtom() == expr2->getAtom();
    }
    throw std::logic_error("Unknown operator in expression: "+expr1->toString());
}

private:
  /** The node type */
  OperatorType kind;
  /** The left child (if applicable) */
  ptr left;
  /** The right child (if applicable) */
  ptr right;
  /** The atom (if applicable) */
  atom_ptr atom;

  /**
   * Returns true if outputing this node in infix syntax needs parentheses,
   * if the operator above is of `enclosingType`
   */
  bool needsParentheses(OperatorType enclosingType) const {
    switch (kind) {
    case EXP_ATOM:
    case EXP_TRUE:
    case EXP_FALSE:
      return false;
    case EXP_AND:
      if (enclosingType==EXP_NOT) return true;
      if (enclosingType==EXP_AND) return false;
      if (enclosingType==EXP_OR) return false;
      break;
    case EXP_OR:
      if (enclosingType==EXP_NOT) return true;
      if (enclosingType==EXP_AND) return true;
      if (enclosingType==EXP_OR) return false;
      break;
    case EXP_NOT:
      return false;
    }
    throw std::logic_error("Unhandled operator");
  }

};

}

#endif
