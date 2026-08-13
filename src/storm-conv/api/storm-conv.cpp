#include "storm-conv/api/storm-conv.h"

#include "storm/api/properties.h"
#include "storm/io/file.h"
#include "storm/storage/jani/Constant.h"
#include "storm/storage/jani/JaniLocationExpander.h"
#include "storm/storage/jani/JaniScopeChanger.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/localeliminator/AutomaticAction.h"
#include "storm/storage/jani/localeliminator/JaniLocalEliminator.h"
#include "storm/storage/jani/visitor/JSONExporter.h"
#include "storm/storage/prism/Program.h"

namespace storm {
namespace api {

void transformJani(storm::jani::Model& janiModel, std::vector<storm::jani::Property>& properties, storm::converter::JaniConversionOptions const& options,
                   std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory) {
    if (options.replaceUnassignedVariablesWithConstants) {
        janiModel.replaceUnassignedVariablesWithConstants();
    }

    if (options.substituteConstants) {
        janiModel.substituteConstantsInPlace(false);
    }

    if (options.localVars) {
        STORM_LOG_WARN_COND(!options.globalVars, "Ignoring 'globalvars' option, since 'localvars' is also set.");
        storm::jani::JaniScopeChanger().makeVariablesLocal(janiModel, properties);
    } else if (options.globalVars) {
        storm::jani::JaniScopeChanger().makeVariablesGlobal(janiModel);
    }

    if (!options.locationVariables.empty()) {
        // Make variables local if necessary/possible
        for (auto const& pair : options.locationVariables) {
            if (janiModel.hasGlobalVariable(pair.second)) {
                auto var = janiModel.getGlobalVariable(pair.second).getExpressionVariable();
                if (storm::jani::JaniScopeChanger().canMakeVariableLocal(var, janiModel, properties, janiModel.getAutomatonIndex(pair.first)).first) {
                    storm::jani::JaniScopeChanger().makeVariableLocal(var, janiModel, janiModel.getAutomatonIndex(pair.first));
                } else {
                    STORM_LOG_ERROR("Can not transform variable " << pair.second << " into locations since it can not be made local to automaton " << pair.first
                                                                  << ".");
                }
            }
        }

        for (auto const& pair : options.locationVariables) {
            storm::jani::JaniLocationExpander expander(janiModel);
            janiModel = expander.transform(pair.first, pair.second).newModel;
        }
    }

    if (options.simplifyComposition) {
        janiModel.simplifyComposition();
    }

    if (options.locationElimination) {
        auto locationHeuristic = options.locationEliminationLocationHeuristic;
        auto edgesHeuristic = options.locationEliminationEdgeHeuristic;
        janiModel = storm::jani::JaniLocalEliminator::eliminateAutomatically(janiModel, properties, locationHeuristic, edgesHeuristic);
    }

    if (options.flatten) {
        if (!smtSolverFactory) {
            smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
        }
        janiModel = janiModel.flattenComposition(smtSolverFactory);
    }

    if (!options.edgeAssignments) {
        janiModel.pushEdgeAssignmentsToDestinations();
    }

    auto uneliminatedFeatures = janiModel.restrictToFeatures(options.allowedModelFeatures, properties);
    STORM_LOG_WARN_COND(uneliminatedFeatures.empty(), "The following model features could not be eliminated: " << uneliminatedFeatures.toString());

    if (options.modelName) {
        janiModel.setName(options.modelName.get());
    }

    if (options.addPropertyConstants) {
        for (auto& f : properties) {
            for (auto const& constant : f.getUndefinedConstants()) {
                if (!janiModel.hasConstant(constant.getName())) {
                    janiModel.addConstant(storm::jani::Constant(constant.getName(), constant));
                }
            }
        }
    }
}

void transformPrism(storm::prism::Program& prismProgram, std::vector<storm::jani::Property>& properties, bool simplify, bool flatten) {
    if (simplify) {
        prismProgram = prismProgram.simplify().simplify();
        properties = storm::api::substituteConstantsInProperties(properties, prismProgram.getConstantsFormulasSubstitution());
    }
    if (flatten) {
        prismProgram = prismProgram.flattenModules();
        if (simplify) {
            // Let's simplify the flattened program again ... just to be sure ... twice ...
            prismProgram = prismProgram.simplify().simplify();
        }
    }
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> convertPrismToJani(storm::prism::Program const& program,
                                                                                     storm::converter::PrismToJaniConverterOptions options) {
    return convertPrismToJani(program, {}, options);
}

std::pair<storm::jani::Model, std::vector<storm::jani::Property>> convertPrismToJani(storm::prism::Program const& program,
                                                                                     std::vector<storm::jani::Property> const& properties,
                                                                                     storm::converter::PrismToJaniConverterOptions options) {
    // Perform conversion
    auto res = program.toJani(properties, options.allVariablesGlobal);
    if (res.second.empty()) {
        std::vector<storm::jani::Property> clonedProperties;
        for (auto const& p : properties) {
            clonedProperties.push_back(p.clone());
        }
        res.second = std::move(clonedProperties);
    }

    // Postprocess Jani model based on the options
    transformJani(res.first, res.second, options.janiOptions);

    return res;
}

void exportJaniToFile(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::string const& filename, bool compact) {
    storm::jani::JsonExporter::toFile(model, properties, filename, true, compact);
}

void printJaniToStream(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::ostream& ostream, bool compact) {
    storm::jani::JsonExporter::toStream(model, properties, ostream, true, compact);
}

void exportPrismToFile(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    stream << program << '\n';
    storm::io::closeFile(stream);

    if (!properties.empty()) {
        storm::io::openFile(filename + ".props", stream);
        for (auto const& prop : properties) {
            stream << prop.asPrismSyntax() << '\n';
            STORM_LOG_WARN_COND(!prop.containsUndefinedConstants(), "A property contains undefined constants. These might not be exported correctly.");
        }
        storm::io::closeFile(stream);
    }
}
void printPrismToStream(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::ostream& ostream) {
    ostream << program << '\n';
    for (auto const& prop : properties) {
        STORM_LOG_WARN_COND(!prop.containsUndefinedConstants(), "A property contains undefined constants. These might not be exported correctly.");
        ostream << prop.asPrismSyntax() << '\n';
    }
}

}  // namespace api
}  // namespace storm
