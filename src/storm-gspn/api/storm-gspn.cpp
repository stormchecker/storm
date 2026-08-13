#include "storm-gspn/api/storm-gspn.h"

#include <boost/algorithm/string.hpp>
#include "storm-conv/api/storm-conv.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-gspn/settings/modules/GSPNExportSettings.h"
#include "storm-parsers/parser/ExpressionParser.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/settings/SettingsManager.h"
#include "storm/utility/solver.h"

namespace storm {
namespace api {

storm::jani::Model* buildJani(storm::gspn::GSPN const& gspn) {
    storm::builder::JaniGSPNBuilder builder(gspn);
    return builder.build();
}

void handleGSPNExportSettings(storm::gspn::GSPN const& gspn,
                              std::function<std::vector<storm::jani::Property>(storm::builder::JaniGSPNBuilder const&)> const& janiProperyGetter) {
    storm::settings::modules::GSPNExportSettings const& exportSettings = storm::settings::getModule<storm::settings::modules::GSPNExportSettings>();
    if (exportSettings.isWriteToDotSet()) {
        std::ofstream fs;
        storm::io::openFile(exportSettings.getWriteToDotFilename(), fs);
        gspn.writeDotToStream(fs);
        storm::io::closeFile(fs);
    }

    if (exportSettings.isWriteToPnproSet()) {
        std::ofstream fs;
        storm::io::openFile(exportSettings.getWriteToPnproFilename(), fs);
        gspn.toPnpro(fs);
        storm::io::closeFile(fs);
    }

    if (exportSettings.isWriteToPnmlSet()) {
        std::ofstream fs;
        storm::io::openFile(exportSettings.getWriteToPnmlFilename(), fs);
        gspn.toPnml(fs);
        storm::io::closeFile(fs);
    }

    if (exportSettings.isWriteToJsonSet()) {
        std::ofstream fs;
        storm::io::openFile(exportSettings.getWriteToJsonFilename(), fs);
        gspn.toJson(fs);
        storm::io::closeFile(fs);
    }

    if (exportSettings.isDisplayStatsSet()) {
        std::cout << "============GSPN Statistics==============\n";
        gspn.writeStatsToStream(std::cout);
        std::cout << "=========================================\n";
    }

    if (exportSettings.isWriteStatsToFileSet()) {
        std::ofstream fs;
        storm::io::openFile(exportSettings.getWriteStatsFilename(), fs);
        gspn.writeStatsToStream(fs);
        storm::io::closeFile(fs);
    }

    if (exportSettings.isWriteToJaniSet()) {
        auto const& jani = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();
        storm::converter::JaniConversionOptions options(jani);

        storm::builder::JaniGSPNBuilder builder(gspn);
        storm::jani::Model* model = builder.build("gspn_automaton");

        auto properties = janiProperyGetter(builder);
        if (exportSettings.isAddJaniPropertiesSet()) {
            auto deadlockProperties = builder.getDeadlockProperties(model);
            properties.insert(properties.end(), deadlockProperties.begin(), deadlockProperties.end());
        }

        storm::api::transformJani(*model, properties, options, std::make_shared<storm::utility::solver::SmtSolverFactory>());

        storm::api::exportJaniToFile(*model, properties, exportSettings.getWriteToJaniFilename(), jani.isCompactJsonSet());
        delete model;
    }
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
