#pragma once

#include <string>

#include "storm/storage/prism/Program.h"

namespace storm::parser {

class PrismParser {
   public:
    /*!
     * Parses the given file into the PRISM storage classes assuming it complies with the PRISM syntax.
     *
     * @param filename the name of the file to parse.
     * @return The resulting PRISM program.
     */
    static storm::prism::Program parse(std::string const& filename, bool prismCompatability = false);

    /*!
     * Parses the given input stream into the PRISM storage classes assuming it complies with the PRISM syntax.
     *
     * @param input The input string to parse.
     * @param filename The name of the file from which the input was read.
     * @return The resulting PRISM program.
     */
    static storm::prism::Program parseFromString(std::string const& input, std::string const& filename, bool prismCompatability = false);
};
}  // namespace storm::parser
