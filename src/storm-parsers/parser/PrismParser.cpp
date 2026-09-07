#include "PrismParser.h"

#include "storm-parsers/parser/PrismParserGrammar.h"

namespace storm::parser {
storm::prism::Program PrismParser::parse(std::string const& filename, bool prismCompatibility) {
    return PrismParserGrammar::parse(filename, prismCompatibility);
}

storm::prism::Program PrismParser::parseFromString(std::string const& input, std::string const& filename, bool prismCompatibility) {
    return PrismParserGrammar::parseFromString(input, filename, prismCompatibility);
}
}  // namespace storm::parser
