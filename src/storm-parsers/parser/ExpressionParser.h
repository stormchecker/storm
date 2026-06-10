#pragma once

#include <cstdint>
#include <memory>

#include "storm-parsers/parser/SpiritErrorHandler.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/storage/expressions/OperatorType.h"

namespace storm {
namespace expressions {
class Expression;
class ExpressionManager;
}  // namespace expressions

namespace parser {
template<typename NumberType>
struct RationalPolicies : boost::spirit::qi::strict_real_policies<NumberType> {
    static const bool expect_dot = true;
    static const bool allow_leading_dot = true;
    static const bool allow_trailing_dot = false;

    template<typename It, typename Attr>
    static bool parse_nan(It&, It const&, Attr&) {
        return false;
    }
    template<typename It, typename Attr>
    static bool parse_inf(It&, It const&, Attr&) {
        return false;
    }
};

class ExpressionCreator;

class ExpressionParser : public qi::grammar<Iterator, storm::expressions::Expression(), Skipper> {
   public:
    /*!
     * Creates an expression parser. Initially the parser is set to a mode in which it will not generate the
     * actual expressions but only perform a syntax check and return the expression "false". To make the parser
     * generate the actual expressions, a mapping of valid identifiers to their expressions need to be provided
     * later.
     *
     * @param manager The manager responsible for the expressions.
     * @param invalidIdentifiers_ A symbol table of identifiers that are to be rejected.
     * @param enableErrorHandling Enables error handling within the parser. Note that this should should be set
     * to true when using the parser as the top level parser.
     * @param allowBacktracking A flag that indicates whether or not the parser is supposed to backtrack beyond
     * points it would typically allow. This can, for example, be used to prevent errors if the outer grammar
     * also parses boolean conjuncts that are erroneously consumed by the expression parser.
     */
    ExpressionParser(storm::expressions::ExpressionManager const& manager,
                     qi::symbols<char, uint_fast64_t> const& invalidIdentifiers_ = qi::symbols<char, uint_fast64_t>(), bool enableErrorHandling = true,
                     bool allowBacktracking = false);
    ~ExpressionParser();

    ExpressionParser(ExpressionParser const& other) = delete;
    ExpressionParser& operator=(ExpressionParser const& other) = delete;

    /*!
     * Sets an identifier mapping that is used to determine valid variables in the expression. The mapped-to
     * expressions will be substituted wherever the key value appears in the parsed expression. After setting
     * this, the parser will generate expressions.
     *
     * @param identifiers_ A pointer to a mapping from identifiers to expressions.
     */
    void setIdentifierMapping(qi::symbols<char, storm::expressions::Expression> const* identifiers_);

    /*!
     * Sets an identifier mapping that is used to determine valid variables in the expression. The mapped-to
     * expressions will be substituted wherever the key value appears in the parsed expression. After setting
     * this, the parser will generate expressions.
     *
     * @param identifierMapping A mapping from identifiers to expressions.
     */
    void setIdentifierMapping(std::unordered_map<std::string, storm::expressions::Expression> const& identifierMapping);

    /*!
     * Unsets a previously set identifier mapping. This will make the parser not generate expressions any more
     * but merely check for syntactic correctness of an expression.
     */
    void unsetIdentifierMapping();

    /*!
     * Sets whether double literals are to be accepted or not.
     *
     * @param flag If set to true, double literals are accepted.
     */
    void setAcceptDoubleLiterals(bool flag);

    /*!
     * Parses an expression from the given string.
     * @param ignoreError If set, no exception is thrown upon a parser error. The returned expression will be uninitialized.
     */
    storm::expressions::Expression parseFromString(std::string const& expressionString, bool ignoreError = false) const;

   private:
    struct orOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        orOperatorStruct() {
            add("|", storm::expressions::OperatorType::Or)("=>", storm::expressions::OperatorType::Implies);
        }
    };

    // A parser used for recognizing the operators at the "or" precedence level.
    orOperatorStruct orOperator_;

    struct andOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        andOperatorStruct() {
            add("&", storm::expressions::OperatorType::And);
        }
    };

    // A parser used for recognizing the operators at the "and" precedence level.
    andOperatorStruct andOperator_;

