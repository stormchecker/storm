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

#ifndef CPPHOAFPARSER_HOALEXER_H
#define CPPHOAFPARSER_HOALEXER_H

#include <map>
#include <string>
#include <stdexcept>

#include "cpphoafparser/parser/hoa_parser_exception.hh"

namespace cpphoafparser {

/** Lexer for tokenizing a HOA stream (used internally by HOAParser). */
class HOALexer {
public:
  /** The type of the tokens in a HOA stream. */
  enum TokenType {
    TOKEN_INT,
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_HEADER_NAME,
    TOKEN_ALIAS_NAME,

    TOKEN_EOF,

    TOKEN_BODY,
    TOKEN_END,
    TOKEN_ABORT,
    TOKEN_HOA,
    TOKEN_STATE,
    TOKEN_STATES,
    TOKEN_START,
    TOKEN_AP,
    TOKEN_ALIAS,
    TOKEN_ACCEPTANCE,
    TOKEN_ACCNAME,
    TOKEN_TOOL,
    TOKEN_NAME,
    TOKEN_PROPERTIES,

    // Punctuation, etc.
    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_LPARENTH,
    TOKEN_RPARENTH,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LCURLY,
    TOKEN_RCURLY,
    TOKEN_TRUE,
    TOKEN_FALSE
  };

  /** A token in the HOA stream. */
  struct Token {
    /** The kind of the token. */
    TokenType kind;
    /** The string representation of this token (if applicable) */
    std::string vString;
    /** The integer representation of this token (if applicable) */
    unsigned int vInteger;

    /** The line where this token started */
    unsigned int line;
    /** The column where this token started */
    unsigned int col;

    /** EOF (end-of-file) constructor. */
    Token() : kind(TOKEN_EOF), vString(""), vInteger(0), line(0), col(0) {}
    /** Constructor for syntactic element */
    Token(TokenType kind, unsigned int line, unsigned int col) : kind(kind), vString(""), vInteger(0), line(line), col(col) {}
    /** Constructor for a token having variable string content (e.g., TOKEN_IDENTIFIER, TOKEN_ALIAS, TOKEN_STRING, ...) */
    Token(TokenType kind, const std::string vString, unsigned int line, unsigned int col) : kind(kind), vString(vString), vInteger(0), line(line), col(col) {}
    /** Constructor for an unsigned integer token */
    Token(unsigned int vInteger, unsigned int line, unsigned int col) : kind(TOKEN_INT), vString(""), vInteger(vInteger), line(line), col(col) {}

    /** Returns true if this token represents the end-of-file. */
    bool isEOF() const {return kind == TOKEN_EOF;}

    /** Returns a string name for the given token type. */
    static std::string typeAsString(TokenType kind) {
      switch (kind) {
      case TOKEN_INT: return std::string("INT");
      case TOKEN_IDENT: return std::string("IDENT");
      case TOKEN_STRING: return std::string("STRING");
      case TOKEN_HEADER_NAME: return std::string("HEADER_NAME");
      case TOKEN_ALIAS_NAME: return std::string("ALIAS_NAME");

      case TOKEN_EOF: return std::string("EOF");

      case TOKEN_BODY: return std::string("BODY");
      case TOKEN_END: return std::string("END");
      case TOKEN_ABORT: return std::string("ABORT");
      case TOKEN_HOA: return std::string("HOA");
      case TOKEN_STATE: return std::string("STATE");
      case TOKEN_STATES: return std::string("STATES");
      case TOKEN_START: return std::string("START");
      case TOKEN_AP: return std::string("AP");
      case TOKEN_ALIAS: return std::string("ALIAS");
      case TOKEN_ACCEPTANCE: return std::string("ACCEPTANCE");
      case TOKEN_ACCNAME: return std::string("ACCNAME");
      case TOKEN_TOOL: return std::string("TOOL");
      case TOKEN_NAME: return std::string("NAME");
      case TOKEN_PROPERTIES: return std::string("PROPERTIES");

      // Punctuation: etc.
      case TOKEN_NOT: return std::string("NOT");
      case TOKEN_AND: return std::string("AND");
      case TOKEN_OR: return std::string("OR");
      case TOKEN_LPARENTH: return std::string("LPARENTH");
      case TOKEN_RPARENTH: return std::string("RPARENTH");
      case TOKEN_LBRACKET: return std::string("LBRACKET");
      case TOKEN_RBRACKET: return std::string("RBRACKET");
      case TOKEN_LCURLY: return std::string("LCURLY");
      case TOKEN_RCURLY: return std::string("RCURLY");
      case TOKEN_TRUE: return std::string("TRUE");
      case TOKEN_FALSE: return std::string("FALSE");
      }
      throw std::logic_error("Unhandled token type");
    }

