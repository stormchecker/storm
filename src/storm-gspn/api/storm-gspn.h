#pragma once

#include <memory>
#include <unordered_map>

#include "storm-conv/converter/options/JaniConversionOptions.h"
#include "storm-gspn/builder/JaniGSPNBuilder.h"
#include "storm-gspn/storage/gspn/GSPN.h"
#include "storm/storage/jani/Model.h"

namespace storm {
namespace utility::solver {
class SmtSolverFactory;
}
namespace api {

/*!
 *    Builds JANI model from GSPN.
 */
storm::jani::Model* buildJani(storm::gspn::GSPN const& gspn);

void exportGspnToDot(storm::gspn::GSPN const& gspn, std::string const& filename);
void exportGspnToPnpro(storm::gspn::GSPN const& gspn, std::string const& filename);
void exportGspnToPnml(storm::gspn::GSPN const& gspn, std::string const& filename);
void exportGspnToJson(storm::gspn::GSPN const& gspn, std::string const& filename);
void printGspnStatsToStream(storm::gspn::GSPN const& gspn, std::ostream& out);
void exportGspnStatsToFile(storm::gspn::GSPN const& gspn, std::string const& filename);

/// Options for exporting a GSPN as a JANI model.
struct GspnJaniExportOptions {
    GspnJaniExportOptions() = default;

    bool addDeadlockProperties = false;
    storm::converter::JaniConversionOptions janiConversionOptions;
    bool compactJson = false;
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = nullptr;
};

void exportGspnToJani(
    storm::gspn::GSPN const& gspn, std::string const& filename, GspnJaniExportOptions const& options = {},
    std::function<std::vector<storm::jani::Property>(storm::builder::JaniGSPNBuilder const&)> const& janiPropertyGetter =
        [](storm::builder::JaniGSPNBuilder const&) { return std::vector<storm::jani::Property>(); });

std::unordered_map<std::string, uint64_t> parseCapacitiesList(std::string const& filename, storm::gspn::GSPN const& gspn);

}  // namespace api
}  // namespace storm
