#include "storm-parsers/parser/ImcaMarkovAutomatonParser.h"

#include <fstream>
#include <memory>

#include "storm-parsers/parser/ImcaMarkovAutomatonParserGrammar.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/io/file.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"

namespace storm {
namespace parser {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> ImcaMarkovAutomatonParser<ValueType>::parseImcaFile(
    std::string const& filename, ExplicitModelParserOptions const& options) {
    // Open file and initialize result.
    std::ifstream inputFileStream;
    storm::io::openFile(filename, inputFileStream);

    storm::storage::sparse::ModelComponents<ValueType> components;

    // Now try to parse the contents of the file.
    std::string fileContent((std::istreambuf_iterator<char>(inputFileStream)), (std::istreambuf_iterator<char>()));
    PositionIteratorType first(fileContent.begin());
    PositionIteratorType iter = first;
    PositionIteratorType last(fileContent.end());

    try {
        // Start parsing.
        ImcaParserGrammar<ValueType> grammar(options);
        bool succeeded = qi::phrase_parse(
            iter, last, grammar, storm::spirit_encoding::space_type() | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)) >> (qi::eol | qi::eoi), components);
        STORM_LOG_THROW(succeeded, storm::exceptions::WrongFormatException, "Could not parse imca file.");
        STORM_LOG_DEBUG("Parsed imca file successfully.");
    } catch (qi::expectation_failure<PositionIteratorType> const& e) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, e.what_);
        storm::io::closeFile(inputFileStream);
    } catch (std::exception& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    }

    // Close the stream in case everything went smoothly
    storm::io::closeFile(inputFileStream);

    // Build the model from the obtained model components
    return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::MarkovAutomaton, std::move(components))
        ->template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
}

template class ImcaMarkovAutomatonParser<double>;
}  // namespace parser
}  // namespace storm