    /** Returns a string name for the given token type (for use in error messages). */
    static std::string forErrorMessage(TokenType kind) {
      switch (kind) {
      case TOKEN_INT: return std::string("INTEGER");
      case TOKEN_IDENT: return std::string("IDENTIFIER");
      case TOKEN_STRING: return std::string("STRING");
      case TOKEN_HEADER_NAME: return std::string("HEADER_NAME");
      case TOKEN_ALIAS_NAME: return std::string("ALIAS_NAME");

      case TOKEN_EOF: return std::string("END-OF_FILE");

      case TOKEN_BODY: return std::string("--BODY--");
      case TOKEN_END: return std::string("--END--");
      case TOKEN_ABORT: return std::string("--ABORT--");
      case TOKEN_HOA: return std::string("HOA:");
      case TOKEN_STATE: return std::string("State:");
      case TOKEN_STATES: return std::string("States:");
      case TOKEN_START: return std::string("Start:");
      case TOKEN_AP: return std::string("AP:");
      case TOKEN_ALIAS: return std::string("Alias:");
      case TOKEN_ACCEPTANCE: return std::string("Acceptance:");
      case TOKEN_ACCNAME: return std::string("acc-name:");
      case TOKEN_TOOL: return std::string("tool:");
      case TOKEN_NAME: return std::string("name:");
      case TOKEN_PROPERTIES: return std::string("properties:");

      // Punctuation: etc.
      case TOKEN_NOT: return std::string("!");
      case TOKEN_AND: return std::string("&");
      case TOKEN_OR: return std::string("|");
      case TOKEN_LPARENTH: return std::string("(");
      case TOKEN_RPARENTH: return std::string(")");
      case TOKEN_LBRACKET: return std::string("[");
      case TOKEN_RBRACKET: return std::string("]");
      case TOKEN_LCURLY: return std::string("{");
      case TOKEN_RCURLY: return std::string("}");
      case TOKEN_TRUE: return std::string("t");
      case TOKEN_FALSE: return std::string("f");
      }
      throw std::logic_error("Unhandled token type");
    }

    /** Returns a string representation of a given token (for error messages). */
    static std::string forErrorMessage(Token token) {
      switch (token.kind) {
      case TOKEN_INT: return std::string("INTEGER ")+std::to_string(token.vInteger);
      case TOKEN_IDENT: return std::string("IDENTIFIER ")+token.vString;
      case TOKEN_STRING: return std::string("STRING ")+token.vString;
      case TOKEN_HEADER_NAME: return std::string("HEADER ")+token.vString;
      case TOKEN_ALIAS_NAME: return std::string("ALIAS ")+token.vString;

      case TOKEN_EOF: return std::string("END-OF-FILE");

      case TOKEN_BODY: return std::string("--BODY--");
      case TOKEN_END: return std::string("--END--");
      case TOKEN_ABORT: return std::string("--ABORT--");
      case TOKEN_HOA: return std::string("HEADER HOA");
      case TOKEN_STATES: return std::string("HEADER States");
      case TOKEN_START: return std::string("HEADERr Start");
      case TOKEN_AP: return std::string("HEADER AP");
      case TOKEN_ALIAS: return std::string("HEADER Alias");
      case TOKEN_ACCEPTANCE: return std::string("HEADER Acceptance");
      case TOKEN_ACCNAME: return std::string("HEADER acc-name");
      case TOKEN_TOOL: return std::string("HEADER tool");
      case TOKEN_NAME: return std::string("HEADER name");
      case TOKEN_PROPERTIES: return std::string("HEADER properties");

      case TOKEN_STATE: return std::string("DEFINITION State");

      // Punctuation: etc.
      case TOKEN_NOT: return std::string("!");
      case TOKEN_AND: return std::string("&");
      case TOKEN_OR: return std::string("|");
      case TOKEN_LPARENTH: return std::string("(");
      case TOKEN_RPARENTH: return std::string(")");
      case TOKEN_LBRACKET: return std::string("[");
      case TOKEN_RBRACKET: return std::string("]");
      case TOKEN_LCURLY: return std::string("{");
      case TOKEN_RCURLY: return std::string("}");
      case TOKEN_TRUE: return std::string("TRUE t");
      case TOKEN_FALSE: return std::string("FALSE f");
      }
      throw std::logic_error("Unhandled token type");
    }

