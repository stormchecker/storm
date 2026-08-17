#pragma once

// Very ugly, but currently we would like to have the symbol table here.
#include "storm-parsers/parser/SpiritParserDefinitions.h"

#include "storm/adapters/RationalNumberAdapter.h"  // TODO: use forward header

namespace storm {

namespace expressions {
class Expression;
class ExpressionManager;
enum struct OperatorType;
}  // namespace expressions

namespace parser {
class ExpressionCreator {
   public:
    ExpressionCreator(storm::expressions::ExpressionManager const& manager);

    ~ExpressionCreator();

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

    bool getAcceptDoubleLiterals() const {
        return acceptDoubleLiterals;
    }

    void setAcceptDoubleLiterals(bool set = true) {
        acceptDoubleLiterals = set;
    }

    storm::expressions::Expression createIteExpression(storm::expressions::Expression const& e1, storm::expressions::Expression const& e2,
                                                       storm::expressions::Expression const& e3, bool& pass) const;

    storm::expressions::Expression createOrExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                      storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createAndExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                       storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createRelationalExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                              storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createEqualsExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                          storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createPlusExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                        storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createMultExpression(storm::expressions::Expression const& e1, storm::expressions::OperatorType const& operatorType,
                                                        storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createPowerModuloLogarithmExpression(storm::expressions::Expression const& e1,
                                                                        storm::expressions::OperatorType const& operatorType,
                                                                        storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createUnaryExpression(std::vector<storm::expressions::OperatorType> const& operatorType,
                                                         storm::expressions::Expression const& e1, bool& pass) const;
    storm::expressions::Expression createRationalLiteralExpression(storm::RationalNumber const& value, bool& pass) const;
    storm::expressions::Expression createIntegerLiteralExpression(storm::RationalNumber const& value, bool& pass, bool& overflow) const;
    storm::expressions::Expression createBooleanLiteralExpression(bool value, bool& pass) const;
    storm::expressions::Expression createMinimumMaximumExpression(storm::expressions::Expression const& e1,
                                                                  storm::expressions::OperatorType const& operatorType,
                                                                  storm::expressions::Expression const& e2, bool& pass) const;
    storm::expressions::Expression createFloorCeilExpression(storm::expressions::OperatorType const& operatorType, storm::expressions::Expression const& e1,
                                                             bool& pass) const;
    storm::expressions::Expression createRoundExpression(storm::expressions::Expression const& e1, bool& pass) const;
    storm::expressions::Expression getIdentifierExpression(std::string const& identifier, bool& pass) const;
    storm::expressions::Expression createPredicateExpression(storm::expressions::OperatorType const& opTyp,
                                                             std::vector<storm::expressions::Expression> const& operands, bool& pass) const;

   private:
    // The manager responsible for the expressions.
    storm::expressions::ExpressionManager const& manager;
    qi::symbols<char, storm::expressions::Expression> const* identifiers = nullptr;

    bool createExpressions = false;

    bool acceptDoubleLiterals = true;

    bool deleteIdentifierMapping = false;
};
}  // namespace parser
}  // namespace storm
