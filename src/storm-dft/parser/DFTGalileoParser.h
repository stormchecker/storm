#pragma once

#include "storm-dft/builder/DFTBuilder.h"
#include "storm-dft/storage/DFT.h"
#include "storm-parsers/parser/ValueParser.h"

namespace storm::dft {
namespace parser {

/*!
 * Parser for DFT in the Galileo format.
 *
 * The original definition of the Galileo format is given at
 * https://www.cse.msu.edu/~cse870/Materials/FaultTolerant/manual-galileo.htm
 * Extensions of the format are provided at
 * https://dftbenchmarks.utwente.nl/galileo.html
 */
template<typename ValueType>
class DFTGalileoParser {
   public:
    /*!
     * Parse DFT in Galileo format and build DFT.
     *
     * @param filename File.
     *
     * @return DFT.
     */
    static storm::dft::storage::DFT<ValueType> parseDFT(std::string const& filename);

    /*!
     * Parse element name (strip quotation marks, etc.).
     *
     * @param name Element name.
     *
     * @return Name.
     */
    static std::string parseName(std::string const& name);

   private:
    /*!
     * Parse basic element and add it to builder.
     *
     * @param name Name of BE.
     * @param input Input line (without name of BE). Will be modified during the parsing.
     * @param builder DFTBuilder.
     * @param valueParser ValueParser for parsing numbers.
     */
    static void parseBasicElement(std::string const& name, std::string& input, storm::dft::builder::DFTBuilder<ValueType>& builder,
                                  storm::parser::ValueParser<ValueType>& valueParser);

    /*!
     * Parse argument of basic element of the form "name=value".
     *
     * @param name Name of BE.
     * @param input Input line. The parsed argument will be removed from the line.
     *
     * @return String containing 'value'. Empty string if the parsing was unsuccessful.
     */
    static std::string parseValue(std::string name, std::string& line);
};

}  // namespace parser
}  // namespace storm::dft
