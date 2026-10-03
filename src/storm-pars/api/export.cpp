#include "storm-pars/api/export.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

#include "storm/analysis/GraphConditions.h"
#include "storm/io/file.h"

namespace storm::pars {
namespace api {

namespace {

void writeConstraints(std::ostream& filestream, storm::analysis::ConstraintCollector::ConstraintSet const& constraints) {
    std::vector<std::string> stringConstraints;
    stringConstraints.reserve(constraints.size());
    std::transform(constraints.begin(), constraints.end(), std::back_inserter(stringConstraints),
                   [](storm::expressions::Expression const& c) -> std::string { return c.toString(); });
    std::copy(stringConstraints.begin(), stringConstraints.end(), std::ostream_iterator<std::string>(filestream, "\n"));
}

}  // namespace

template<>
void exportParametricResultToFile(std::optional<storm::RationalFunction> result,
                                  storm::OptionalRef<storm::analysis::ConstraintCollector const> const& constraintCollector, std::string const& path) {
    std::ofstream filestream;
    storm::io::openFile(path, filestream);
    if (constraintCollector.has_value()) {
        filestream << "$Parameters: ";
        auto const& vars = constraintCollector->getVariables();
        std::copy(vars.begin(), vars.end(), std::ostream_iterator<storm::RationalFunctionVariable>(filestream, "; "));
        filestream << '\n';
    } else {
        if (result) {
            filestream << "$Parameters: ";
            auto const& vars = result->gatherVariables();
            std::copy(vars.begin(), vars.end(), std::ostream_iterator<storm::RationalFunctionVariable>(filestream, "; "));
            filestream << '\n';
        }
    }
    if (result) {
        filestream << "$Result: " << result->toString(false, true) << '\n';
    }
    if (constraintCollector.has_value()) {
        filestream << "$Well-formed Constraints: \n";
        writeConstraints(filestream, constraintCollector->getWellformedConstraints());
        filestream << "$Graph-preserving Constraints: \n";
        writeConstraints(filestream, constraintCollector->getGraphPreservingConstraints());
    }
    storm::io::closeFile(filestream);
}

}  // namespace api
}  // namespace storm::pars
