#include "storm-parsers/parser/FormulaParser.h"

#include <fstream>

#include "storm-parsers/parser/SpiritErrorHandler.h"

#include "storm/storage/prism/Program.h"

#include "storm/logic/Formulas.h"

// If the parser fails due to ill-formed data, this exception is thrown.
#include "storm/exceptions/WrongFormatException.h"

#include "FormulaParserGrammar.h"
#include "storm/io/file.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace parser {

FormulaParser::FormulaParser() : manager(new storm::expressions::ExpressionManager()), grammar(new FormulaParserGrammar(manager)) {
    // Intentionally left empty.
}

FormulaParser::FormulaParser(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager)
    : manager(manager), grammar(new FormulaParserGrammar(manager)) {
    // Intentionally left empty.
}

FormulaParser::FormulaParser(std::shared_ptr<storm::expressions::ExpressionManager> const& manager)
    : manager(manager), grammar(new FormulaParserGrammar(manager)) {
    // Intentionally left empty.
}

FormulaParser::FormulaParser(storm::prism::Program const& program)
    : manager(program.getManager().getSharedPointer()), grammar(new FormulaParserGrammar(program.getManager().getSharedPointer())) {}

FormulaParser::FormulaParser(storm::prism::Program& program)
    : manager(program.getManager().getSharedPointer()), grammar(new FormulaParserGrammar(program.getManager().getSharedPointer())) {}

FormulaParser::FormulaParser(FormulaParser const& other) : FormulaParser(other.manager) {
    other.grammar->getIdentifiers().for_each(
        [this](std::string const& name, storm::expressions::Expression const& expression) { this->addIdentifierExpression(name, expression); });
}

FormulaParser& FormulaParser::operator=(FormulaParser const& other) {
    this->manager = other.manager;
    this->grammar = std::shared_ptr<FormulaParserGrammar>(new FormulaParserGrammar(this->manager));
    other.grammar->getIdentifiers().for_each(
        [this](std::string const& name, storm::expressions::Expression const& expression) { this->addIdentifierExpression(name, expression); });
    return *this;
}

std::shared_ptr<storm::logic::Formula const> FormulaParser::parseSingleFormulaFromString(std::string const& formulaString) const {
    std::vector<storm::jani::Property> property = parseFromString(formulaString);
    STORM_LOG_THROW(property.size() == 1, storm::exceptions::WrongFormatException,
                    "Expected exactly one formula, but found " << property.size() << " instead.");
    return property.front().getRawFormula();
}

std::vector<storm::jani::Property> FormulaParser::parseFromFile(std::string const& filename) const {
    // Open file and initialize result.
    std::ifstream inputFileStream;
    storm::io::openFile(filename, inputFileStream);

    std::vector<storm::jani::Property> properties;

    // Now try to parse the contents of the file.
    try {
        std::string fileContent((std::istreambuf_iterator<char>(inputFileStream)), (std::istreambuf_iterator<char>()));
        properties = parseFromString(fileContent);
    } catch (std::exception& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    }

    // Close the stream in case everything went smoothly and return result.
    storm::io::closeFile(inputFileStream);
    return properties;
}

std::vector<storm::jani::Property> FormulaParser::parseFromString(std::string const& formulaString) const {
    PositionIteratorType first(formulaString.begin());
    PositionIteratorType iter = first;
    PositionIteratorType last(formulaString.end());

    // Create empty result;
    std::vector<storm::jani::Property> result;

    // Create grammar.
    try {
        // Start parsing.
        bool succeeded = qi::phrase_parse(
            iter, last, *grammar, storm::spirit_encoding::space_type() | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)) >> (qi::eol | qi::eoi), result);
        STORM_LOG_THROW(succeeded, storm::exceptions::WrongFormatException, "Could not parse formula: " << formulaString << ".");
        STORM_LOG_DEBUG("Parsed formula successfully.");
    } catch (qi::expectation_failure<PositionIteratorType> const& e) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, e.what_);
    }

    return result;
}

void FormulaParser::addIdentifierExpression(std::string const& identifier, storm::expressions::Expression const& expression) {
    // Record the mapping and hand it over to the grammar.
    grammar->addIdentifierExpression(identifier, expression);
}

}  // namespace parser
}  // namespace storm
