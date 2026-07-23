#ifndef STORM_UTILITY_PRISM_H_
#define STORM_UTILITY_PRISM_H_

#include <map>
#include <string>

namespace storm {
namespace expressions {
class Variable;
class Expression;
}  // namespace expressions

namespace prism {
class Program;
}

namespace utility {
namespace prism {

storm::prism::Program preprocess(storm::prism::Program const& program,
                                 std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions);

storm::prism::Program preprocess(storm::prism::Program const& program, std::string const& constantDefinitionString);

}  // namespace prism
}  // namespace utility
}  // namespace storm

#endif /* STORM_UTILITY_PRISM_H_ */