    /** Output function for a given token. */
    friend std::ostream& operator<<(std::ostream& out, const Token& token) {
      out << "<" << token.typeAsString(token.kind) << "> ";
      if (token.kind == TOKEN_INT) {
        out << token.vInteger;
      } else {
        out << token.vString;
      }
      out << "     (" << token.line << "," << token.col << ")";
      return out;
    }
  };

  /** Constructor for a lexer, reading from the given input stream. */
  HOALexer(std::istream& in) 
  : in(in), line(1), col(0), ch(0) {
    // The headers we know
    knownHeaders["HOA:"] = TOKEN_HOA;    
    knownHeaders["State:"] = TOKEN_STATE;
    knownHeaders["States:"] = TOKEN_STATES;
    knownHeaders["Start:"] = TOKEN_START;
    knownHeaders["AP:"] = TOKEN_AP;
    knownHeaders["Alias:"] = TOKEN_ALIAS;
    knownHeaders["Acceptance:"] = TOKEN_ACCEPTANCE;
    knownHeaders["acc-name:"] = TOKEN_ACCNAME;
    knownHeaders["tool:"] = TOKEN_TOOL;
    knownHeaders["name:"] = TOKEN_NAME;
    knownHeaders["properties:"] = TOKEN_PROPERTIES;
  }

  /** Get the next token from the input stream. */
  Token nextToken() {
    // first, skip any whitespace
    skip();
    if (ch == EOF) return Token(TOKEN_EOF, line, col);

    // handle the simple syntactic elements
    switch (ch) {
    case '!': return Token(TOKEN_NOT, line, col);
    case '&': return Token(TOKEN_AND, line, col);
    case '|': return Token(TOKEN_OR, line, col);
    case '(': return Token(TOKEN_LPARENTH, line, col);
    case ')': return Token(TOKEN_RPARENTH, line, col);
    case '[': return Token(TOKEN_LBRACKET, line, col);
    case ']': return Token(TOKEN_RBRACKET, line, col);
    case '{': return Token(TOKEN_LCURLY, line, col);
    case '}': return Token(TOKEN_RCURLY, line, col);
    }

    // remember where the token began
    unsigned int lineStart = line;
    unsigned int colStart = col;

    // handle --XYZ-- style markers
    if (ch == '-') {
      unsigned int index=0;
      bool canBeAbort = true;
      bool canBeBody  = true;
      bool canBeEnd   = true;
      std::string abort("-ABORT--");
      std::string body("-BODY--");
      std::string end("-END--");

      while (canBeAbort || canBeBody || canBeEnd) {
        nextChar();
        if (ch == EOF) {throw error("Premature end-of-file inside token", lineStart, colStart);}
        if (canBeAbort && ch == abort.at(index)) {
          if (index == abort.length()-1) {
            return Token(TOKEN_ABORT, lineStart, colStart);
          }
        } else {
          canBeAbort=false;
        }
        if (canBeBody && ch == body.at(index)) {
          if (index == body.length()-1) {
            return Token(TOKEN_BODY, lineStart, colStart);
          }
        } else {
          canBeBody=false;
        }
        if (canBeEnd && ch == end.at(index)) {
          if (index == end.length()-1) {
            return Token(TOKEN_END, lineStart, colStart);
          }
        } else {
          canBeEnd=false;
        }

        index++;
        if (index >= abort.length()) canBeAbort = false;
        if (index >= body.length()) canBeBody = false;
        if (index >= end.length()) canBeEnd = false;
      }
      throw error("Lexical error: For token starting with '-', expected either '--BODY--', '--END--' or '--ABORT--'", lineStart, colStart);
    }

    // handle quoted strings
    if (ch == '"') {
      std::string text(1, (char)ch);
      bool last_was_quote = false;
      while (true) {
        nextChar();
        if (ch == EOF) {throw error("Premature end-of-file in quoted string", lineStart, colStart);}
        text+=(char)ch;
        if (ch == '"' && !last_was_quote) break;
        if (ch == '\\' && !last_was_quote) {
          last_was_quote = true;
        } else {
          last_was_quote = false;
        }
      }

      return Token(TOKEN_STRING, text, lineStart, colStart);
    }

    // handle integers
    if (ch >= '0' && ch <= '9') {
      std::string text(1, (char)ch);
      while (true) {
        int next = peekChar();
        if (next >= '0' && next <= '9') {
          nextChar();
          text+=(char)ch;
        } else {
          break;
        }
      }

      if (text.at(0)=='0' && text.length() > 1) {
        throw error("Syntax error parsing integer, starts with 0: "+text, lineStart, colStart);
      }

      try {
        unsigned int vInteger = std::stoi(text);
        return Token(vInteger, lineStart, colStart);
      } catch (std::invalid_argument& e) {
        throw error("Syntax error: "+text+" is not an integer", lineStart, colStart);
      } catch (std::out_of_range& e) {
        throw error("Syntax error: integer "+text+" is too big to represent as an unsigned int", lineStart, colStart);
      }

    } else if (ch == '@' || ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
      // handle identifiers, @alias-names, headers, t and f
      std::string text(1, (char)ch);

      bool alias = (ch == '@');
      while (true) {
        int next = peekChar();
        if (next == EOF) break;
        if (next == ':') {
          if (alias) break;
          // consume ':'
          nextChar();
          text+=':';
          break;
        }
        if (next == '_' ||
            next == '-' ||
            (next >= 'a' && next <= 'z') ||
            (next >= 'A' && next <= 'Z') ||
            (next >= '0' && next <= '9')) {
          nextChar();
          text+=(char)ch;
          continue;
        } else {
          break;
        }
      }

      if (alias) {
        return Token(TOKEN_ALIAS_NAME, text, lineStart, colStart);
      }

      if (text.back() == ':') {
        auto it = knownHeaders.find(text);
        if (it != knownHeaders.end()) {
          return Token((*it).second, text, lineStart, colStart);
        }
        return Token(TOKEN_HEADER_NAME, text, lineStart, colStart);
      }
      if (text == "t") {
        return Token(TOKEN_TRUE, text, lineStart, colStart);
      } else if (text == "f") {
        return Token(TOKEN_FALSE, text, lineStart, colStart);
      }
      return Token(TOKEN_IDENT, text, lineStart, colStart);
    }

    throw error("Syntax error, illegal character '"+std::string(1, (char)ch)+"'", lineStart, colStart);
  }

private:

