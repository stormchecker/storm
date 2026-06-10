#include "storm/api/properties.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cctype>
#include <limits>
#include <stdexcept>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/prism/Program.h"

#include "storm/logic/Formulas.h"

#include "storm/utility/cli.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace api {
namespace {

std::string trimAndStripLeadingPlus(std::string const& input) {
    std::string result = boost::algorithm::trim_copy(input);
    if (!result.empty() && result.front() == '+') {
        result.erase(result.begin());
    }
    return result;
}

bool isNonEmptyUnsignedDecimalInteger(std::string const& input) {
    return !input.empty() && std::all_of(input.begin(), input.end(), [](unsigned char c) { return std::isdigit(c); });
}

storm::RationalNumber parseUnsignedIntegerAsRational(std::string const& input, std::string const& originalInput) {
    std::string strippedInput = trimAndStripLeadingPlus(input);
    STORM_LOG_THROW(isNonEmptyUnsignedDecimalInteger(strippedInput), storm::exceptions::InvalidArgumentException,
                    "Unable to parse CVaR alpha '" << originalInput << "'.");
    return storm::utility::convertNumber<storm::RationalNumber>(strippedInput);
}

storm::RationalNumber powerOfTen(uint64_t exponent) {
    storm::RationalNumber result = storm::utility::one<storm::RationalNumber>();
    storm::RationalNumber const ten = storm::utility::convertNumber<storm::RationalNumber>(10);
    for (uint64_t i = 0; i < exponent; ++i) {
        result *= ten;
    }
    return result;
}

int64_t parseSignedExponent(std::string const& input, std::string const& originalInput) {
    STORM_LOG_THROW(!input.empty(), storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");
    std::string exponentString = input;
    bool negative = false;
    if (exponentString.front() == '+' || exponentString.front() == '-') {
        negative = exponentString.front() == '-';
        exponentString.erase(exponentString.begin());
    }
    STORM_LOG_THROW(isNonEmptyUnsignedDecimalInteger(exponentString), storm::exceptions::InvalidArgumentException,
                    "Unable to parse CVaR alpha '" << originalInput << "'.");

    uint64_t exponent = 0;
    try {
        exponent = std::stoull(exponentString);
    } catch (std::exception const&) {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");
    }
    STORM_LOG_THROW(exponent <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), storm::exceptions::InvalidArgumentException,
                    "Unable to parse CVaR alpha '" << originalInput << "'.");
    return negative ? -static_cast<int64_t>(exponent) : static_cast<int64_t>(exponent);
}

storm::RationalNumber parseDecimalOrScientificCvarAlpha(std::string const& input, std::string const& originalInput) {
    std::string mantissa = input;
    int64_t exponent = 0;

    auto exponentPosition = mantissa.find_first_of("eE");
    if (exponentPosition != std::string::npos) {
        STORM_LOG_THROW(mantissa.find_first_of("eE", exponentPosition + 1) == std::string::npos, storm::exceptions::InvalidArgumentException,
                        "Unable to parse CVaR alpha '" << originalInput << "'.");
        exponent = parseSignedExponent(mantissa.substr(exponentPosition + 1), originalInput);
        mantissa = mantissa.substr(0, exponentPosition);
    }

    STORM_LOG_THROW(!mantissa.empty(), storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");
    STORM_LOG_THROW(mantissa.front() != '-', storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");
    if (mantissa.front() == '+') {
        mantissa.erase(mantissa.begin());
    }

    auto decimalPosition = mantissa.find('.');
    STORM_LOG_THROW(decimalPosition == std::string::npos || mantissa.find('.', decimalPosition + 1) == std::string::npos,
                    storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");

    std::string digitsBeforeDecimal;
    std::string digitsAfterDecimal;
    if (decimalPosition == std::string::npos) {
        digitsBeforeDecimal = mantissa;
    } else {
        digitsBeforeDecimal = mantissa.substr(0, decimalPosition);
        digitsAfterDecimal = mantissa.substr(decimalPosition + 1);
    }

    STORM_LOG_THROW((digitsBeforeDecimal.empty() || isNonEmptyUnsignedDecimalInteger(digitsBeforeDecimal)) &&
                        (digitsAfterDecimal.empty() || isNonEmptyUnsignedDecimalInteger(digitsAfterDecimal)) &&
                        !(digitsBeforeDecimal.empty() && digitsAfterDecimal.empty()),
                    storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << originalInput << "'.");
    STORM_LOG_THROW(digitsAfterDecimal.size() <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), storm::exceptions::InvalidArgumentException,
                    "Unable to parse CVaR alpha '" << originalInput << "'.");

    std::string digits = digitsBeforeDecimal + digitsAfterDecimal;
    storm::RationalNumber value = storm::utility::convertNumber<storm::RationalNumber>(digits);
    int64_t decimalScale = static_cast<int64_t>(digitsAfterDecimal.size()) - exponent;
    if (decimalScale >= 0) {
        value /= powerOfTen(static_cast<uint64_t>(decimalScale));
    } else {
        value *= powerOfTen(static_cast<uint64_t>(-decimalScale));
    }
    return value;
}

storm::RationalNumber parseCvarAlpha(std::string const& input) {
    std::string strippedInput = trimAndStripLeadingPlus(input);
    STORM_LOG_THROW(!strippedInput.empty(), storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << input << "'.");
    STORM_LOG_THROW(strippedInput.front() != '-', storm::exceptions::InvalidArgumentException, "Unable to parse CVaR alpha '" << input << "'.");

    storm::RationalNumber alpha;
    auto fractionSeparator = strippedInput.find('/');
    if (fractionSeparator != std::string::npos) {
        STORM_LOG_THROW(strippedInput.find('/', fractionSeparator + 1) == std::string::npos, storm::exceptions::InvalidArgumentException,
                        "Unable to parse CVaR alpha '" << input << "'.");
        auto numerator = parseUnsignedIntegerAsRational(strippedInput.substr(0, fractionSeparator), input);
        auto denominator = parseUnsignedIntegerAsRational(strippedInput.substr(fractionSeparator + 1), input);
        STORM_LOG_THROW(denominator != storm::utility::zero<storm::RationalNumber>(), storm::exceptions::InvalidArgumentException,
                        "Unable to parse CVaR alpha '" << input << "' because the denominator is zero.");
        alpha = numerator / denominator;
    } else {
        alpha = parseDecimalOrScientificCvarAlpha(strippedInput, input);
    }

    STORM_LOG_THROW(storm::utility::zero<storm::RationalNumber>() < alpha && alpha < storm::utility::one<storm::RationalNumber>(),
                    storm::exceptions::InvalidArgumentException, "The CVaR alpha must be in the open interval (0, 1).");
    return alpha;
}

}  // namespace

std::vector<storm::jani::Property> substituteConstantsInProperties(std::vector<storm::jani::Property> const& properties,
                                                                   std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) {
    std::vector<storm::jani::Property> preprocessedProperties;
    for (auto const& property : properties) {
        preprocessedProperties.emplace_back(property.substitute(substitution));
    }
    return preprocessedProperties;
}

std::vector<storm::jani::Property> substituteTranscendentalNumbersInProperties(std::vector<storm::jani::Property> const& properties) {
    std::vector<storm::jani::Property> preprocessedProperties;
    for (auto const& property : properties) {
        preprocessedProperties.emplace_back(property.substituteTranscendentalNumbers());
    }
    return preprocessedProperties;
}

std::vector<storm::jani::Property> filterProperties(std::vector<storm::jani::Property> const& properties,
                                                    boost::optional<std::set<std::string>> const& propertyFilter) {
    if (propertyFilter) {
        std::set<std::string> const& propertyNameSet = propertyFilter.get();
        std::vector<storm::jani::Property> result;
        std::set<std::string> reducedPropertyNames;

        if (propertyNameSet.empty()) {
            STORM_LOG_WARN("Filtering all properties.");
        }

        for (auto const& property : properties) {
            if (propertyNameSet.find(property.getName()) != propertyNameSet.end()) {
                result.push_back(property);
                reducedPropertyNames.insert(property.getName());
            }
        }

        if (reducedPropertyNames.size() < propertyNameSet.size()) {
            std::set<std::string> missingProperties;
            std::set_difference(propertyNameSet.begin(), propertyNameSet.end(), reducedPropertyNames.begin(), reducedPropertyNames.end(),
                                std::inserter(missingProperties, missingProperties.begin()));
            STORM_LOG_WARN("Filtering unknown properties " << boost::join(missingProperties, ", ") << ".");
        }

        return result;
    } else {
        return properties;
    }
}

std::vector<std::shared_ptr<storm::logic::Formula const>> extractFormulasFromProperties(std::vector<storm::jani::Property> const& properties) {
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
    for (auto const& prop : properties) {
        formulas.push_back(prop.getRawFormula());
    }
    return formulas;
}

storm::jani::Property createCvarProperty(storm::jani::Property const& property, storm::RationalNumber const& alpha) {
    STORM_LOG_WARN_COND(property.getFilter().isDefault(),
                        "Non-default property filter of property " << property.getName() << " will be dropped during conversion to CVaR property.");
    auto cvarFormula = std::make_shared<storm::logic::CvarFormula>(alpha, property.getRawFormula());
    return storm::jani::Property(property.getName(), cvarFormula, property.getUndefinedConstants(), property.getComment());
}

storm::jani::Property createCvarProperty(storm::jani::Property const& property, std::string const& alpha) {
    return createCvarProperty(property, parseCvarAlpha(alpha));
}

storm::jani::Property createMultiObjectiveProperty(std::vector<storm::jani::Property> const& properties, bool lexicographic) {
    std::set<storm::expressions::Variable> undefConstants;
    std::string name = "";
    std::string comment = "";
    for (auto const& prop : properties) {
        undefConstants.insert(prop.getUndefinedConstants().begin(), prop.getUndefinedConstants().end());
        name += prop.getName();
        comment += prop.getComment();
        STORM_LOG_WARN_COND(prop.getFilter().isDefault(),
                            "Non-default property filter of property " + prop.getName() + " will be dropped during conversion to multi-objective property.");
    }
    auto multiFormula = std::make_shared<storm::logic::MultiObjectiveFormula>(
        extractFormulasFromProperties(properties),
        lexicographic ? storm::logic::MultiObjectiveFormula::Type::Lexicographic : storm::logic::MultiObjectiveFormula::Type::Tradeoff);
    return storm::jani::Property(name, multiFormula, undefConstants, comment);
}
}  // namespace api
}  // namespace storm
