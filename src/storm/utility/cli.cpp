#include "storm/utility/cli.h"

#include <boost/algorithm/string.hpp>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {
namespace cli {

std::map<storm::expressions::Variable, storm::expressions::Expression> parseConstantDefinitionString(storm::expressions::ExpressionManager const& manager,
                                                                                                     std::string const& constantDefinitionString) {
    std::map<storm::expressions::Variable, storm::expressions::Expression> constantDefinitions;
    std::set<storm::expressions::Variable> definedConstants;

    if (!constantDefinitionString.empty()) {
        // Parse the string that defines the undefined constants of the model and make sure that it contains exactly
        // one value for each undefined constant of the model.
        std::vector<std::string> definitions;
        boost::split(definitions, constantDefinitionString, boost::is_any_of(","));
        for (auto& definition : definitions) {
            boost::trim(definition);

            // Check whether the token could be a legal constant definition.
            std::size_t positionOfAssignmentOperator = definition.find('=');
            STORM_LOG_THROW(positionOfAssignmentOperator != std::string::npos, storm::exceptions::WrongFormatException,
                            "Illegal constant definition string: syntax error.");

            // Now extract the variable name and the value from the string.
            std::string constantName = definition.substr(0, positionOfAssignmentOperator);
            boost::trim(constantName);
            std::string value = definition.substr(positionOfAssignmentOperator + 1);
            boost::trim(value);

            // Check whether the constant is a legal undefined constant of the program and if so, of what type it is.
            if (manager.hasVariable(constantName)) {
                // Get the actual constant and check whether it's in fact undefined.
                auto const& variable = manager.getVariable(constantName);
                STORM_LOG_THROW(definedConstants.find(variable) == definedConstants.end(), storm::exceptions::WrongFormatException,
                                "Illegally trying to define constant '" << constantName << "' twice.");
                definedConstants.insert(variable);

                if (manager.hasVariable(value)) {
                    auto const& valueVariable = manager.getVariable(value);
                    STORM_LOG_THROW(
                        variable.getType() == valueVariable.getType(), storm::exceptions::WrongFormatException,
                        "Illegally trying to define constant '" << constantName << "' by constant '" << valueVariable.getName() << " of different type.");
                    constantDefinitions[variable] = valueVariable.getExpression();
                } else if (variable.hasBooleanType()) {
                    if (value == "true") {
                        constantDefinitions[variable] = manager.boolean(true);
                    } else if (value == "false") {
                        constantDefinitions[variable] = manager.boolean(false);
                    } else {
                        throw storm::exceptions::WrongFormatException() << "Illegal value for boolean constant: " << value << ".";
                    }
                } else if (variable.hasIntegerType()) {
                    int_fast64_t integerValue = std::stoll(value);
                    constantDefinitions[variable] = manager.integer(integerValue);
                } else if (variable.hasRationalType()) {
                    try {
                        storm::RationalNumber rationalValue = storm::utility::convertNumber<storm::RationalNumber>(value);
                        constantDefinitions[variable] = manager.rational(rationalValue);
                    } catch (std::exception& e) {
                        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                        "Illegal constant definition string '" << constantName << "=" << value << "': " << e.what());
                    }
                }
            } else {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                "Illegal constant definition string: unknown undefined constant '" << constantName << "'.");
            }
        }
    }

    return constantDefinitions;
}

std::vector<std::string> parseCommaSeparatedStrings(std::string const& input) {
    std::vector<std::string> result;
    if (!input.empty()) {
        boost::split(result, input, boost::is_any_of(","));
        for (auto& entry : result) {
            boost::trim(entry);
        }
    }
    return result;
}

}  // namespace cli
}  // namespace utility
}  // namespace storm
