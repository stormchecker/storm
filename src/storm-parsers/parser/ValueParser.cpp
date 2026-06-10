#include "storm-parsers/parser/ValueParser.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "storm-parsers/parser/ExpressionParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/utility/constants.h"

namespace storm {
namespace parser {

template<typename ValueType>
ValueParser<ValueType>::ParametricData::ParametricData()
    : manager(new storm::expressions::ExpressionManager()),
      parser(std::make_unique<ExpressionParser>(*manager)),
      evaluator(new storm::expressions::ExpressionEvaluator<storm::RationalFunction>(*manager)) {
    // Set empty mapping to enable expression creation even without parameters
    parser->setIdentifierMapping(identifierMapping);
}

template<typename ValueType>
ValueParser<ValueType>::ParametricData::~ParametricData() = default;

template<typename ValueType>
void ValueParser<ValueType>::addParameter(std::string const& parameter) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameters are not supported in this build (Have you checked storm-pars?).");
}

template<>
void ValueParser<storm::RationalFunction>::addParameter(std::string const& parameter) {
    storm::expressions::Variable var = data.manager->declareRationalVariable(parameter);
    data.identifierMapping.emplace(var.getName(), var);
    data.parser->setIdentifierMapping(data.identifierMapping);
    STORM_LOG_TRACE("Added parameter: " << var.getName());
}

template<>
storm::RationalFunction ValueParser<storm::RationalFunction>::parseValue(std::string const& value) const {
    storm::RationalFunction rationalFunction = data.evaluator->asRational(data.parser->parseFromString(value));
    STORM_LOG_TRACE("Parsed expression: " << rationalFunction);
    return rationalFunction;
}

template<typename ValueType>
ValueType ValueParser<ValueType>::parseValue(std::string const& value) const {
    return parseNumber<ValueType>(value);
}

template<typename NumberType>
NumberType parseNumber(std::string const& value) {
    NumberType result;
    if (!parseNumber(value, result)) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Could not parse value '" << value << "' into " << typeid(NumberType).name() << ".");
    }
    return result;
}

bool parseDouble(std::string const& value, double& result) {
    if (boost::conversion::try_lexical_convert(value, result)) {
        return true;
    } else {
        // Try as rational number
        storm::RationalNumber rationalResult;
        if (parseNumber(value, rationalResult)) {
            result = storm::utility::convertNumber<double>(rationalResult);
            return true;
        } else {
            return false;
        }
    }
}

template<typename IntervalType>
bool parseInterval(std::string const& value, IntervalType& result) {
    // Try whether it is a constant.
    if (IntervalBaseType<IntervalType> pointResult; parseNumber(value, pointResult)) {
        result = IntervalType(pointResult);
        return true;
    }

    std::string intermediate = value;
    boost::trim(intermediate);
    carl::BoundType leftBound;
    carl::BoundType rightBound;
    if (intermediate.front() == '(') {
        leftBound = carl::BoundType::STRICT;
    } else if (intermediate.front() == '[') {
        leftBound = carl::BoundType::WEAK;
    } else {
        return false;  // Expect start with '(' or '['.
    }
    if (intermediate.back() == ')') {
        rightBound = carl::BoundType::STRICT;
    } else if (intermediate.back() == ']') {
        rightBound = carl::BoundType::WEAK;
    } else {
        return false;  // Expected end with ')' or ']'.
    }
    intermediate = intermediate.substr(1, intermediate.size() - 2);

    std::vector<std::string> words;
    boost::split(words, intermediate, boost::is_any_of(","));
    if (words.size() != 2) {
        return false;  // Did not find exactly one comma.
    }
    IntervalBaseType<IntervalType> leftVal, rightVal;
    boost::trim(words[0]);
    if (!parseNumber(words[0], leftVal)) {
        return false;  // lower value of interval invalid.
    }
    boost::trim(words[1]);
    if (!parseNumber(words[1], rightVal)) {
        return false;  // upper value of interval invalid.
    }
    result = IntervalType(leftVal, leftBound, rightVal, rightBound);
    return true;
}

template<typename NumberType>
bool parseNumber(std::string const& value, NumberType& result) {
    if constexpr (std::is_same_v<NumberType, double>) {
        return parseDouble(value, result);
    } else if constexpr (std::is_same_v<NumberType, storm::RationalNumber>) {
        return carl::try_parse(value, result);
    } else if constexpr (std::is_same_v<NumberType, storm::Interval>) {
        return parseInterval<storm::Interval>(value, result);
    } else if constexpr (std::is_same_v<NumberType, storm::RationalInterval>) {
        return parseInterval<storm::RationalInterval>(value, result);
    } else {
        return boost::conversion::try_lexical_convert(value, result);
    }
}

// Template instantiations.
template class ValueParser<double>;
template class ValueParser<storm::RationalNumber>;
template class ValueParser<storm::RationalFunction>;
template class ValueParser<storm::Interval>;
template class ValueParser<storm::RationalInterval>;

template std::size_t parseNumber<std::size_t>(std::string const&);

}  // namespace parser
}  // namespace storm