    struct equalityOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        equalityOperatorStruct() {
            add("=", storm::expressions::OperatorType::Equal)("!=", storm::expressions::OperatorType::NotEqual);
        }
    };

    // A parser used for recognizing the operators at the "equality" precedence level.
    equalityOperatorStruct equalityOperator_;

    struct relationalOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        relationalOperatorStruct() {
            add(">=", storm::expressions::OperatorType::GreaterOrEqual)(">", storm::expressions::OperatorType::Greater)(
                "<=", storm::expressions::OperatorType::LessOrEqual)("<", storm::expressions::OperatorType::Less);
        }
    };

    // A parser used for recognizing the operators at the "relational" precedence level.
    relationalOperatorStruct relationalOperator_;

    struct plusOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        plusOperatorStruct() {
            add("+", storm::expressions::OperatorType::Plus)("-", storm::expressions::OperatorType::Minus);
        }
    };

    // A parser used for recognizing the operators at the "plus" precedence level.
    plusOperatorStruct plusOperator_;

    struct multiplicationOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        multiplicationOperatorStruct() {
            add("*", storm::expressions::OperatorType::Times)("/", storm::expressions::OperatorType::Divide);
        }
    };

    // A parser used for recognizing the operators at the "multiplication" precedence level.
    multiplicationOperatorStruct multiplicationOperator_;

    struct infixPowerModuloOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        infixPowerModuloOperatorStruct() {
            add("^", storm::expressions::OperatorType::Power)("%", storm::expressions::OperatorType::Modulo);
        }
    };

    // A parser used for recognizing the operators at the "power" precedence level.
    infixPowerModuloOperatorStruct infixPowerModuloOperator_;

    struct unaryOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        unaryOperatorStruct() {
            add("!", storm::expressions::OperatorType::Not)("-", storm::expressions::OperatorType::Minus);
        }
    };

    // A parser used for recognizing the operators at the "unary" precedence level.
    unaryOperatorStruct unaryOperator_;

    struct floorCeilOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        floorCeilOperatorStruct() {
            add("floor", storm::expressions::OperatorType::Floor)("ceil", storm::expressions::OperatorType::Ceil);
        }
    };

    // A parser used for recognizing the operators at the "floor/ceil" precedence level.
    floorCeilOperatorStruct floorCeilOperator_;

    struct minMaxOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        minMaxOperatorStruct() {
            add("min", storm::expressions::OperatorType::Min)("max", storm::expressions::OperatorType::Max);
        }
    };

    // A parser used for recognizing the operators at the "min/max" precedence level.
    minMaxOperatorStruct minMaxOperator_;

    struct prefixPowerModuloLogarithmOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        prefixPowerModuloLogarithmOperatorStruct() {
            add("pow", storm::expressions::OperatorType::Power)("mod", storm::expressions::OperatorType::Modulo)("log",
                                                                                                                 storm::expressions::OperatorType::Logarithm);
        }
    };

    // A parser used for recognizing the operators at the "power" precedence level.
    prefixPowerModuloLogarithmOperatorStruct prefixPowerModuloLogarithmOperator_;

    struct predicateOperatorStruct : qi::symbols<char, storm::expressions::OperatorType> {
        predicateOperatorStruct() {
            add("atLeastOneOf", storm::expressions::OperatorType::AtLeastOneOf)("atMostOneOf", storm::expressions::OperatorType::AtMostOneOf)(
                "exactlyOneOf", storm::expressions::OperatorType::ExactlyOneOf);
        }
    };

    // A parser used for recognizing the operators at the "min/max" precedence level.
    predicateOperatorStruct predicateOperator_;

    std::unique_ptr<ExpressionCreator> expressionCreator;

    // The symbol table of invalid identifiers.
    qi::symbols<char, uint_fast64_t> invalidIdentifiers_;

    // Rules for parsing a composed expression.
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> expression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> iteExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> orExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> andExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> relativeExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> equalityExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> plusExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> multiplicationExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<bool>, Skipper> prefixPowerModuloLogarithmExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::Expression>, Skipper> infixPowerModuloExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> unaryExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> atomicExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> literalExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<bool>, Skipper> integerLiteralExpression;
    qi::rule<Iterator, qi::unused_type(bool), Skipper> integerOverflowHelperRule;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> identifierExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::OperatorType, storm::expressions::Expression>, Skipper>
        minMaxExpression;
    qi::rule<Iterator, storm::expressions::Expression(), qi::locals<storm::expressions::OperatorType>, Skipper> floorCeilExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> roundExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> predicateExpression;
    qi::rule<Iterator, std::string(), Skipper> identifier;

    // Parser that is used to recognize doubles only (as opposed to Spirit's double_ parser).
    boost::spirit::qi::real_parser<storm::RationalNumber, RationalPolicies<storm::RationalNumber>> floatLiteral_;
    boost::spirit::qi::int_parser<storm::RationalNumber> integerLiteral_;

    bool isValidIdentifier(std::string const& identifier);

    // An error handler function.
    phoenix::function<SpiritErrorHandler> handler;
};
}  // namespace parser
}  // namespace storm
