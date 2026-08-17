#pragma once

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/jani/Property.h"

namespace storm {
namespace prism {
class Program;
}

namespace logic {
class Formula;
}

namespace parser {

// Forward-declare grammar.
class FormulaParserGrammar;

class FormulaParser {
   public:
    FormulaParser();
    explicit FormulaParser(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager);
    explicit FormulaParser(std::shared_ptr<storm::expressions::ExpressionManager> const& manager);
    explicit FormulaParser(storm::prism::Program const& program);
    explicit FormulaParser(storm::prism::Program& program);

    FormulaParser(FormulaParser const& other);
    FormulaParser& operator=(FormulaParser const& other);

    /*!
     * Parses the formula given by the provided string.
     *
     * @param formulaString The formula as a string.
     * @return The resulting formula.
     */
    std::shared_ptr<storm::logic::Formula const> parseSingleFormulaFromString(std::string const& formulaString) const;

    /*!
     * Parses the property given by the provided string.
     *
     * @param propertyString The formula as a string.
     * @return The contained properties.
     */
    std::vector<storm::jani::Property> parseFromString(std::string const& propertyString) const;

    /*!
     * Parses the properties in the given file.
     *
     * @param filename The name of the file to parse.
     * @return The contained properties.
     */
    std::vector<storm::jani::Property> parseFromFile(std::string const& filename) const;

    /*!
     * Adds an identifier and the expression it is supposed to be replaced with. This can, for example be used
     * to substitute special identifiers in the formula by expressions.
     *
     * @param identifier The identifier that is supposed to be substituted.
     * @param expression The expression it is to be substituted with.
     */
    void addIdentifierExpression(std::string const& identifier, storm::expressions::Expression const& expression);

   private:
    // The manager used to parse expressions.
    std::shared_ptr<storm::expressions::ExpressionManager const> manager;

    // The grammar used to parse the input.
    std::shared_ptr<FormulaParserGrammar> grammar;
};

}  // namespace parser
}  // namespace storm
