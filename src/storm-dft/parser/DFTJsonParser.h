#pragma once

#include "storm/adapters/JsonForward.h"

// Forward declarations
namespace storm::parser {
template<typename ValueType>
class ValueParser;
;
}  // namespace storm::parser
namespace storm::dft::builder {
template<typename ValueType>
class DFTBuilder;
}
namespace storm::dft::storage {
template<typename ValueType>
class DFT;
}

namespace storm::dft {
namespace parser {

/*!
 * Parser for DFT in custom JSON format.
 */
template<typename ValueType>
class DFTJsonParser {
    typedef typename storm::json<double> Json;

   public:
    /*!
     * Parse DFT from JSON format given as a string and build DFT.
     * @param jsonString String containing JSON of DFT.
     * @return DFT.
     */
    static storm::dft::storage::DFT<ValueType> parseJsonFromString(std::string const& jsonString);

    /*!
     * Parse DFT from JSON format given as file and build DFT.
     * @param filename File.
     * @return DFT.
     */
    static storm::dft::storage::DFT<ValueType> parseJsonFromFile(std::string const& filename);

   private:
    /*!
     * Parse DFT from JSON and build DFT.
     * @param jsonInput JSON input.
     * @return DFT.
     */
    static storm::dft::storage::DFT<ValueType> parseJson(Json const& jsonInput);

    /*!
     * Parse basic element and add it to builder.
     *
     * @param name Name of BE.
     * @param type Type of BE (only used for sanity check).
     * @param input Input JSON.
     * @param builder DFTBuilder.
     * @param valueParser ValueParser for parsing numbers.
     */
    static void parseBasicElement(std::string const& name, std::string const& type, Json input, storm::dft::builder::DFTBuilder<ValueType>& builder,
                                  storm::parser::ValueParser<ValueType>& valueParser);

    /*!
     * Parse element name (replace white space by '_', etc.).
     *
     * @param name Element name.
     * @return Name.
     */
    static std::string parseName(std::string const& name);

    /*!
     * Translate JSON value to string.
     * @param value Input JSON.
     * @return String representation of value.
     */
    static std::string parseValue(Json value);
};

}  // namespace parser
}  // namespace storm::dft
