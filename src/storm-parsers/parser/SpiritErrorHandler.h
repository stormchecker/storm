#pragma once

#include "storm-parsers/parser/SpiritParserDefinitions.h"

#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {
// Functor used for displaying error information.
struct SpiritErrorHandler {
    typedef qi::error_handler_result result_type;

    template<typename T1, typename T2, typename T3, typename T4>
    qi::error_handler_result operator()(T1 b, T2 e, T3 where, T4 const& what) const {
        auto lineStart = boost::spirit::get_line_start(b, where);
        auto lineEnd = std::find(where, e, '\n');
        std::string line(lineStart, lineEnd);

        std::stringstream stream;
        stream << "Parsing error at " << get_line(where) << ":" << boost::spirit::get_column(lineStart, where) << ": "
               << " expecting " << what << ", here:\n";
        stream << "\t" << line << '\n';
        auto caretColumn = boost::spirit::get_column(lineStart, where);
        stream << "\t" << std::string(caretColumn - 1, ' ') << "^\n";

        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, stream.str());
        return qi::fail;
    }
};
}  // namespace parser
}  // namespace storm
