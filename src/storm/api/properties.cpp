#include "storm/api/properties.h"

#include <boost/algorithm/string.hpp>
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/prism/Program.h"

#include "storm/logic/Formula.h"

#include "storm/utility/cli.h"

namespace storm {
namespace api {

std::vector<storm::jani::Property> substituteConstantsInProperties(std::vector<storm::jani::Property> const& properties,
                                                                   std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) {
    std::vector<storm::jani::Property> preprocessedProperties;
    for (auto const& property : properties) {
        preprocessedProperties.emplace_back(property.substitute(substitution));
    }
    return preprocessedProperties;
}

std::vector<storm::jani::Property> substituteTranscendentalNumbersInProperties(std::vector<storm::jani::Property> const& properties) {
    std::vector<storm::jani::Property> preprocessedProperties;
    for (auto const& property : properties) {
        preprocessedProperties.emplace_back(property.substituteTranscendentalNumbers());
    }
    return preprocessedProperties;
}

std::vector<storm::jani::Property> filterProperties(std::vector<storm::jani::Property> const& properties,
                                                    boost::optional<std::set<std::string>> const& propertyFilter) {
    if (propertyFilter) {
        std::set<std::string> const& propertyNameSet = propertyFilter.get();
        std::vector<storm::jani::Property> result;
        std::set<std::string> reducedPropertyNames;

        if (propertyNameSet.empty()) {
            STORM_LOG_WARN("Filtering all properties.");
        }

        for (auto const& property : properties) {
            if (propertyNameSet.find(property.getName()) != propertyNameSet.end()) {
                result.push_back(property);
                reducedPropertyNames.insert(property.getName());
            }
        }

        if (reducedPropertyNames.size() < propertyNameSet.size()) {
            std::set<std::string> missingProperties;
            std::set_difference(propertyNameSet.begin(), propertyNameSet.end(), reducedPropertyNames.begin(), reducedPropertyNames.end(),
                                std::inserter(missingProperties, missingProperties.begin()));
            STORM_LOG_WARN("Filtering unknown properties " << boost::join(missingProperties, ", ") << ".");
        }

        return result;
    } else {
        return properties;
    }
}

std::vector<std::shared_ptr<storm::logic::Formula const>> extractFormulasFromProperties(std::vector<storm::jani::Property> const& properties) {
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
    for (auto const& prop : properties) {
        formulas.push_back(prop.getRawFormula());
    }
    return formulas;
}

storm::jani::Property createCvarProperty(storm::jani::Property const& property, double alpha) {
    STORM_LOG_THROW(property.getFilter().isDefault(), storm::exceptions::InvalidArgumentException,
                    "Non-default property filter of property " << property.getName() << " is not supported for CVaR queries.");
    auto cvarFormula = std::make_shared<storm::logic::CvarFormula>(alpha, property.getRawFormula());
    return storm::jani::Property(property.getName(), cvarFormula, property.getUndefinedConstants(), property.getComment());
}

storm::jani::Property createMultiObjectiveProperty(std::vector<storm::jani::Property> const& properties, bool lexicographic) {
    std::set<storm::expressions::Variable> undefConstants;
    std::string name = "";
    std::string comment = "";
    for (auto const& prop : properties) {
        undefConstants.insert(prop.getUndefinedConstants().begin(), prop.getUndefinedConstants().end());
        name += prop.getName();
        comment += prop.getComment();
        STORM_LOG_WARN_COND(prop.getFilter().isDefault(),
                            "Non-default property filter of property " + prop.getName() + " will be dropped during conversion to multi-objective property.");
    }
    auto multiFormula = std::make_shared<storm::logic::MultiObjectiveFormula>(
        extractFormulasFromProperties(properties),
        lexicographic ? storm::logic::MultiObjectiveFormula::Type::Lexicographic : storm::logic::MultiObjectiveFormula::Type::Tradeoff);
    return storm::jani::Property(name, multiFormula, undefConstants, comment);
}
}  // namespace api
}  // namespace storm
