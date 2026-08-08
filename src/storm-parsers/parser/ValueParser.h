#pragma once

#include <cstddef>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace expressions {
template<typename V>
class ExpressionEvaluator;
}

namespace parser {
class ExpressionParser;

/*!
 * Parser for values according to their ValueType.
 */
template<typename ValueType>
class ValueParser {
   public:
    /*!
     * Parse ValueType from string.
     *
     * @param value String containing the value.
     *
     * @return ValueType
     */
    ValueType parseValue(std::string const& value) const;

    /*!
     * Add declaration of parameter.
     *
     * @param parameter New parameter.
     */
    void addParameter(std::string const& parameter);

   private:
    struct ParametricData {
        ParametricData();
        ~ParametricData();
        std::shared_ptr<storm::expressions::ExpressionManager> manager;
        std::unique_ptr<storm::parser::ExpressionParser> parser;  // Pointer to avoid header include.
        std::unique_ptr<storm::expressions::ExpressionEvaluator<storm::RationalFunction>> evaluator;
        std::unordered_map<std::string, storm::expressions::Expression> identifierMapping;
        std::unordered_map<std::string, storm::RationalFunction> rationalFunctionCache;
    };
    std::conditional_t<std::is_same_v<ValueType, storm::RationalFunction>, ParametricData, std::nullptr_t> data;
};

/*!
 * Parse number from string.
 *
 * @param value String containing the value.
 *
 * @throws WrongFormatException if parsing is not successful
 * @return NumberType.
 */
template<typename NumberType>
NumberType parseNumber(std::string const& value);

/*!
 * Parse number from string.
 * @param value String containing the value.
 * @param result if parsing is successful, the parsed number is stored here
 * @return whether parsing is successful.
 */
template<typename NumberType>
bool parseNumber(std::string const& value, NumberType& result);

}  // namespace parser
}  // namespace storm