  /** Skip whitespace. */
  void skip() {
    while (true) {
      nextChar();
      if (ch == EOF) { // EOF
        return;
      }
      if (ch == '/') {
        skipComment();
        continue;
      }
      if (ch == ' ' || ch == '\t') {
        continue;
      }
      if (ch == '\n' || ch == '\r') {
        line++;
        col=0;
        continue;
      }
      break;
    }
  }

  /** Skip a comment */
  void skipComment() {
    nextChar();
    if (ch != '*') {
      throw error("Malformed start of comment", line, col);
    }
    bool last_was_slash = false;
    bool last_was_star = false;
    unsigned int nesting = 0;
    while (true) {
      nextChar();
      if (ch == EOF) {throw error("End-of-file inside comment", line, col);}
      if (ch == '\n' || ch == '\r') {
        line++;
        col=0;
        last_was_slash = false;
        last_was_star = false;
        continue;
      }
      if (ch == '/') {
        if (last_was_star) {
          if (nesting == 0) {
            return;
          } else {
            nesting--;
          }
        } else {
          last_was_slash = true;
        }
        continue;
      }
      if (ch == '*') {
        if (last_was_slash) {
          nesting++;
        } else {
          last_was_star = true;
          continue;
        }
      }
      last_was_slash = false;
      last_was_star = false;
    }
  }

  /** Read the next char in the input stream, store in `ch` */
  void nextChar() {
    ch = in.get();
    if (ch != EOF) {
      col++;
    }
  }

  /** Peek at the next char in the input stream without consuming */
  int peekChar() {
    return in.peek();
  }

  /**
   * Construct a HOAParserExeption for a lexer error.
   * @param msg the error message
   * @param errLine the line number where the error occured
   * @param errCol column number where the error occured
   */
  HOAParserException error(const std::string& msg, unsigned int errLine, unsigned int errCol) {
    return HOAParserException(msg+" (at line "+std::to_string(errLine)+", col "+std::to_string(errCol)+")", errLine, errCol);
  }

private:
  /** The input stream */
  std::istream& in;
  /** The current line number */
  unsigned int line;
  /** The current column number */
  unsigned int col;
  /** The current character (or EOF) */
  int ch;

  /** A map for mapping the known header names to the corresponding token types */
  std::map<std::string, TokenType> knownHeaders;
};

}

#endif
