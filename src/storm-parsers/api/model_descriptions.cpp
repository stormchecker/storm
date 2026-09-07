#include "model_descriptions.h"

#include "storm-parsers/parser/JaniParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/properties.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/macros.h"

namespace storm {
namespace api {

storm::prism::Program parseProgram(std::string const& filename, bool prismCompatibility, bool simplify) {
    storm::prism::Program program = storm::parser::PrismParser::parse(filename, prismCompatibility);
    if (simplify) {
        program = program.simplify().simplify();
    }
    program.checkValidity();
    return program;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> filterProperties(
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>>& modelAndFormulae, boost::optional<std::vector<std::string>> const& propertyFilter) {
    // eliminate unselected properties.
    if (propertyFilter.is_initialized()) {
        std::vector<storm::jani::Property> newProperties;
        // Make sure to preserve the provided order
        for (auto const& propName : propertyFilter.get()) {
            bool found = false;
            for (auto const& property : modelAndFormulae.second) {
                if (property.getName() == propName) {
                    newProperties.push_back(std::move(property));
                    found = true;
                    break;
                }
            }
            STORM_LOG_ERROR_COND(found, "No JANI property with name '" << propName << "' is known.");
        }
        modelAndFormulae.second = std::move(newProperties);
    }
    modelAndFormulae.first.checkValid();
    return modelAndFormulae;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> parseJaniModel(std::string const& filename,
                                                                                 boost::optional<std::vector<std::string>> const& propertyFilter) {
    bool parseProperties = !propertyFilter.is_initialized() || !propertyFilter.get().empty();
    auto modelAndFormulae = storm::parser::JaniParser<storm::RationalNumber>::parse(filename, parseProperties);
    filterProperties(modelAndFormulae, propertyFilter);
    modelAndFormulae.second = storm::api::substituteConstantsInProperties(modelAndFormulae.second, modelAndFormulae.first.getConstantsSubstitution());
    return modelAndFormulae;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> parseJaniModelFromString(std::string const& jsonstring,
                                                                                           boost::optional<std::vector<std::string>> const& propertyFilter) {
    bool parseProperties = !propertyFilter.is_initialized() || !propertyFilter.get().empty();
    auto modelAndFormulae = storm::parser::JaniParser<storm::RationalNumber>::parseFromString(jsonstring, parseProperties);
    filterProperties(modelAndFormulae, propertyFilter);
    modelAndFormulae.second = storm::api::substituteConstantsInProperties(modelAndFormulae.second, modelAndFormulae.first.getConstantsSubstitution());
    return modelAndFormulae;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> parseJaniModel(std::string const& filename,
                                                                                 storm::jani::ModelFeatures const& supportedFeatures,
                                                                                 boost::optional<std::vector<std::string>> const& propertyFilter) {
    auto modelAndFormulae = parseJaniModel(filename, propertyFilter);
    simplifyJaniModel(modelAndFormulae.first, modelAndFormulae.second, supportedFeatures);
    return modelAndFormulae;
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> parseJaniModelFromString(std::string const& jsonstring,
                                                                                           storm::jani::ModelFeatures const& supportedFeatures,
                                                                                           boost::optional<std::vector<std::string>> const& propertyFilter) {
    auto modelAndFormulae = parseJaniModelFromString(jsonstring, propertyFilter);
    simplifyJaniModel(modelAndFormulae.first, modelAndFormulae.second, supportedFeatures);
    return modelAndFormulae;
}

void simplifyJaniModel(storm::jani::Model& model, std::vector<storm::jani::Property>& properties, storm::jani::ModelFeatures const& supportedFeatures) {
    auto nonEliminatedFeatures = model.restrictToFeatures(supportedFeatures, properties);
    STORM_LOG_THROW(nonEliminatedFeatures.empty(), storm::exceptions::NotSupportedException,
                    "The used model feature(s) " << nonEliminatedFeatures.toString() << " is/are not in the list of supported features.");
}

}  // namespace api
}  // namespace storm
