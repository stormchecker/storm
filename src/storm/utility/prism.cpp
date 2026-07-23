#include "storm/utility/prism.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/cli.h"

namespace storm {
namespace utility {
namespace prism {

storm::prism::Program preprocess(storm::prism::Program const& program,
                                 std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) {
    storm::prism::Program result = program.defineUndefinedConstants(constantDefinitions);
    result = result.substituteConstantsFormulas();
    return result;
}

storm::prism::Program preprocess(storm::prism::Program const& program, std::string const& constantDefinitionString) {
    return preprocess(program, storm::utility::cli::parseConstantDefinitionString(program.getManager(), constantDefinitionString));
}

}  // namespace prism
}  // namespace utility
}  // namespace storm
