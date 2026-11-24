#include "storm-gspn.h"

#include <boost/algorithm/string.hpp>

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/parser/ExpressionParser.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/solver.h"

namespace storm {
namespace api {

storm::jani::Model* buildJani(storm::gspn::GSPN const& gspn) {
    storm::builder::JaniGSPNBuilder builder(gspn);
    return builder.build();
}

void exportGspnToDot(storm::gspn::GSPN const& gspn, std::string const& filename) {
    std::ofstream fs;
    storm::io::openFile(filename, fs);
    gspn.writeDotToStream(fs);
    storm::io::closeFile(fs);
}

void exportGspnToPnpro(storm::gspn::GSPN const& gspn, std::string const& filename) {
    std::ofstream fs;
    storm::io::openFile(filename, fs);
    gspn.toPnpro(fs);
    storm::io::closeFile(fs);
}

void exportGspnToPnml(storm::gspn::GSPN const& gspn, std::string const& filename) {
    std::ofstream fs;
    storm::io::openFile(filename, fs);
    gspn.toPnml(fs);
    storm::io::closeFile(fs);
}

void exportGspnToJson(storm::gspn::GSPN const& gspn, std::string const& filename) {
    std::ofstream fs;
    storm::io::openFile(filename, fs);
    gspn.toJson(fs);
    storm::io::closeFile(fs);
}

void printGspnStatsToStream(storm::gspn::GSPN const& gspn, std::ostream& out) {
    gspn.writeStatsToStream(out);
}

void exportGspnStatsToFile(storm::gspn::GSPN const& gspn, std::string const& filename) {
    std::ofstream fs;
    storm::io::openFile(filename, fs);
    gspn.writeStatsToStream(fs);
    storm::io::closeFile(fs);
}

void exportGspnToJani(storm::gspn::GSPN const& gspn, std::string const& filename, GspnJaniExportOptions const& options,
                      std::function<std::vector<storm::jani::Property>(storm::builder::JaniGSPNBuilder const&)> const& janiPropertyGetter) {
    storm::builder::JaniGSPNBuilder builder(gspn);
    storm::jani::Model* model = builder.build("gspn_automaton");

    auto properties = janiPropertyGetter(builder);
    if (options.addDeadlockProperties) {
        auto deadlockProperties = builder.getDeadlockProperties(model);
        properties.insert(properties.end(), deadlockProperties.begin(), deadlockProperties.end());
    }

    storm::api::transformJani(*model, properties, options.janiConversionOptions, options.smtSolverFactory);

    storm::api::exportJaniToFile(*model, properties, filename, options.compactJson);
    delete model;
}

std::unordered_map<std::string, uint64_t> parseCapacitiesList(std::string const& filename, storm::gspn::GSPN const& gspn) {
    storm::parser::ExpressionParser expressionParser(*gspn.getExpressionManager());
    std::unordered_map<std::string, storm::expressions::Expression> identifierMapping;
    for (auto const& var : gspn.getExpressionManager()->getVariables()) {
        identifierMapping.emplace(var.getName(), var.getExpression());
    }
    expressionParser.setIdentifierMapping(identifierMapping);
    expressionParser.setAcceptDoubleLiterals(false);

    std::unordered_map<std::string, uint64_t> map;

    std::ifstream stream;
    storm::io::openFile(filename, stream);

    std::string line;
    while (storm::io::getline(stream, line)) {
        std::vector<std::string> strs;
        boost::split(strs, line, boost::is_any_of("\t "));
        STORM_LOG_THROW(strs.size() == 2, storm::exceptions::WrongFormatException, "Expect key value pairs.");
        storm::expressions::Expression expr = expressionParser.parseFromString(strs[1]);
        if (!gspn.getConstantsSubstitution().empty()) {
            expr = expr.substitute(gspn.getConstantsSubstitution());
        }
        STORM_LOG_THROW(!expr.containsVariables(), storm::exceptions::WrongFormatException,
                        "The capacity expression '" << strs[1] << "' still contains undefined constants.");
        map[strs[0]] = expr.evaluateAsInt();
    }
    storm::io::closeFile(stream);
    return map;
}
}  // namespace api
}  // namespace storm
