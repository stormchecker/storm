#include "storm/builder/DdJaniModelBuilder.h"

#include <boost/algorithm/string/join.hpp>
#include <sstream>

#include "storm/adapters/AddExpressionAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/Formulas.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/Variable.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"
#include "storm/storage/jani/types/AllJaniTypes.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/utility/dd.h"
#include "storm/utility/macros.h"
#include "storm/utility/math.h"

namespace storm {
namespace builder {

template<storm::dd::DdType Type, typename ValueType>
storm::jani::ModelFeatures DdJaniModelBuilder<Type, ValueType>::getSupportedJaniFeatures() {
    storm::jani::ModelFeatures features;
    features.add(storm::jani::ModelFeature::DerivedOperators);
    features.add(storm::jani::ModelFeature::StateExitRewards);
    features.add(storm::jani::ModelFeature::MultiObjectiveProperties);
    // We do not add Functions and arrays as these should ideally be substituted before creating this generator.
    // This is because functions or arrays may also occur in properties and the user of this builder should take care of that.
    return features;
}

template<storm::dd::DdType Type, typename ValueType>
bool DdJaniModelBuilder<Type, ValueType>::canHandle(storm::jani::Model const& model, storm::OptionalRef<std::vector<storm::jani::Property> const> properties) {
    // Check jani features
    auto features = model.getModelFeatures();
    features.remove(storm::jani::ModelFeature::Arrays);  // can be substituted
    features.remove(storm::jani::ModelFeature::DerivedOperators);
    features.remove(storm::jani::ModelFeature::Functions);  // can be substituted
    features.remove(storm::jani::ModelFeature::StateExitRewards);
    features.remove(storm::jani::ModelFeature::MultiObjectiveProperties);
    if (!features.empty()) {
        STORM_LOG_INFO("Symbolic engine can not build Jani model due to unsupported jani features.");
        return false;
    }
    // Check assignment levels
    if (model.usesAssignmentLevels()) {
        STORM_LOG_INFO("Symbolic engine can not build Jani model due to assignment levels.");
        return false;
    }
    // Check nonTrivial reward expressions
    if (properties) {
        std::set<std::string> rewardModels;
        for (auto const& p : properties.value()) {
            p.gatherReferencedRewardModels(rewardModels);
        }
        for (auto const& r : rewardModels) {
            if (model.isNonTrivialRewardModelExpression(r)) {
                STORM_LOG_INFO("Symbolic engine can not build Jani model due to non-trivial reward expressions.");
                return false;
            }
        }
    } else {
        if (model.hasNonTrivialRewardExpression()) {
            STORM_LOG_INFO("Symbolic engine can not build Jani model due to non-trivial reward expressions.");
            return false;
        }
    }

    // There probably are more cases where the model is unsupported. However, checking these is often more involved.
    // As this method is supposed to be a quick check, we just return true at this point.
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
DdJaniModelBuilder<Type, ValueType>::Options::Options(bool buildAllLabels, bool buildAllRewardModels, bool applyMaximumProgressAssumption)
    : buildAllLabels(buildAllLabels),
      buildAllRewardModels(buildAllRewardModels),
      applyMaximumProgressAssumption(applyMaximumProgressAssumption),
      rewardModelsToBuild(),
      constantDefinitions() {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
DdJaniModelBuilder<Type, ValueType>::Options::Options(storm::logic::Formula const& formula)
    : buildAllRewardModels(false), rewardModelsToBuild(), constantDefinitions() {
    this->preserveFormula(formula);
    this->setTerminalStatesFromFormula(formula);
}

template<storm::dd::DdType Type, typename ValueType>
DdJaniModelBuilder<Type, ValueType>::Options::Options(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas)
    : buildAllLabels(false), buildAllRewardModels(false), rewardModelsToBuild(), constantDefinitions() {
    if (!formulas.empty()) {
        for (auto const& formula : formulas) {
            this->preserveFormula(*formula);
        }
        if (formulas.size() == 1) {
            this->setTerminalStatesFromFormula(*formulas.front());
        }
    }
}

template<storm::dd::DdType Type, typename ValueType>
void DdJaniModelBuilder<Type, ValueType>::Options::preserveFormula(storm::logic::Formula const& formula) {
    // If we already had terminal states, we need to erase them.
    terminalStates.clear();

    // If we are not required to build all reward models, we determine the reward models we need to build.
    if (!buildAllRewardModels) {
        std::set<std::string> referencedRewardModels = formula.getReferencedRewardModels();
        rewardModelsToBuild.insert(referencedRewardModels.begin(), referencedRewardModels.end());
    }

    // Extract all the labels used in the formula.
    std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> atomicLabelFormulas = formula.getAtomicLabelFormulas();
    for (auto const& formula : atomicLabelFormulas) {
        addLabel(formula->getLabel());
    }
}

template<storm::dd::DdType Type, typename ValueType>
void DdJaniModelBuilder<Type, ValueType>::Options::setTerminalStatesFromFormula(storm::logic::Formula const& formula) {
    terminalStates = getTerminalStatesFromFormula(formula);
}

template<storm::dd::DdType Type, typename ValueType>
std::set<std::string> const& DdJaniModelBuilder<Type, ValueType>::Options::getRewardModelNames() const {
    return rewardModelsToBuild;
}

template<storm::dd::DdType Type, typename ValueType>
bool DdJaniModelBuilder<Type, ValueType>::Options::isBuildAllRewardModelsSet() const {
    return buildAllRewardModels;
}

template<storm::dd::DdType Type, typename ValueType>
bool DdJaniModelBuilder<Type, ValueType>::Options::isBuildAllLabelsSet() const {
    return buildAllLabels;
}

template<storm::dd::DdType Type, typename ValueType>
void DdJaniModelBuilder<Type, ValueType>::Options::addLabel(std::string const& labelName) {
    STORM_LOG_THROW(!buildAllLabels, storm::exceptions::InvalidStateException, "Cannot add label, because all labels are built anyway.");
    labelNames.insert(labelName);
}

template<storm::dd::DdType Type, typename ValueType>
class ParameterCreator {
   public:
    void create(storm::jani::Model const& /*model*/, storm::adapters::AddExpressionAdapter<Type, ValueType>& /*rowExpressionAdapter*/) {
        // Intentionally left empty: no support for parameters for this data type.
    }

    std::set<storm::RationalFunctionVariable> const& getParameters() const {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Creating parameters for non-parametric model is not supported.");
    }

   private:
};

template<storm::dd::DdType Type>
class ParameterCreator<Type, storm::RationalFunction> {
   public:
    ParameterCreator() : cache(std::make_shared<storm::RawPolynomialCache>()) {
        // Intentionally left empty.
    }

    void create(storm::jani::Model const& model, storm::adapters::AddExpressionAdapter<Type, storm::RationalFunction>& rowExpressionAdapter) {
        for (auto const& constant : model.getConstants()) {
            if (!constant.isDefined()) {
                storm::RationalFunctionVariable carlVariable = carl::freshRealVariable(constant.getExpressionVariable().getName());
                parameters.insert(carlVariable);
                auto rf = convertVariableToPolynomial(carlVariable);
                rowExpressionAdapter.setValue(constant.getExpressionVariable(), rf);
            }
        }
    }

    template<typename RationalFunctionType = storm::RationalFunction, typename TP = typename RationalFunctionType::PolyType,
             carl::EnableIf<carl::needs_cache<TP>> = carl::dummy>
    RationalFunctionType convertVariableToPolynomial(storm::RationalFunctionVariable const& variable) {
        return RationalFunctionType(typename RationalFunctionType::PolyType(typename RationalFunctionType::PolyType::PolyType(variable), cache));
    }

    template<typename RationalFunctionType = storm::RationalFunction, typename TP = typename RationalFunctionType::PolyType,
             carl::DisableIf<carl::needs_cache<TP>> = carl::dummy>
    RationalFunctionType convertVariableToPolynomial(storm::RationalFunctionVariable const& variable) {
        return RationalFunctionType(variable);
    }

    std::set<storm::RationalFunctionVariable> const& getParameters() const {
        return parameters;
    }

   private:
    // A mapping from our variables to carl's.
    std::unordered_map<storm::expressions::Variable, storm::RationalFunctionVariable> variableToVariableMap;

    // The cache that is used in case the underlying type needs a cache.
    std::shared_ptr<storm::RawPolynomialCache> cache;

    // All created parameters.
    std::set<storm::RationalFunctionVariable> parameters;
};

template<storm::dd::DdType Type, typename ValueType>
struct CompositionVariables {
    CompositionVariables(std::shared_ptr<storm::dd::DdManager<Type>> const& manager)
        : manager(manager),
          variableToRowMetaVariableMap(std::make_shared<std::map<storm::expressions::Variable, storm::expressions::Variable>>()),
          rowExpressionAdapter(std::make_shared<storm::adapters::AddExpressionAdapter<Type, ValueType>>(manager, variableToRowMetaVariableMap)),
          variableToColumnMetaVariableMap(std::make_shared<std::map<storm::expressions::Variable, storm::expressions::Variable>>()) {
        // Intentionally left empty.
    }

    std::shared_ptr<storm::dd::DdManager<Type>> manager;

    // The meta variables for the row encoding.
    std::set<storm::expressions::Variable> rowMetaVariables;
    std::shared_ptr<std::map<storm::expressions::Variable, storm::expressions::Variable>> variableToRowMetaVariableMap;
    std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter;

    // The meta variables for the column encoding.
    std::set<storm::expressions::Variable> columnMetaVariables;
    std::shared_ptr<std::map<storm::expressions::Variable, storm::expressions::Variable>> variableToColumnMetaVariableMap;

    // All pairs of row/column meta variables.
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs;

    // A mapping from automata to the meta variables encoding their location.
    std::map<std::string, std::pair<storm::expressions::Variable, storm::expressions::Variable>> automatonToLocationDdVariableMap;

    // A mapping from action indices to the meta variables used to encode these actions.
    std::map<uint64_t, storm::expressions::Variable> actionVariablesMap;

    // The meta variables used to encode the remaining nondeterminism.
    std::vector<storm::expressions::Variable> localNondeterminismVariables;

    // The meta variable used to distinguish Markovian from probabilistic choices in Markov automata.
    storm::expressions::Variable probabilisticNondeterminismVariable;
    storm::dd::Bdd<Type> probabilisticMarker;

    // The meta variables used to encode the actions and nondeterminism.
    std::set<storm::expressions::Variable> allNondeterminismVariables;

    // DDs representing the identity for each variable.
    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> variableToIdentityMap;

    // DDs representing the ranges of each variable.
    std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> variableToRangeMap;

    // A set of all meta variables that correspond to global variables.
    std::set<storm::expressions::Variable> allGlobalVariables;

    // DDs representing the identity for each automaton.
    std::map<std::string, storm::dd::Add<Type, ValueType>> automatonToIdentityMap;

    // DDs representing the valid ranges of the variables of each automaton.
    std::map<std::string, storm::dd::Add<Type, ValueType>> automatonToRangeMap;

    // A DD representing the valid ranges of the global variables.
    storm::dd::Add<Type, ValueType> globalVariableRanges;

    // The parameters that appear in the model.
    std::set<storm::RationalFunctionVariable> parameters;
};

// A class responsible for creating the necessary variables for a subsequent composition of automata.
template<storm::dd::DdType Type, typename ValueType>
class CompositionVariableCreator : public storm::jani::CompositionVisitor {
   public:
    CompositionVariableCreator(storm::jani::Model const& model, storm::jani::CompositionInformation const& actionInformation)
        : model(model), automata(), actionInformation(actionInformation) {
        // Intentionally left empty.
    }

    CompositionVariables<Type, ValueType> create(std::shared_ptr<storm::dd::DdManager<Type>> const& manager) {
        // First, check whether every automaton appears exactly once in the system composition. Simultaneously,
        // we determine the set of non-silent actions used by the composition.
        automata.clear();
        this->model.getSystemComposition().accept(*this, boost::none);
        STORM_LOG_THROW(automata.size() == this->model.getNumberOfAutomata(), storm::exceptions::InvalidArgumentException,
                        "Cannot build symbolic model from JANI model whose system composition refers to a subset of automata.");

        STORM_LOG_THROW(!this->model.hasTransientEdgeDestinationAssignments(), storm::exceptions::InvalidArgumentException,
                        "The symbolic JANI model builder currently does not support transient edge destination assignments.");

        // Then, check that the model does not contain non-transient unbounded integer or non-transient real variables.
        STORM_LOG_THROW(!this->model.getGlobalVariables().containsNonTransientUnboundedIntegerVariables(), storm::exceptions::InvalidArgumentException,
                        "Cannot build symbolic model from JANI model that contains non-transient global unbounded integer variables.");
        STORM_LOG_THROW(!this->model.getGlobalVariables().containsNonTransientRealVariables(), storm::exceptions::InvalidArgumentException,
                        "Cannot build symbolic model from JANI model that contains global non-transient real variables.");
        for (auto const& automaton : this->model.getAutomata()) {
            STORM_LOG_THROW(!automaton.getVariables().containsNonTransientUnboundedIntegerVariables(), storm::exceptions::InvalidArgumentException,
                            "Cannot build symbolic model from JANI model that contains non-transient unbounded integer variables in automaton '"
                                << automaton.getName() << "'.");
            STORM_LOG_THROW(
                !automaton.getVariables().containsNonTransientRealVariables(), storm::exceptions::InvalidArgumentException,
                "Cannot build symbolic model from JANI model that contains non-transient real variables in automaton '" << automaton.getName() << "'.");
        }

        // Based on this assumption, we create the variables.
        return createVariables(manager);
    }

    boost::any visit(storm::jani::AutomatonComposition const& composition, boost::any const&) override {
        auto it = automata.find(composition.getAutomatonName());
        STORM_LOG_THROW(it == automata.end(), storm::exceptions::InvalidArgumentException,
                        "Cannot build symbolic model from JANI model whose system composition refers to the automaton '" << composition.getAutomatonName()
                                                                                                                         << "' multiple times.");
        automata.insert(it, composition.getAutomatonName());
        return boost::none;
    }

    boost::any visit(storm::jani::ParallelComposition const& composition, boost::any const& data) override {
        for (auto const& subcomposition : composition.getSubcompositions()) {
            subcomposition->accept(*this, data);
        }
        return boost::none;
    }

   private:
    CompositionVariables<Type, ValueType> createVariables(std::shared_ptr<storm::dd::DdManager<Type>> const& manager) {
        CompositionVariables<Type, ValueType> result(manager);

        for (auto const& nonSilentActionIndex : actionInformation.getNonSilentActionIndices()) {
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair =
                result.manager->addMetaVariable(actionInformation.getActionName(nonSilentActionIndex));
            result.actionVariablesMap[nonSilentActionIndex] = variablePair.first;
            result.allNondeterminismVariables.insert(variablePair.first);
        }

        // FIXME: check how many nondeterminism variables we should actually allocate.
        uint64_t numberOfNondeterminismVariables = this->model.getNumberOfAutomata();
        for (auto const& automaton : this->model.getAutomata()) {
            numberOfNondeterminismVariables += automaton.getNumberOfEdges();
        }
        for (uint_fast64_t i = 0; i < numberOfNondeterminismVariables; ++i) {
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair = result.manager->addMetaVariable("nondet" + std::to_string(i));
            result.localNondeterminismVariables.push_back(variablePair.first);
            result.allNondeterminismVariables.insert(variablePair.first);
        }

        if (this->model.getModelType() == storm::jani::ModelType::MA) {
            result.probabilisticNondeterminismVariable = result.manager->addMetaVariable("prob").first;
            result.probabilisticMarker = result.manager->getEncoding(result.probabilisticNondeterminismVariable, 1);
            result.allNondeterminismVariables.insert(result.probabilisticNondeterminismVariable);
        }

        for (auto const& automatonName : this->automata) {
            storm::jani::Automaton const& automaton = this->model.getAutomaton(automatonName);

            // Start by creating a meta variable for the location of the automaton.
            storm::expressions::Variable locationExpressionVariable = automaton.getLocationExpressionVariable();
            std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair =
                result.manager->addMetaVariable("l_" + automaton.getName(), 0, automaton.getNumberOfLocations() - 1);
            result.automatonToLocationDdVariableMap[automaton.getName()] = variablePair;
            result.rowColumnMetaVariablePairs.push_back(variablePair);

            result.variableToRowMetaVariableMap->emplace(locationExpressionVariable, variablePair.first);
            result.variableToColumnMetaVariableMap->emplace(locationExpressionVariable, variablePair.second);

            // Add the location variable to the row/column variables.
            result.rowMetaVariables.insert(variablePair.first);
            result.columnMetaVariables.insert(variablePair.second);

            // Add the legal range for the location variables.
            result.variableToRangeMap.emplace(variablePair.first, result.manager->getRange(variablePair.first));
            result.variableToRangeMap.emplace(variablePair.second, result.manager->getRange(variablePair.second));
        }

        // Create global variables.
        storm::dd::Bdd<Type> globalVariableRanges = result.manager->getBddOne();
        for (auto const& variable : this->model.getGlobalVariables()) {
            // Only create the variable if it's non-transient.
            if (variable.isTransient()) {
                continue;
            }

            createVariable(variable, result);
            globalVariableRanges &= result.manager->getRange(result.variableToRowMetaVariableMap->at(variable.getExpressionVariable()));
        }
        result.globalVariableRanges = globalVariableRanges.template toAdd<ValueType>();

        // Create the variables for the individual automata.
        for (auto const& automaton : this->model.getAutomata()) {
            storm::dd::Bdd<Type> identity = result.manager->getBddOne();
            storm::dd::Bdd<Type> range = result.manager->getBddOne();

            // Add the identity and ranges of the location variables to the ones of the automaton.
            std::pair<storm::expressions::Variable, storm::expressions::Variable> const& locationVariables =
                result.automatonToLocationDdVariableMap[automaton.getName()];
            storm::dd::Bdd<Type> variableIdentity = result.manager->getIdentity(locationVariables.first, locationVariables.second);
            identity &= variableIdentity;
            range &= result.manager->getRange(locationVariables.first);

            // Then create variables for the variables of the automaton.
            for (auto const& variable : automaton.getVariables()) {
                // Only create the variable if it's non-transient.
                if (variable.isTransient()) {
                    continue;
                }

                createVariable(variable, result);
                identity &= result.variableToIdentityMap.at(variable.getExpressionVariable()).toBdd();
                range &= result.manager->getRange(result.variableToRowMetaVariableMap->at(variable.getExpressionVariable()));
            }

            result.automatonToIdentityMap[automaton.getName()] = identity.template toAdd<ValueType>();
            result.automatonToRangeMap[automaton.getName()] = (range && globalVariableRanges).template toAdd<ValueType>();
        }

        ParameterCreator<Type, ValueType> parameterCreator;
        parameterCreator.create(model, *result.rowExpressionAdapter);
        if (std::is_same<ValueType, storm::RationalFunction>::value) {
            result.parameters = parameterCreator.getParameters();
        }

        return result;
    }

    void createVariable(storm::jani::Variable const& variable, CompositionVariables<Type, ValueType>& result) {
        auto const& type = variable.getType();
        if (type.isBasicType() && type.asBasicType().isBooleanType()) {
            createBooleanVariable(variable, result);
        } else if (type.isBoundedType() && type.asBoundedType().isIntegerType()) {
            createBoundedIntegerVariable(variable, result);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Invalid type of variable in JANI model.");
        }
    }

    void createBoundedIntegerVariable(storm::jani::Variable const& variable, CompositionVariables<Type, ValueType>& result) {
        auto const& type = variable.getType().asBoundedType();
        STORM_LOG_THROW(type.hasLowerBound(), storm::exceptions::NotSupportedException,
                        "DdJaniModelBuilder only supports bounded variables. Variable " << variable.getName() << " has no lower bound.");
        STORM_LOG_THROW(type.hasUpperBound(), storm::exceptions::NotSupportedException,
                        "DdJaniModelBuilder only supports bounded variables. Variable " << variable.getName() << " has no upper bound.");
        int_fast64_t low = type.getLowerBound().evaluateAsInt();
        int_fast64_t high = type.getUpperBound().evaluateAsInt();

        std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair =
            result.manager->addMetaVariable(variable.getExpressionVariable().getName(), low, high);

        STORM_LOG_TRACE("Created meta variables for global integer variable: " << variablePair.first.getName() << " and " << variablePair.second.getName()
                                                                               << ".");

        result.rowMetaVariables.insert(variablePair.first);
        result.variableToRowMetaVariableMap->emplace(variable.getExpressionVariable(), variablePair.first);

        result.columnMetaVariables.insert(variablePair.second);
        result.variableToColumnMetaVariableMap->emplace(variable.getExpressionVariable(), variablePair.second);

        storm::dd::Bdd<Type> variableIdentity = result.manager->getIdentity(variablePair.first, variablePair.second);
        result.variableToIdentityMap.emplace(variable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());
        result.rowColumnMetaVariablePairs.push_back(variablePair);
        result.variableToRangeMap.emplace(variablePair.first, result.manager->getRange(variablePair.first));
        result.variableToRangeMap.emplace(variablePair.second, result.manager->getRange(variablePair.second));

        result.allGlobalVariables.insert(variable.getExpressionVariable());
    }

    void createBooleanVariable(storm::jani::Variable const& variable, CompositionVariables<Type, ValueType>& result) {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> variablePair =
            result.manager->addMetaVariable(variable.getExpressionVariable().getName());

        STORM_LOG_TRACE("Created meta variables for global boolean variable: " << variablePair.first.getName() << " and " << variablePair.second.getName()
                                                                               << ".");

        result.rowMetaVariables.insert(variablePair.first);
        result.variableToRowMetaVariableMap->emplace(variable.getExpressionVariable(), variablePair.first);

        result.columnMetaVariables.insert(variablePair.second);
        result.variableToColumnMetaVariableMap->emplace(variable.getExpressionVariable(), variablePair.second);

        storm::dd::Bdd<Type> variableIdentity = result.manager->getIdentity(variablePair.first, variablePair.second);
        result.variableToIdentityMap.emplace(variable.getExpressionVariable(), variableIdentity.template toAdd<ValueType>());

        result.variableToRangeMap.emplace(variablePair.first, result.manager->getRange(variablePair.first));
        result.variableToRangeMap.emplace(variablePair.second, result.manager->getRange(variablePair.second));

        result.rowColumnMetaVariablePairs.push_back(variablePair);
        result.allGlobalVariables.insert(variable.getExpressionVariable());
    }

    storm::jani::Model const& model;
    std::set<std::string> automata;
    storm::jani::CompositionInformation actionInformation;
};

template<storm::dd::DdType Type, typename ValueType>
struct ComposerResult {
    ComposerResult(storm::dd::Add<Type, ValueType> const& transitions,
                   std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientLocationAssignments,
                   std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientEdgeAssignments,
                   storm::dd::Bdd<Type> const& illegalFragment, uint64_t numberOfNondeterminismVariables = 0)
        : transitions(transitions),
          transientLocationAssignments(transientLocationAssignments),
          transientEdgeAssignments(transientEdgeAssignments),
          illegalFragment(illegalFragment),
          numberOfNondeterminismVariables(numberOfNondeterminismVariables) {
        // Intentionally left empty.
    }

    storm::dd::Add<Type, ValueType> transitions;
    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientLocationAssignments;
    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
    storm::dd::Bdd<Type> illegalFragment;
    uint64_t numberOfNondeterminismVariables;
};

// A class that is responsible for performing the actual composition. This
template<storm::dd::DdType Type, typename ValueType>
class SystemComposer : public storm::jani::CompositionVisitor {
   public:
    SystemComposer(storm::jani::Model const& model, CompositionVariables<Type, ValueType> const& variables,
                   std::vector<storm::expressions::Variable> const& transientVariables)
        : model(model), variables(variables), transientVariables(transientVariables) {
        // Intentionally left empty.
    }

    virtual ComposerResult<Type, ValueType> compose() = 0;

   protected:
    // The model that is referred to by the composition.
    storm::jani::Model const& model;

    // The variable to use when building an automaton.
    CompositionVariables<Type, ValueType> const& variables;

    // The transient variables to consider during system composition.
    std::vector<storm::expressions::Variable> transientVariables;
};

// This structure represents an edge destination.
template<storm::dd::DdType Type, typename ValueType>
struct EdgeDestinationDd {
    EdgeDestinationDd(storm::dd::Add<Type, ValueType> const& transitions, std::set<storm::expressions::Variable> const& writtenGlobalVariables = {})
        : transitions(transitions), writtenGlobalVariables(writtenGlobalVariables) {
        // Intentionally left empty.
    }

    storm::dd::Add<Type, ValueType> transitions;
    std::set<storm::expressions::Variable> writtenGlobalVariables;
};

template<storm::dd::DdType Type, typename ValueType>
EdgeDestinationDd<Type, ValueType> buildEdgeDestinationDd(storm::jani::Automaton const& automaton, storm::jani::EdgeDestination const& destination,
                                                          storm::dd::Bdd<Type> const& guard, CompositionVariables<Type, ValueType> const& variables) {
    storm::dd::Add<Type, ValueType> transitions = variables.rowExpressionAdapter->translateExpression(destination.getProbability());

    STORM_LOG_TRACE("Translating edge destination.");

    // Iterate over all assignments (boolean and integer) and build the DD for it.
    std::set<storm::expressions::Variable> assignedVariables;
    for (auto const& assignment : destination.getOrderedAssignments().getNonTransientAssignments()) {
        // Record the variable as being written.
        STORM_LOG_TRACE("Assigning to variable " << variables.variableToRowMetaVariableMap->at(assignment.getExpressionVariable()).getName());
        assignedVariables.insert(assignment.getExpressionVariable());

        // Translate the written variable.
        auto const& primedMetaVariable = variables.variableToColumnMetaVariableMap->at(assignment.getExpressionVariable());
        storm::dd::Add<Type, ValueType> writtenVariable = variables.manager->template getIdentity<ValueType>(primedMetaVariable);

        // Translate the expression that is being assigned.
        storm::dd::Add<Type, ValueType> assignedExpression = variables.rowExpressionAdapter->translateExpression(assignment.getAssignedExpression());

        // Combine the assigned expression with the guard.
        storm::dd::Add<Type, ValueType> result = assignedExpression * guard.template toAdd<ValueType>();

        // Combine the variable and the assigned expression.
        result = result.equals(writtenVariable).template toAdd<ValueType>();
        result *= guard.template toAdd<ValueType>();

        // Restrict the transitions to the range of the written variable.
        result = result * variables.variableToRangeMap.at(primedMetaVariable).template toAdd<ValueType>();

        // Combine the assignment DDs.
        transitions *= result;
    }

    // Compute the set of assigned global variables.
    std::set<storm::expressions::Variable> assignedGlobalVariables;
    std::set_intersection(assignedVariables.begin(), assignedVariables.end(), variables.allGlobalVariables.begin(), variables.allGlobalVariables.end(),
                          std::inserter(assignedGlobalVariables, assignedGlobalVariables.begin()));

    // All unassigned boolean variables need to keep their value.
    for (storm::jani::Variable const& variable : automaton.getVariables().getBooleanVariables()) {
        if (assignedVariables.find(variable.getExpressionVariable()) == assignedVariables.end()) {
            STORM_LOG_TRACE("Multiplying identity of variable " << variable.getName());
            transitions *= variables.variableToIdentityMap.at(variable.getExpressionVariable());
        }
    }

    // All unassigned integer variables need to keep their value.
    for (storm::jani::Variable const& variable : automaton.getVariables().getBoundedIntegerVariables()) {
        if (assignedVariables.find(variable.getExpressionVariable()) == assignedVariables.end()) {
            STORM_LOG_TRACE("Multiplying identity of variable " << variable.getName());
            transitions *= variables.variableToIdentityMap.at(variable.getExpressionVariable());
        }
    }

    transitions *= variables.manager->getEncoding(variables.automatonToLocationDdVariableMap.at(automaton.getName()).second, destination.getLocationIndex())
                       .template toAdd<ValueType>();

    return EdgeDestinationDd<Type, ValueType>(transitions, assignedGlobalVariables);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> encodeAction(boost::optional<uint64_t> const& actionIndex, boost::optional<bool> const& markovian,
                                             CompositionVariables<Type, ValueType> const& variables) {
    storm::dd::Add<Type, ValueType> encoding = variables.manager->template getAddOne<ValueType>();

    for (auto it = variables.actionVariablesMap.rbegin(), ite = variables.actionVariablesMap.rend(); it != ite; ++it) {
        if (actionIndex && it->first == actionIndex.get()) {
            encoding *= variables.manager->getEncoding(it->second, 1).template toAdd<ValueType>();
        } else {
            encoding *= variables.manager->getEncoding(it->second, 0).template toAdd<ValueType>();
        }
    }

    if (markovian) {
        if (markovian.get()) {
            encoding *= (!variables.probabilisticMarker).template toAdd<ValueType>();
        } else {
            encoding *= variables.probabilisticMarker.template toAdd<ValueType>();
        }
    }

    return encoding;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> encodeIndex(uint64_t index, uint64_t localNondeterminismVariableOffset, uint64_t numberOfLocalNondeterminismVariables,
                                            CompositionVariables<Type, ValueType> const& variables) {
    storm::dd::Add<Type, ValueType> result = variables.manager->template getAddZero<ValueType>();

    std::map<storm::expressions::Variable, int_fast64_t> metaVariableNameToValueMap;
    for (uint_fast64_t i = 0; i < numberOfLocalNondeterminismVariables; ++i) {
        if (index & (1ull << (numberOfLocalNondeterminismVariables - i - 1))) {
            metaVariableNameToValueMap.emplace(variables.localNondeterminismVariables[localNondeterminismVariableOffset + i], 1);
        } else {
            metaVariableNameToValueMap.emplace(variables.localNondeterminismVariables[localNondeterminismVariableOffset + i], 0);
        }
    }

    result.setValue(metaVariableNameToValueMap, storm::utility::one<ValueType>());
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
class CombinedEdgesSystemComposer : public SystemComposer<Type, ValueType> {
   public:
    // This structure represents an edge.
    struct EdgeDd {
        EdgeDd(bool isMarkovian, storm::dd::Bdd<Type> const& guard, storm::dd::Add<Type, ValueType> const& transitions,
               std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientEdgeAssignments,
               std::set<storm::expressions::Variable> const& writtenGlobalVariables)
            : isMarkovian(isMarkovian),
              guard(guard),
              transitions(transitions),
              transientEdgeAssignments(transientEdgeAssignments),
              variableToWritingFragment() {
            // Convert the set of written variables to a mapping from variable to the writing fragments.
            for (auto const& variable : writtenGlobalVariables) {
                variableToWritingFragment[variable] = guard;
            }
        }

        EdgeDd(bool isMarkovian, storm::dd::Bdd<Type> const& guard, storm::dd::Add<Type, ValueType> const& transitions,
               std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientEdgeAssignments,
               std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> const& variableToWritingFragment)
            : isMarkovian(isMarkovian),
              guard(guard),
              transitions(transitions),
              transientEdgeAssignments(transientEdgeAssignments),
              variableToWritingFragment(variableToWritingFragment) {
            // Intentionally left empty.
        }

        // A flag storing whether this edge is a Markovian one (i.e. one with a rate).
        bool isMarkovian;

        // A DD that represents all states that have this edge enabled.
        storm::dd::Bdd<Type> guard;

        // A DD that represents the transitions of this edge.
        storm::dd::Add<Type, ValueType> transitions;

        // A mapping from transient variables to the DDs representing their value assignments.
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;

        // A mapping of variables to the variables to the fragment of transitions that is writing the corresponding variable.
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> variableToWritingFragment;
    };

    // This structure represents an edge.
    struct ActionDd {
        ActionDd(storm::dd::Bdd<Type> const& guard = storm::dd::Bdd<Type>(),
                 storm::dd::Add<Type, ValueType> const& transitions = storm::dd::Add<Type, ValueType>(),
                 std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientEdgeAssignments = {},
                 std::pair<uint64_t, uint64_t> localNondeterminismVariables = std::pair<uint64_t, uint64_t>(0, 0),
                 std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> const& variableToWritingFragment = {},
                 storm::dd::Bdd<Type> const& illegalFragment = storm::dd::Bdd<Type>())
            : guard(guard),
              transitions(transitions),
              transientEdgeAssignments(transientEdgeAssignments),
              localNondeterminismVariables(localNondeterminismVariables),
              variableToWritingFragment(variableToWritingFragment),
              illegalFragment(illegalFragment),
              inputEnabled(false) {
            // Intentionally left empty.
        }

        uint64_t getLowestLocalNondeterminismVariable() const {
            return localNondeterminismVariables.first;
        }

        uint64_t getHighestLocalNondeterminismVariable() const {
            return localNondeterminismVariables.second;
        }

        std::pair<uint64_t, uint64_t> const& getLocalNondeterminismVariables() const {
            return localNondeterminismVariables;
        }

        ActionDd multiplyTransitions(storm::dd::Add<Type, ValueType> const& factor) const {
            return ActionDd(guard, transitions * factor, transientEdgeAssignments, localNondeterminismVariables, variableToWritingFragment, illegalFragment);
        }

        ActionDd add(ActionDd const& other) const {
            storm::dd::Bdd<Type> newGuard = this->guard || other.guard;
            storm::dd::Add<Type, ValueType> newTransitions = this->transitions + other.transitions;

            // Join the transient edge assignments.
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> newTransientEdgeAssignments(this->transientEdgeAssignments);
            for (auto const& entry : other.transientEdgeAssignments) {
                auto it = newTransientEdgeAssignments.find(entry.first);
                if (it == newTransientEdgeAssignments.end()) {
                    newTransientEdgeAssignments[entry.first] = entry.second;
                } else {
                    it->second += entry.second;
                }
            }

            std::pair<uint64_t, uint64_t> newLocalNondeterminismVariables =
                std::make_pair(std::min(this->localNondeterminismVariables.first, other.localNondeterminismVariables.first),
                               std::max(this->localNondeterminismVariables.second, other.localNondeterminismVariables.second));

            // Join variable-to-writing-fragment maps.
            std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> newVariableToWritingFragment(this->variableToWritingFragment);
            for (auto const& entry : other.variableToWritingFragment) {
                auto it = newVariableToWritingFragment.find(entry.first);
                if (it == newVariableToWritingFragment.end()) {
                    newVariableToWritingFragment[entry.first] = entry.second;
                } else {
                    it->second |= entry.second;
                }
            }

            // Join illegal fragments.
            storm::dd::Bdd<Type> newIllegalFragment = this->illegalFragment || other.illegalFragment;

            return ActionDd(newGuard, newTransitions, newTransientEdgeAssignments, newLocalNondeterminismVariables, newVariableToWritingFragment,
                            newIllegalFragment);
        }

        /*!
         * Conjuncts the guard of the action with the provided condition, i.e., this action is only enabled if the provided condition is true.
         */
        void conjunctGuardWith(storm::dd::Bdd<Type> const& condition) {
            guard &= condition;
            storm::dd::Add<Type, ValueType> conditionAdd = condition.template toAdd<ValueType>();
            transitions *= conditionAdd;
            for (auto& t : transientEdgeAssignments) {
                t.second *= conditionAdd;
            }
            illegalFragment &= condition;
        }

        bool isInputEnabled() const {
            return inputEnabled;
        }

        void setIsInputEnabled() {
            inputEnabled = true;
        }

        // A DD that represents all states that have this action enabled.
        storm::dd::Bdd<Type> guard;

        // A DD that represents the transitions of this action.
        storm::dd::Add<Type, ValueType> transitions;

        // A mapping from transient variables to their assignments.
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;

        // The local nondeterminism variables used by this action DD, given as the lowest
        std::pair<uint64_t, uint64_t> localNondeterminismVariables;

        // A mapping from global variables to a DD that characterizes choices (nondeterminism variables) in
        // states that write to this global variable.
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> variableToWritingFragment;

        // A DD characterizing the fragment of the states satisfying the guard that are illegal because
        // there are synchronizing edges enabled that write to the same global variable.
        storm::dd::Bdd<Type> illegalFragment;

        // A flag storing whether this action is input-enabled.
        bool inputEnabled;
    };

    struct ActionIdentification {
        ActionIdentification(uint64_t actionIndex, bool markovian = false)
            : actionIndex(actionIndex), synchronizationVectorIndex(boost::none), markovian(markovian) {
            // Intentionally left empty.
        }

        ActionIdentification(uint64_t actionIndex, uint64_t synchronizationVectorIndex, bool markovian = false)
            : actionIndex(actionIndex), synchronizationVectorIndex(synchronizationVectorIndex), markovian(markovian) {
            // Intentionally left empty.
        }

        ActionIdentification(uint64_t actionIndex, boost::optional<uint64_t> synchronizationVectorIndex, bool markovian = false)
            : actionIndex(actionIndex), synchronizationVectorIndex(synchronizationVectorIndex), markovian(markovian) {
            // Intentionally left empty.
        }

        void setMarkovian(bool markovian) {
            this->markovian = markovian;
        }

        bool isMarkovian() const {
            return this->markovian;
        }

        bool operator==(ActionIdentification const& other) const {
            bool result = actionIndex == other.actionIndex && markovian == other.markovian;
            if (synchronizationVectorIndex) {
                if (other.synchronizationVectorIndex) {
                    result &= synchronizationVectorIndex.get() == other.synchronizationVectorIndex.get();
                } else {
                    result = false;
                }
            } else {
                if (other.synchronizationVectorIndex) {
                    result = false;
                }
            }
            return result;
        }

        uint64_t actionIndex;
        boost::optional<uint64_t> synchronizationVectorIndex;
        bool markovian;
    };

    struct ActionIdentificationHash {
        std::size_t operator()(ActionIdentification const& identification) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, identification.actionIndex);
            if (identification.synchronizationVectorIndex) {
                boost::hash_combine(seed, identification.synchronizationVectorIndex.get());
            }
            return identification.markovian ? ~seed : seed;
        }
    };

    // This structure represents a subcomponent of a composition.
    struct AutomatonDd {
        AutomatonDd(storm::dd::Add<Type, ValueType> const& identity,
                    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientLocationAssignments = {})
            : actions(),
              transientLocationAssignments(transientLocationAssignments),
              identity(identity),
              localNondeterminismVariables(std::make_pair<uint64_t, uint64_t>(0, 0)) {
            // Intentionally left empty.
        }

        uint64_t getLowestLocalNondeterminismVariable() const {
            return localNondeterminismVariables.first;
        }

        void setLowestLocalNondeterminismVariable(uint64_t newValue) {
            localNondeterminismVariables.first = newValue;
        }

        uint64_t getHighestLocalNondeterminismVariable() const {
            return localNondeterminismVariables.second;
        }

        void setHighestLocalNondeterminismVariable(uint64_t newValue) {
            localNondeterminismVariables.second = newValue;
        }

        void extendLocalNondeterminismVariables(std::pair<uint64_t, uint64_t> const& localNondeterminismVariables) {
            setLowestLocalNondeterminismVariable(std::min(localNondeterminismVariables.first, getLowestLocalNondeterminismVariable()));
            setHighestLocalNondeterminismVariable(std::max(localNondeterminismVariables.second, getHighestLocalNondeterminismVariable()));
        }

        // A mapping from action identifications to the action DDs.
        std::unordered_map<ActionIdentification, ActionDd, ActionIdentificationHash> actions;

        // A mapping from transient variables to their location-based transient assignment values.
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientLocationAssignments;

        // The identity of the automaton's variables.
        storm::dd::Add<Type, ValueType> identity;

        // The local nondeterminism variables used by this action DD, given as the lowest and highest variable index.
        std::pair<uint64_t, uint64_t> localNondeterminismVariables;
    };

    CombinedEdgesSystemComposer(storm::jani::Model const& model, storm::jani::CompositionInformation const& actionInformation,
                                CompositionVariables<Type, ValueType> const& variables, std::vector<storm::expressions::Variable> const& transientVariables,
                                bool applyMaximumProgress)
        : SystemComposer<Type, ValueType>(model, variables, transientVariables),
          actionInformation(actionInformation),
          applyMaximumProgress(applyMaximumProgress) {
        // Intentionally left empty.
    }

    storm::jani::CompositionInformation const& actionInformation;
    bool applyMaximumProgress;

    ComposerResult<Type, ValueType> compose() override {
        STORM_LOG_THROW(this->model.hasStandardCompliantComposition(), storm::exceptions::WrongFormatException,
                        "Model builder only supports non-nested parallel compositions.");
        AutomatonDd globalAutomaton = boost::any_cast<AutomatonDd>(this->model.getSystemComposition().accept(*this, boost::any()));
        return buildSystemFromAutomaton(globalAutomaton);
    }

    struct ActionInstantiation {
        ActionInstantiation(uint64_t actionIndex, uint64_t synchronizationVectorIndex, uint64_t localNondeterminismVariableOffset, bool markovian = false)
            : actionIndex(actionIndex),
              synchronizationVectorIndex(synchronizationVectorIndex),
              localNondeterminismVariableOffset(localNondeterminismVariableOffset),
              markovian(markovian) {
            // Intentionally left empty.
        }

        ActionInstantiation(uint64_t actionIndex, uint64_t localNondeterminismVariableOffset, bool markovian = false)
            : actionIndex(actionIndex), localNondeterminismVariableOffset(localNondeterminismVariableOffset), markovian(markovian) {
            // Intentionally left empty.
        }

        void setMarkovian(bool markovian) {
            this->markovian = markovian;
        }

        bool isMarkovian() const {
            return this->markovian;
        }

        bool operator==(ActionInstantiation const& other) const {
            bool result = actionIndex == other.actionIndex && markovian == other.markovian;
            result &= localNondeterminismVariableOffset == other.localNondeterminismVariableOffset;
            if (synchronizationVectorIndex) {
                if (!other.synchronizationVectorIndex) {
                    result = false;
                } else {
                    result &= synchronizationVectorIndex.get() == other.synchronizationVectorIndex.get();
                }
            } else {
                if (other.synchronizationVectorIndex) {
                    result = false;
                }
            }
            return result;
        }

        uint64_t actionIndex;
        boost::optional<uint64_t> synchronizationVectorIndex;
        uint64_t localNondeterminismVariableOffset;
        bool markovian;
    };

    struct ActionInstantiationHash {
        std::size_t operator()(ActionInstantiation const& instantiation) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, instantiation.actionIndex);
            boost::hash_combine(seed, instantiation.localNondeterminismVariableOffset);
            if (instantiation.synchronizationVectorIndex) {
                boost::hash_combine(seed, instantiation.synchronizationVectorIndex.get());
            }
            return instantiation.isMarkovian() ? ~seed : seed;
        }
    };

    typedef std::map<uint64_t, std::vector<ActionInstantiation>> ActionInstantiations;

    boost::any visit(storm::jani::AutomatonComposition const& composition, boost::any const& data) override {
        ActionInstantiations actionInstantiations;
        if (data.empty()) {
            // If no data was provided, this is the top level element in which case we build the full automaton.
            bool isCtmc = this->model.getModelType() == storm::jani::ModelType::CTMC;

            for (auto const& actionIndex : actionInformation.getNonSilentActionIndices()) {
                actionInstantiations[actionIndex].emplace_back(actionIndex, 0, isCtmc);
            }
            actionInstantiations[storm::jani::Model::SILENT_ACTION_INDEX].emplace_back(storm::jani::Model::SILENT_ACTION_INDEX, 0, isCtmc);
            if (this->model.getModelType() == storm::jani::ModelType::MA) {
                actionInstantiations[storm::jani::Model::SILENT_ACTION_INDEX].emplace_back(storm::jani::Model::SILENT_ACTION_INDEX, 0, true);
            }
        }

        std::set<uint64_t> inputEnabledActionIndices;
        for (auto const& actionName : composition.getInputEnabledActions()) {
            inputEnabledActionIndices.insert(actionInformation.getActionIndex(actionName));
        }

        return buildAutomatonDd(composition.getAutomatonName(), data.empty() ? actionInstantiations : boost::any_cast<ActionInstantiations const&>(data),
                                inputEnabledActionIndices, data.empty());
    }

    boost::any visit(storm::jani::ParallelComposition const& composition, boost::any const& data) override {
        STORM_LOG_ASSERT(data.empty(), "Expected parallel composition to be on topmost level to be JANI compliant.");

        bool isCtmc = this->model.getModelType() == storm::jani::ModelType::CTMC;

        // Prepare storage for the subautomata of the composition.
        std::vector<AutomatonDd> subautomata;

        // The outer loop iterates over the indices of the subcomposition, because the first subcomposition needs
        // to be built before the second and so on.
        uint64_t silentActionIndex = actionInformation.getActionIndex(storm::jani::Model::SILENT_ACTION_NAME);
        for (uint64_t subcompositionIndex = 0; subcompositionIndex < composition.getNumberOfSubcompositions(); ++subcompositionIndex) {
            // Now build a new set of action instantiations for the current subcomposition index.
            ActionInstantiations actionInstantiations;
            actionInstantiations[silentActionIndex].emplace_back(silentActionIndex, 0, isCtmc);
            if (this->model.getModelType() == storm::jani::ModelType::MA) {
                actionInstantiations[storm::jani::Model::SILENT_ACTION_INDEX].emplace_back(silentActionIndex, 0, true);
            }

            for (uint64_t synchronizationVectorIndex = 0; synchronizationVectorIndex < composition.getNumberOfSynchronizationVectors();
                 ++synchronizationVectorIndex) {
                auto const& synchVector = composition.getSynchronizationVector(synchronizationVectorIndex);

                // Determine the first participating subcomposition, because we need to build the corresponding action
                // from all local nondeterminism variable offsets that the output action of the synchronization vector
                // is required to have.
                if (subcompositionIndex == synchVector.getPositionOfFirstParticipatingAction()) {
                    uint64_t actionIndex = actionInformation.getActionIndex(synchVector.getInput(subcompositionIndex));
                    actionInstantiations[actionIndex].emplace_back(actionIndex, synchronizationVectorIndex, 0, isCtmc);
                } else if (synchVector.getInput(subcompositionIndex) != storm::jani::SynchronizationVector::NO_ACTION_INPUT) {
                    uint64_t actionIndex = actionInformation.getActionIndex(synchVector.getInput(subcompositionIndex));

                    // If this subcomposition is participating in the synchronization vector, but it's not the first
                    // such subcomposition, then we have to retrieve the offset we need for the participating action
                    // by looking at the maximal offset used by the preceding participating action.
                    boost::optional<uint64_t> previousActionPosition = synchVector.getPositionOfPrecedingParticipatingAction(subcompositionIndex);
                    STORM_LOG_ASSERT(previousActionPosition, "Inconsistent information about synchronization vector.");
                    AutomatonDd const& previousAutomatonDd = subautomata[previousActionPosition.get()];
                    auto precedingActionIndex = actionInformation.getActionIndex(synchVector.getInput(previousActionPosition.get()));
                    auto precedingActionIt = previousAutomatonDd.actions.find(ActionIdentification(precedingActionIndex, synchronizationVectorIndex, isCtmc));

                    uint64_t highestLocalNondeterminismVariable = 0;
                    if (precedingActionIt != previousAutomatonDd.actions.end()) {
                        highestLocalNondeterminismVariable = precedingActionIt->second.getHighestLocalNondeterminismVariable();
                    } else {
                        STORM_LOG_WARN("Subcomposition does not have action" << actionInformation.getActionName(precedingActionIndex)
                                                                             << " that is mentioned in parallel composition.");
                    }
                    actionInstantiations[actionIndex].emplace_back(actionIndex, synchronizationVectorIndex, highestLocalNondeterminismVariable, isCtmc);
                }
            }

            subautomata.push_back(boost::any_cast<AutomatonDd>(composition.getSubcomposition(subcompositionIndex).accept(*this, actionInstantiations)));
        }

        return composeInParallel(subautomata, composition.getSynchronizationVectors());
    }

   private:
    AutomatonDd composeInParallel(std::vector<AutomatonDd> const& subautomata, std::vector<storm::jani::SynchronizationVector> const& synchronizationVectors) {
        AutomatonDd result(this->variables.manager->template getAddOne<ValueType>());

        // Disjunction of all guards of non-markovian actions (only required for maximum progress assumption.
        storm::dd::Bdd<Type> nonMarkovianActionGuards = this->variables.manager->getBddZero();

        // Build the results of the synchronization vectors.
        std::unordered_map<ActionIdentification, std::vector<ActionDd>, ActionIdentificationHash> actions;
        for (uint64_t synchronizationVectorIndex = 0; synchronizationVectorIndex < synchronizationVectors.size(); ++synchronizationVectorIndex) {
            auto const& synchVector = synchronizationVectors[synchronizationVectorIndex];

            boost::optional<ActionDd> synchronizingAction = combineSynchronizingActions(subautomata, synchVector, synchronizationVectorIndex);
            if (synchronizingAction) {
                if (applyMaximumProgress) {
                    STORM_LOG_ASSERT(this->model.getModelType() == storm::jani::ModelType::MA,
                                     "Maximum progress assumption enabled for unexpected model type.");
                    // By the JANI standard, we can assume that synchronizing actions of MAs are always non-Markovian.
                    nonMarkovianActionGuards |= synchronizingAction->guard;
                }
                actions[ActionIdentification(actionInformation.getActionIndex(synchVector.getOutput()),
                                             this->model.getModelType() == storm::jani::ModelType::CTMC)]
                    .emplace_back(synchronizingAction.get());
            }
        }

        // Construct the two silent action identifications.
        ActionIdentification silentActionIdentification(storm::jani::Model::SILENT_ACTION_INDEX);
        ActionIdentification silentMarkovianActionIdentification(storm::jani::Model::SILENT_ACTION_INDEX, true);

        // Construct the silent action DDs.
        std::vector<ActionDd> silentActionDds;
        std::vector<ActionDd> silentMarkovianActionDds;
        for (auto const& automaton : subautomata) {
            for (auto& actionDd : silentActionDds) {
                STORM_LOG_TRACE("Extending previous (non-Markovian) silent action by identity of current automaton.");
                actionDd = actionDd.multiplyTransitions(automaton.identity);
            }
            for (auto& actionDd : silentMarkovianActionDds) {
                STORM_LOG_TRACE("Extending previous (Markovian) silent action by identity of current automaton.");
                actionDd = actionDd.multiplyTransitions(automaton.identity);
            }

            auto silentActionIt = automaton.actions.find(silentActionIdentification);
            if (silentActionIt != automaton.actions.end()) {
                STORM_LOG_TRACE("Extending (non-Markovian) silent action by running identity.");
                silentActionDds.emplace_back(silentActionIt->second.multiplyTransitions(result.identity));
            }

            silentActionIt = automaton.actions.find(silentMarkovianActionIdentification);
            if (silentActionIt != automaton.actions.end()) {
                STORM_LOG_TRACE("Extending (Markovian) silent action by running identity.");
                silentMarkovianActionDds.emplace_back(silentActionIt->second.multiplyTransitions(result.identity));
            }

            result.identity *= automaton.identity;

            // Add the transient location assignments of the automata.
            addToTransientAssignmentMap(result.transientLocationAssignments, automaton.transientLocationAssignments);
        }

        if (!silentActionDds.empty()) {
            auto& allSilentActionDds = actions[silentActionIdentification];
            allSilentActionDds.insert(allSilentActionDds.end(), silentActionDds.begin(), silentActionDds.end());
        }

        // Add guards of non-markovian actions
        if (applyMaximumProgress) {
            auto allSilentActionDdsIt = actions.find(silentActionIdentification);
            if (allSilentActionDdsIt != actions.end()) {
                for (ActionDd const& silentActionDd : allSilentActionDdsIt->second) {
                    nonMarkovianActionGuards |= silentActionDd.guard;
                }
            }
        }

        if (!silentMarkovianActionDds.empty()) {
            auto& allMarkovianSilentActionDds = actions[silentMarkovianActionIdentification];
            allMarkovianSilentActionDds.insert(allMarkovianSilentActionDds.end(), silentMarkovianActionDds.begin(), silentMarkovianActionDds.end());
            if (applyMaximumProgress && !nonMarkovianActionGuards.isZero()) {
                auto invertedNonMarkovianGuards = !nonMarkovianActionGuards;
                for (ActionDd& markovianActionDd : allMarkovianSilentActionDds) {
                    markovianActionDd.conjunctGuardWith(invertedNonMarkovianGuards);
                }
            }
        }

        // Finally, combine (potentially) multiple action DDs.
        for (auto const& actionDds : actions) {
            ActionDd combinedAction;
            if (actionDds.first == silentMarkovianActionIdentification) {
                // For the Markovian transitions, we can simply add the actions.
                combinedAction = actionDds.second.front();
                for (uint64_t i = 1; i < actionDds.second.size(); ++i) {
                    combinedAction = combinedAction.add(actionDds.second[i]);
                }
            } else {
                combinedAction = actionDds.second.size() > 1 ? combineUnsynchronizedActions(actionDds.second) : actionDds.second.front();
            }
            result.actions[actionDds.first] = combinedAction;
            result.extendLocalNondeterminismVariables(combinedAction.getLocalNondeterminismVariables());
        }

        // Construct combined identity.
        for (auto const& subautomaton : subautomata) {
            result.identity *= subautomaton.identity;
        }

        return result;
    }

    boost::optional<ActionDd> combineSynchronizingActions(std::vector<AutomatonDd> const& subautomata,
                                                          storm::jani::SynchronizationVector const& synchronizationVector,
                                                          uint64_t synchronizationVectorIndex) {
        std::vector<std::pair<uint64_t, std::reference_wrapper<ActionDd const>>> actions;
        storm::dd::Add<Type, ValueType> nonSynchronizingIdentity = this->variables.manager->template getAddOne<ValueType>();
        for (uint64_t subautomatonIndex = 0; subautomatonIndex < subautomata.size(); ++subautomatonIndex) {
            auto const& subautomaton = subautomata[subautomatonIndex];
            if (synchronizationVector.getInput(subautomatonIndex) != storm::jani::SynchronizationVector::NO_ACTION_INPUT) {
                auto it =
                    subautomaton.actions.find(ActionIdentification(actionInformation.getActionIndex(synchronizationVector.getInput(subautomatonIndex)),
                                                                   synchronizationVectorIndex, this->model.getModelType() == storm::jani::ModelType::CTMC));
                if (it != subautomaton.actions.end()) {
                    actions.emplace_back(subautomatonIndex, it->second);
                } else {
                    return boost::none;
                }
            } else {
                nonSynchronizingIdentity *= subautomaton.identity;
            }
        }

        // If there are only input-enabled actions, we also need to build the disjunction of the guards.
        bool allActionsInputEnabled = true;
        for (auto const& action : actions) {
            if (!action.second.get().isInputEnabled()) {
                allActionsInputEnabled = false;
            }
        }

        boost::optional<storm::dd::Bdd<Type>> guardDisjunction;
        if (allActionsInputEnabled) {
            guardDisjunction = this->variables.manager->getBddZero();
        }

        // Otherwise, construct the synchronization.
        storm::dd::Bdd<Type> illegalFragment = this->variables.manager->getBddZero();

        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> globalVariableToWritingFragment;
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> globalVariableToWritingFragmentWithoutNondeterminism;

        storm::dd::Bdd<Type> inputEnabledGuard = this->variables.manager->getBddOne();
        storm::dd::Add<Type, ValueType> transitions = this->variables.manager->template getAddOne<ValueType>();

        uint64_t lowestNondeterminismVariable = actions.front().second.get().getLowestLocalNondeterminismVariable();
        uint64_t highestNondeterminismVariable = actions.front().second.get().getHighestLocalNondeterminismVariable();

        bool hasTransientEdgeAssignments = false;
        for (auto const& actionIndexPair : actions) {
            auto const& action = actionIndexPair.second.get();
            if (!action.transientEdgeAssignments.empty()) {
                hasTransientEdgeAssignments = true;
                break;
            }
        }

        boost::optional<storm::dd::Add<Type, ValueType>> exitRates;
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
        if (this->model.getModelType() == storm::jani::ModelType::CTMC && hasTransientEdgeAssignments) {
            // For CTMCs, we need to weigh the transient assignments with the exit rates.
            exitRates = this->variables.manager->template getAddOne<ValueType>();
            for (auto const& actionIndexPair : actions) {
                auto const& action = actionIndexPair.second.get();

                std::set<storm::expressions::Variable> columnVariablesToAbstract;
                std::set_intersection(action.transitions.getContainedMetaVariables().begin(), action.transitions.getContainedMetaVariables().end(),
                                      this->variables.columnMetaVariables.begin(), this->variables.columnMetaVariables.end(),
                                      std::inserter(columnVariablesToAbstract, columnVariablesToAbstract.begin()));
                auto actionExitRates = action.transitions.sumAbstract(columnVariablesToAbstract);
                exitRates = exitRates.get() * actionExitRates;

                if (!action.transientEdgeAssignments.empty()) {
                    for (auto const& entry : action.transientEdgeAssignments) {
                        auto transientEdgeAssignmentIt = transientEdgeAssignments.find(entry.first);
                        if (transientEdgeAssignmentIt != transientEdgeAssignments.end()) {
                            transientEdgeAssignmentIt->second *= entry.second / actionExitRates;
                        } else {
                            transientEdgeAssignments.emplace(entry.first, entry.second / actionExitRates);
                        }
                    }
                }
            }
        } else if (hasTransientEdgeAssignments) {
            // Otherwise, just join the assignments.
            for (auto const& actionIndexPair : actions) {
                auto const& action = actionIndexPair.second.get();
                joinTransientAssignmentMapsInPlace(transientEdgeAssignments, action.transientEdgeAssignments);
            }
        }

        storm::dd::Bdd<Type> newIllegalFragment = this->variables.manager->getBddZero();
        for (auto const& actionIndexPair : actions) {
            auto componentIndex = actionIndexPair.first;
            auto const& action = actionIndexPair.second.get();

            if (guardDisjunction) {
                guardDisjunction.get() |= action.guard;
            }

            lowestNondeterminismVariable = std::min(lowestNondeterminismVariable, action.getLowestLocalNondeterminismVariable());
            highestNondeterminismVariable = std::max(highestNondeterminismVariable, action.getHighestLocalNondeterminismVariable());

            if (action.isInputEnabled()) {
                // If the action is input-enabled, we add self-loops to all states.
                transitions *= action.guard.ite(
                    action.transitions,
                    encodeIndex(0, action.getLowestLocalNondeterminismVariable(),
                                action.getHighestLocalNondeterminismVariable() - action.getLowestLocalNondeterminismVariable(), this->variables) *
                        subautomata[componentIndex].identity);
            } else {
                transitions *= action.transitions;
            }

            // Create a set of variables that is used as nondeterminism variables in this action.
            auto nondetVariables =
                std::set<storm::expressions::Variable>(this->variables.localNondeterminismVariables.begin() + action.getLowestLocalNondeterminismVariable(),
                                                       this->variables.localNondeterminismVariables.begin() + action.getHighestLocalNondeterminismVariable());

            for (auto const& entry : action.variableToWritingFragment) {
                storm::dd::Bdd<Type> guardedWritingFragment = inputEnabledGuard && entry.second;

                // Check whether there already is an entry for this variable in the mapping of global variables
                // to their writing fragments.
                auto globalFragmentIt = globalVariableToWritingFragment.find(entry.first);
                if (globalFragmentIt != globalVariableToWritingFragment.end()) {
                    // If there is, take the conjunction of the entries and also of their versions without nondeterminism
                    // variables.
                    globalFragmentIt->second &= guardedWritingFragment;
                    illegalFragment |=
                        globalVariableToWritingFragmentWithoutNondeterminism[entry.first] && guardedWritingFragment.existsAbstract(nondetVariables);
                    globalVariableToWritingFragmentWithoutNondeterminism[entry.first] |= guardedWritingFragment.existsAbstract(nondetVariables);
                } else {
                    // If not, create the entry and also create a version of the entry that abstracts from the
                    // used nondeterminism variables.
                    globalVariableToWritingFragment[entry.first] = guardedWritingFragment;
                    globalVariableToWritingFragmentWithoutNondeterminism[entry.first] = guardedWritingFragment.existsAbstract(nondetVariables);
                }

                // Join all individual illegal fragments so we can see whether any of these elements lie in the
                // conjunction of all guards.
                illegalFragment |= action.illegalFragment;
            }

            // Now go through all fragments that are not written by the current action and join them with the
            // guard of the current action if the current action is not input enabled.
            for (auto& entry : globalVariableToWritingFragment) {
                if (action.variableToWritingFragment.find(entry.first) == action.variableToWritingFragment.end() && !action.isInputEnabled()) {
                    entry.second &= action.guard;
                }
            }

            if (!action.isInputEnabled()) {
                inputEnabledGuard &= action.guard;
            }
        }

        // If all actions were input-enabled, we need to constrain the transitions with the disjunction of all
        // guards to make sure there are not transitions resulting from input enabledness alone.
        if (allActionsInputEnabled) {
            inputEnabledGuard &= guardDisjunction.get();
            transitions *= guardDisjunction.get().template toAdd<ValueType>();
        }

        // Cut the union of the illegal fragments to the conjunction of the guards since only these states have
        // such a combined transition.
        illegalFragment &= inputEnabledGuard;

        storm::dd::Add<Type, ValueType> transientEdgeAssignmentWeights;
        if (hasTransientEdgeAssignments) {
            transientEdgeAssignmentWeights = inputEnabledGuard.template toAdd<ValueType>();
            if (exitRates) {
                transientEdgeAssignmentWeights *= exitRates.get();
            }

            for (auto& entry : transientEdgeAssignments) {
                entry.second *= transientEdgeAssignmentWeights;
            }
        }

        return ActionDd(inputEnabledGuard, transitions * nonSynchronizingIdentity, transientEdgeAssignments,
                        std::make_pair(lowestNondeterminismVariable, highestNondeterminismVariable), globalVariableToWritingFragment, illegalFragment);
    }

    ActionDd combineUnsynchronizedActions(ActionDd action1, ActionDd action2, storm::dd::Add<Type, ValueType> const& identity1,
                                          storm::dd::Add<Type, ValueType> const& identity2) {
        // First extend the action DDs by the other identities.
        STORM_LOG_TRACE("Multiplying identities to combine unsynchronized actions.");
        action1.transitions = action1.transitions * identity2;
        action2.transitions = action2.transitions * identity1;

        // Then combine the extended action DDs.
        return combineUnsynchronizedActions(action1, action2);
    }

    ActionDd combineUnsynchronizedActions(ActionDd action1, ActionDd action2) {
        return combineUnsynchronizedActions({action1, action2});
    }

    ActionDd combineUnsynchronizedActions(std::vector<ActionDd> actions) {
        STORM_LOG_TRACE("Combining unsynchronized actions.");

        if (this->model.getModelType() == storm::jani::ModelType::DTMC || this->model.getModelType() == storm::jani::ModelType::CTMC) {
            auto actionIt = actions.begin();
            ActionDd result(*actionIt);

            for (++actionIt; actionIt != actions.end(); ++actionIt) {
                result = ActionDd(result.guard || actionIt->guard, result.transitions + actionIt->transitions,
                                  joinTransientAssignmentMaps(result.transientEdgeAssignments, actionIt->transientEdgeAssignments),
                                  std::make_pair<uint64_t, uint64_t>(0, 0),
                                  joinVariableWritingFragmentMaps(result.variableToWritingFragment, actionIt->variableToWritingFragment),
                                  result.illegalFragment || actionIt->illegalFragment);
            }
            return result;
        } else if (this->model.getModelType() == storm::jani::ModelType::MDP || this->model.getModelType() == storm::jani::ModelType::LTS ||
                   this->model.getModelType() == storm::jani::ModelType::MA) {
            // Ensure that all actions start at the same local nondeterminism variable.
            uint_fast64_t lowestLocalNondeterminismVariable = actions.front().getLowestLocalNondeterminismVariable();
            uint_fast64_t highestLocalNondeterminismVariable = actions.front().getHighestLocalNondeterminismVariable();
            for (auto const& action : actions) {
                STORM_LOG_ASSERT(action.getLowestLocalNondeterminismVariable() == lowestLocalNondeterminismVariable,
                                 "Mismatching lowest nondeterminism variable indices.");
                highestLocalNondeterminismVariable = std::max(highestLocalNondeterminismVariable, action.getHighestLocalNondeterminismVariable());
            }

            // Bring all actions to the same number of variables that encode the nondeterminism.
            for (auto& action : actions) {
                storm::dd::Bdd<Type> nondeterminismEncodingBdd = this->variables.manager->getBddOne();
                for (uint_fast64_t i = action.getHighestLocalNondeterminismVariable(); i < highestLocalNondeterminismVariable; ++i) {
                    nondeterminismEncodingBdd &= this->variables.manager->getEncoding(this->variables.localNondeterminismVariables[i], 0);
                }
                storm::dd::Add<Type, ValueType> nondeterminismEncoding = nondeterminismEncodingBdd.template toAdd<ValueType>();

                action.transitions *= nondeterminismEncoding;

                for (auto& variableFragment : action.variableToWritingFragment) {
                    variableFragment.second &= nondeterminismEncodingBdd;
                }
                for (auto& transientAssignment : action.transientEdgeAssignments) {
                    transientAssignment.second *= nondeterminismEncoding;
                }
            }

            uint64_t numberOfLocalNondeterminismVariables = static_cast<uint64_t>(std::ceil(std::log2(actions.size())));
            storm::dd::Bdd<Type> guard = this->variables.manager->getBddZero();
            storm::dd::Add<Type, ValueType> transitions = this->variables.manager->template getAddZero<ValueType>();
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
            std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> variableToWritingFragment;
            storm::dd::Bdd<Type> illegalFragment = this->variables.manager->getBddZero();

            for (uint64_t actionIndex = 0; actionIndex < actions.size(); ++actionIndex) {
                ActionDd& action = actions[actionIndex];

                guard |= action.guard;

                storm::dd::Add<Type, ValueType> nondeterminismEncoding =
                    encodeIndex(actionIndex, highestLocalNondeterminismVariable, numberOfLocalNondeterminismVariables, this->variables);
                transitions += nondeterminismEncoding * action.transitions;

                joinTransientAssignmentMapsInPlace(transientEdgeAssignments, action.transientEdgeAssignments, nondeterminismEncoding);

                storm::dd::Bdd<Type> nondeterminismEncodingBdd = nondeterminismEncoding.toBdd();
                for (auto& entry : action.variableToWritingFragment) {
                    entry.second &= nondeterminismEncodingBdd;
                }
                addToVariableWritingFragmentMap(variableToWritingFragment, action.variableToWritingFragment);
                illegalFragment |= action.illegalFragment;
            }

            return ActionDd(guard, transitions, transientEdgeAssignments,
                            std::make_pair(lowestLocalNondeterminismVariable, highestLocalNondeterminismVariable + numberOfLocalNondeterminismVariables),
                            variableToWritingFragment, illegalFragment);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidStateException, "Illegal model type.");
        }
    }

    void performTransientAssignments(storm::jani::detail::ConstAssignments const& transientAssignments,
                                     std::function<void(storm::jani::Assignment const&)> const& callback) {
        auto transientVariableIt = this->transientVariables.begin();
        auto transientVariableIte = this->transientVariables.end();
        for (auto const& assignment : transientAssignments) {
            while (transientVariableIt != transientVariableIte && *transientVariableIt < assignment.getExpressionVariable()) {
                ++transientVariableIt;
            }
            if (transientVariableIt == transientVariableIte) {
                break;
            }
            if (*transientVariableIt == assignment.getExpressionVariable()) {
                callback(assignment);
                ++transientVariableIt;
            }
        }
    }

    EdgeDd buildEdgeDd(storm::jani::Automaton const& automaton, storm::jani::Edge const& edge) {
        STORM_LOG_TRACE("Translating guard " << edge.getGuard());

        // We keep the guard and a "ranged" version seperate, because building the destinations tends to be
        // slower when the full range is applied.
        storm::dd::Bdd<Type> guard = this->variables.rowExpressionAdapter->translateBooleanExpression(edge.getGuard());
        storm::dd::Bdd<Type> rangedGuard = guard && this->variables.automatonToRangeMap.at(automaton.getName()).toBdd();
        STORM_LOG_WARN_COND(!rangedGuard.isZero(), "The guard '" << edge.getGuard() << "' is unsatisfiable.");

        if (!rangedGuard.isZero()) {
            // Create the DDs representing the individual updates.
            std::vector<EdgeDestinationDd<Type, ValueType>> destinationDds;
            for (storm::jani::EdgeDestination const& destination : edge.getDestinations()) {
                destinationDds.push_back(buildEdgeDestinationDd(automaton, destination, guard, this->variables));

                STORM_LOG_WARN_COND(!destinationDds.back().transitions.isZero(), "Destination does not have any effect.");
            }

            // Now that we have built the destinations, we always take the full guard.
            storm::dd::Bdd<Type> sourceLocationBdd = this->variables.manager->getEncoding(
                this->variables.automatonToLocationDdVariableMap.at(automaton.getName()).first, edge.getSourceLocationIndex());
            guard = sourceLocationBdd && rangedGuard;

            // Start by gathering all variables that were written in at least one destination.
            std::set<storm::expressions::Variable> globalVariablesInSomeDestination;

            // If the edge is not labeled with the silent action, we have to analyze which portion of the global
            // variables was written by any of the updates and make all update results equal w.r.t. this set. If
            // the edge is labeled with the silent action, we can already multiply the identities of all global variables.
            if (edge.getActionIndex() != storm::jani::Model::SILENT_ACTION_INDEX) {
                for (auto const& edgeDestinationDd : destinationDds) {
                    globalVariablesInSomeDestination.insert(edgeDestinationDd.writtenGlobalVariables.begin(), edgeDestinationDd.writtenGlobalVariables.end());
                }
            } else {
                globalVariablesInSomeDestination = this->variables.allGlobalVariables;
            }

            // Then, multiply the missing identities.
            for (auto& destinationDd : destinationDds) {
                std::set<storm::expressions::Variable> missingIdentities;
                std::set_difference(globalVariablesInSomeDestination.begin(), globalVariablesInSomeDestination.end(),
                                    destinationDd.writtenGlobalVariables.begin(), destinationDd.writtenGlobalVariables.end(),
                                    std::inserter(missingIdentities, missingIdentities.begin()));

                for (auto const& variable : missingIdentities) {
                    STORM_LOG_TRACE("Multiplying identity for variable " << variable.getName() << " to destination DD.");
                    destinationDd.transitions *= this->variables.variableToIdentityMap.at(variable);
                }
            }

            // Now combine the destination DDs to the edge DD.
            storm::dd::Add<Type, ValueType> transitions = this->variables.manager->template getAddZero<ValueType>();
            for (auto const& destinationDd : destinationDds) {
                transitions += destinationDd.transitions;
            }

            // Add the source location and the guard.
            storm::dd::Add<Type, ValueType> guardAdd = guard.template toAdd<ValueType>();
            transitions *= guardAdd;

            // If we multiply the ranges of global variables, make sure everything stays within its bounds.
            if (!globalVariablesInSomeDestination.empty()) {
                transitions *= this->variables.globalVariableRanges;
            }

            // If the edge has a rate, we multiply it to the DD.
            bool isMarkovian = false;
            boost::optional<storm::dd::Add<Type, ValueType>> exitRates;
            if (edge.hasRate()) {
                exitRates = this->variables.rowExpressionAdapter->translateExpression(edge.getRate());
                transitions *= exitRates.get();
                isMarkovian = true;
            }

            // Finally treat the transient assignments.
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
            if (!this->transientVariables.empty()) {
                performTransientAssignments(edge.getAssignments().getTransientAssignments(), [this, &transientEdgeAssignments, &guardAdd,
                                                                                              &exitRates](storm::jani::Assignment const& assignment) {
                    auto newTransientEdgeAssignments = guardAdd * this->variables.rowExpressionAdapter->translateExpression(assignment.getAssignedExpression());
                    if (exitRates) {
                        newTransientEdgeAssignments *= exitRates.get();
                    }
                    transientEdgeAssignments[assignment.getExpressionVariable()] = newTransientEdgeAssignments;
                });
            }

            return EdgeDd(isMarkovian, guard, transitions, transientEdgeAssignments, globalVariablesInSomeDestination);
        } else {
            return EdgeDd(edge.hasRate(), rangedGuard, rangedGuard.template toAdd<ValueType>(),
                          std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>>(), std::set<storm::expressions::Variable>());
        }
    }

    EdgeDd combineMarkovianEdgesToSingleEdge(std::vector<EdgeDd> const& edgeDds) {
        storm::dd::Bdd<Type> guard = this->variables.manager->getBddZero();
        storm::dd::Add<Type, ValueType> transitions = this->variables.manager->template getAddZero<ValueType>();
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> variableToWritingFragment;

        bool overlappingGuards = false;
        for (auto const& edge : edgeDds) {
            STORM_LOG_THROW(edge.isMarkovian, storm::exceptions::WrongFormatException, "Can only combine Markovian edges.");

            if (!overlappingGuards) {
                overlappingGuards |= !(guard && edge.guard).isZero();
            }

            guard |= edge.guard;
            transitions += edge.transitions;
            variableToWritingFragment = joinVariableWritingFragmentMaps(variableToWritingFragment, edge.variableToWritingFragment);
            joinTransientAssignmentMapsInPlace(transientEdgeAssignments, edge.transientEdgeAssignments);
        }

        // Currently, we can only combine the transient edge assignments if there is no overlap of the guards of the edges.
        STORM_LOG_THROW(!overlappingGuards || transientEdgeAssignments.empty(), storm::exceptions::NotSupportedException,
                        "Cannot have transient edge assignments when combining Markovian edges with overlapping guards.");

        return EdgeDd(true, guard, transitions, transientEdgeAssignments, variableToWritingFragment);
    }

    ActionDd buildActionDdForActionInstantiation(storm::jani::Automaton const& automaton, ActionInstantiation const& instantiation) {
        // Translate the individual edges.
        std::vector<EdgeDd> edgeDds;
        for (auto const& edge : automaton.getEdges()) {
            if (edge.getActionIndex() == instantiation.actionIndex && edge.hasRate() == instantiation.isMarkovian()) {
                EdgeDd result = buildEdgeDd(automaton, edge);
                edgeDds.emplace_back(result);
            }
        }

        // Now combine the edges to a single action.
        uint64_t localNondeterminismVariableOffset = instantiation.localNondeterminismVariableOffset;
        if (!edgeDds.empty()) {
            storm::jani::ModelType modelType = this->model.getModelType();
            if (modelType == storm::jani::ModelType::DTMC) {
                return combineEdgesToActionDeterministic(edgeDds);
            } else if (modelType == storm::jani::ModelType::CTMC) {
                return combineEdgesToActionDeterministic(edgeDds);
            } else if (modelType == storm::jani::ModelType::MDP || modelType == storm::jani::ModelType::LTS) {
                return combineEdgesToActionNondeterministic(edgeDds, localNondeterminismVariableOffset);
            } else if (modelType == storm::jani::ModelType::MA) {
                if (instantiation.isMarkovian()) {
                    return combineEdgesToActionDeterministic(edgeDds);
                } else {
                    return combineEdgesToActionNondeterministic(edgeDds, localNondeterminismVariableOffset);
                }
            } else {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Cannot translate model of type " << modelType << ".");
            }
        } else {
            return ActionDd(this->variables.manager->getBddZero(), this->variables.manager->template getAddZero<ValueType>(), {},
                            std::make_pair<uint64_t, uint64_t>(0, 0), {}, this->variables.manager->getBddZero());
        }
    }

    void addToTransientAssignmentMap(std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>>& transientAssignments,
                                     std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& assignmentsToAdd) {
        for (auto const& entry : assignmentsToAdd) {
            auto it = transientAssignments.find(entry.first);
            if (it != transientAssignments.end()) {
                it->second += entry.second;
            } else {
                transientAssignments[entry.first] = entry.second;
            }
        }
    }

    void addToTransientAssignmentMap(std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>>& transientAssignments,
                                     storm::expressions::Variable const& variable, storm::dd::Add<Type, ValueType> const& assignmentToAdd) {
        auto it = transientAssignments.find(variable);
        if (it != transientAssignments.end()) {
            it->second += assignmentToAdd;
        } else {
            transientAssignments[variable] = assignmentToAdd;
        }
    }

    std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> joinTransientAssignmentMaps(
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientAssignments1,
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& transientAssignments2) {
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> result = transientAssignments1;

        for (auto const& entry : transientAssignments2) {
            auto resultIt = result.find(entry.first);
            if (resultIt != result.end()) {
                resultIt->second += entry.second;
            } else {
                result.emplace(entry);
            }
        }

        return result;
    }

    void joinTransientAssignmentMapsInPlace(std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>>& target,
                                            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> const& newTransientAssignments,
                                            boost::optional<storm::dd::Add<Type, ValueType>> const& factor = boost::none) {
        for (auto const& entry : newTransientAssignments) {
            auto targetIt = target.find(entry.first);
            if (targetIt != target.end()) {
                targetIt->second += factor ? factor.get() * entry.second : entry.second;
            } else {
                target[entry.first] = factor ? factor.get() * entry.second : entry.second;
            }
        }
    }

    ActionDd combineEdgesToActionDeterministic(std::vector<EdgeDd> const& edgeDds) {
        storm::dd::Bdd<Type> allGuards = this->variables.manager->getBddZero();
        storm::dd::Add<Type, ValueType> allTransitions = this->variables.manager->template getAddZero<ValueType>();
        storm::dd::Bdd<Type> temporary;

        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> globalVariableToWritingFragment;
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
        bool overlappingGuards = false;
        for (auto const& edgeDd : edgeDds) {
            STORM_LOG_THROW(
                (this->model.getModelType() == storm::jani::ModelType::CTMC || this->model.getModelType() == storm::jani::ModelType::MA) == edgeDd.isMarkovian,
                storm::exceptions::WrongFormatException, "Unexpected edge type.");

            // Check for overlapping guards.
            overlappingGuards = !(edgeDd.guard && allGuards).isZero();

            // Issue a warning if there are overlapping guards in a DTMC.
            STORM_LOG_WARN_COND(
                !overlappingGuards || this->model.getModelType() == storm::jani::ModelType::CTMC || this->model.getModelType() == storm::jani::ModelType::MA,
                "Guard of an edge in a DTMC overlaps with previous guards.");

            // Add the elements of the current edge to the global ones.
            allGuards |= edgeDd.guard;
            allTransitions += edgeDd.transitions;

            // Add the transient variable assignments to the resulting one. This transformation is illegal for
            // CTMCs for which there is some overlap in edges that have some transient assignment (this needs to
            // be checked later).
            addToTransientAssignmentMap(transientEdgeAssignments, edgeDd.transientEdgeAssignments);

            // Keep track of the fragment that is writing global variables.
            globalVariableToWritingFragment = joinVariableWritingFragmentMaps(globalVariableToWritingFragment, edgeDd.variableToWritingFragment);
        }

        STORM_LOG_THROW(this->model.getModelType() == storm::jani::ModelType::DTMC || !overlappingGuards || transientEdgeAssignments.empty(),
                        storm::exceptions::NotSupportedException,
                        "Cannot have transient edge assignments when combining Markovian edges with overlapping guards.");

        return ActionDd(allGuards, allTransitions, transientEdgeAssignments, std::make_pair<uint64_t, uint64_t>(0, 0), globalVariableToWritingFragment,
                        this->variables.manager->getBddZero());
    }

    void addToVariableWritingFragmentMap(std::map<storm::expressions::Variable, storm::dd::Bdd<Type>>& globalVariableToWritingFragment,
                                         storm::expressions::Variable const& variable, storm::dd::Bdd<Type> const& partToAdd) const {
        auto it = globalVariableToWritingFragment.find(variable);
        if (it != globalVariableToWritingFragment.end()) {
            it->second |= partToAdd;
        } else {
            globalVariableToWritingFragment.emplace(variable, partToAdd);
        }
    }

    void addToVariableWritingFragmentMap(std::map<storm::expressions::Variable, storm::dd::Bdd<Type>>& globalVariableToWritingFragment,
                                         std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> const& partToAdd) const {
        for (auto const& entry : partToAdd) {
            addToVariableWritingFragmentMap(globalVariableToWritingFragment, entry.first, entry.second);
        }
    }

    std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> joinVariableWritingFragmentMaps(
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> const& globalVariableToWritingFragment1,
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> const& globalVariableToWritingFragment2) {
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> result = globalVariableToWritingFragment1;

        for (auto const& entry : globalVariableToWritingFragment2) {
            auto resultIt = result.find(entry.first);
            if (resultIt != result.end()) {
                resultIt->second |= entry.second;
            } else {
                result[entry.first] = entry.second;
            }
        }

        return result;
    }

    ActionDd combineEdgesBySummation(storm::dd::Bdd<Type> const& guard, std::vector<EdgeDd> const& edges) {
        storm::dd::Add<Type, ValueType> transitions = this->variables.manager->template getAddZero<ValueType>();
        std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> globalVariableToWritingFragment;
        std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;

        for (auto const& edge : edges) {
            transitions += edge.transitions;
            for (auto const& assignment : edge.transientEdgeAssignments) {
                addToTransientAssignmentMap(transientEdgeAssignments, assignment.first, assignment.second);
            }
            for (auto const& variableFragment : edge.variableToWritingFragment) {
                addToVariableWritingFragmentMap(globalVariableToWritingFragment, variableFragment.first, variableFragment.second);
            }
        }

        return ActionDd(guard, transitions, transientEdgeAssignments, std::make_pair<uint64_t, uint64_t>(0, 0), globalVariableToWritingFragment,
                        this->variables.manager->getBddZero());
    }

    ActionDd combineEdgesToActionNondeterministic(std::vector<EdgeDd> const& edges, uint64_t localNondeterminismVariableOffset) {
        // Sum all guards, so we can read off the maximal number of nondeterministic choices in any given state.
        storm::dd::Bdd<Type> allGuards = this->variables.manager->getBddZero();
        storm::dd::Add<Type, uint_fast64_t> sumOfGuards = this->variables.manager->template getAddZero<uint_fast64_t>();
        for (auto const& edge : edges) {
            STORM_LOG_ASSERT(!edge.isMarkovian, "Unexpected Markovian edge.");
            sumOfGuards += edge.guard.template toAdd<uint_fast64_t>();
            allGuards |= edge.guard;
        }
        uint_fast64_t maxChoices = sumOfGuards.getMax();
        STORM_LOG_TRACE("Found " << maxChoices << " non-Markovian local choices.");

        // Depending on the maximal number of nondeterminstic choices, we need to use some variables to encode the nondeterminism.
        if (maxChoices <= 1) {
            return combineEdgesBySummation(allGuards, edges);
        } else {
            // Calculate number of required variables to encode the nondeterminism.
            uint_fast64_t numberOfBinaryVariables = static_cast<uint_fast64_t>(std::ceil(std::log2(maxChoices)));

            storm::dd::Add<Type, ValueType> allEdges = this->variables.manager->template getAddZero<ValueType>();
            std::map<storm::expressions::Variable, storm::dd::Bdd<Type>> globalVariableToWritingFragment;
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientAssignments;

            storm::dd::Bdd<Type> equalsNumberOfChoicesDd;
            std::vector<storm::dd::Add<Type, ValueType>> choiceDds(maxChoices, this->variables.manager->template getAddZero<ValueType>());
            std::vector<storm::dd::Bdd<Type>> remainingDds(maxChoices, this->variables.manager->getBddZero());
            std::vector<std::pair<storm::dd::Bdd<Type>, storm::dd::Add<Type, ValueType>>> indicesEncodedWithLocalNondeterminismVariables;
            for (uint64_t j = 0; j < maxChoices; ++j) {
                storm::dd::Add<Type, ValueType> indexEncoding = encodeIndex(j, localNondeterminismVariableOffset, numberOfBinaryVariables, this->variables);
                indicesEncodedWithLocalNondeterminismVariables.push_back(std::make_pair(indexEncoding.toBdd(), indexEncoding));
            }

            for (uint_fast64_t currentChoices = 1; currentChoices <= maxChoices; ++currentChoices) {
                // Determine the set of states with exactly currentChoices choices.
                equalsNumberOfChoicesDd = sumOfGuards.equals(this->variables.manager->getConstant(currentChoices));

                // If there is no such state, continue with the next possible number of choices.
                if (equalsNumberOfChoicesDd.isZero()) {
                    continue;
                }

                // Reset the previously used intermediate storage.
                for (uint_fast64_t j = 0; j < currentChoices; ++j) {
                    choiceDds[j] = this->variables.manager->template getAddZero<ValueType>();
                    remainingDds[j] = equalsNumberOfChoicesDd;
                }

                for (std::size_t j = 0; j < edges.size(); ++j) {
                    EdgeDd const& currentEdge = edges[j];

                    // Check if edge guard overlaps with equalsNumberOfChoicesDd. That is, there are states with exactly currentChoices
                    // choices such that one outgoing choice is given by the j-th edge.
                    storm::dd::Bdd<Type> guardChoicesIntersection = currentEdge.guard && equalsNumberOfChoicesDd;

                    // If there is no such state, continue with the next command.
                    if (guardChoicesIntersection.isZero()) {
                        continue;
                    }

                    // Split the currentChoices nondeterministic choices.
                    for (uint_fast64_t k = 0; k < currentChoices; ++k) {
                        // Calculate the overlapping part of command guard and the remaining DD.
                        storm::dd::Bdd<Type> remainingGuardChoicesIntersection = guardChoicesIntersection && remainingDds[k];

                        // Check if we can add some overlapping parts to the current index.
                        if (!remainingGuardChoicesIntersection.isZero()) {
                            // Remove overlapping parts from the remaining DD.
                            remainingDds[k] = remainingDds[k] && !remainingGuardChoicesIntersection;

                            // Combine the overlapping part of the guard with command updates and add it to the resulting DD.
                            choiceDds[k] += remainingGuardChoicesIntersection.template toAdd<ValueType>() * currentEdge.transitions;

                            // Keep track of the fragment of transient assignments.
                            for (auto const& transientAssignment : currentEdge.transientEdgeAssignments) {
                                addToTransientAssignmentMap(transientAssignments, transientAssignment.first,
                                                            remainingGuardChoicesIntersection.template toAdd<ValueType>() * transientAssignment.second *
                                                                indicesEncodedWithLocalNondeterminismVariables[k].second);
                            }

                            // Keep track of the written global variables of the fragment.
                            for (auto const& variableFragment : currentEdge.variableToWritingFragment) {
                                addToVariableWritingFragmentMap(
                                    globalVariableToWritingFragment, variableFragment.first,
                                    remainingGuardChoicesIntersection && variableFragment.second && indicesEncodedWithLocalNondeterminismVariables[k].first);
                            }
                        }

                        // Remove overlapping parts from the command guard DD
                        guardChoicesIntersection = guardChoicesIntersection && !remainingGuardChoicesIntersection;

                        // If the guard DD has become equivalent to false, we can stop here.
                        if (guardChoicesIntersection.isZero()) {
                            break;
                        }
                    }
                }

                // Add the meta variables that encode the nondeterminisim to the different choices.
                for (uint_fast64_t j = 0; j < currentChoices; ++j) {
                    allEdges += indicesEncodedWithLocalNondeterminismVariables[j].second * choiceDds[j];
                }

                // Delete currentChoices out of overlapping DD
                sumOfGuards = sumOfGuards * (!equalsNumberOfChoicesDd).template toAdd<uint_fast64_t>();
            }

            return ActionDd(allGuards, allEdges, transientAssignments,
                            std::make_pair(localNondeterminismVariableOffset, localNondeterminismVariableOffset + numberOfBinaryVariables),
                            globalVariableToWritingFragment, this->variables.manager->getBddZero());
        }
    }

    AutomatonDd buildAutomatonDd(std::string const& automatonName, ActionInstantiations const& actionInstantiations,
                                 std::set<uint64_t> const& inputEnabledActionIndices, bool isTopLevelAutomaton) {
        STORM_LOG_TRACE("Building DD for automaton '" << automatonName << "'.");
        AutomatonDd result(this->variables.automatonToIdentityMap.at(automatonName));

        // Disjunction of all guards of non-markovian actions (only required for maximum progress assumption).
        storm::dd::Bdd<Type> nonMarkovianActionGuards = this->variables.manager->getBddZero();

        storm::jani::Automaton const& automaton = this->model.getAutomaton(automatonName);
        for (auto const& actionInstantiation : actionInstantiations) {
            uint64_t actionIndex = actionInstantiation.first;
            if (!automaton.hasEdgeLabeledWithActionIndex(actionIndex)) {
                continue;
            }
            bool inputEnabled = false;
            if (inputEnabledActionIndices.find(actionIndex) != inputEnabledActionIndices.end()) {
                inputEnabled = true;
            }
            for (auto const& instantiation : actionInstantiation.second) {
                STORM_LOG_TRACE("Building " << (instantiation.isMarkovian() ? "(Markovian) " : "")
                                            << (actionInformation.getActionName(actionIndex).empty() ? "silent " : "") << "action "
                                            << (actionInformation.getActionName(actionIndex).empty() ? "" : actionInformation.getActionName(actionIndex) + " ")
                                            << "from offset " << instantiation.localNondeterminismVariableOffset << ".");
                ActionDd actionDd = buildActionDdForActionInstantiation(automaton, instantiation);
                if (inputEnabled) {
                    actionDd.setIsInputEnabled();
                }
                if (applyMaximumProgress && isTopLevelAutomaton && !instantiation.isMarkovian()) {
                    nonMarkovianActionGuards |= actionDd.guard;
                }
                STORM_LOG_TRACE("Used local nondeterminism variables are " << actionDd.getLowestLocalNondeterminismVariable() << " to "
                                                                           << actionDd.getHighestLocalNondeterminismVariable() << ".");
                result.actions[ActionIdentification(actionIndex, instantiation.synchronizationVectorIndex, instantiation.isMarkovian())] = actionDd;
                result.extendLocalNondeterminismVariables(actionDd.getLocalNondeterminismVariables());
            }
        }

        if (applyMaximumProgress && isTopLevelAutomaton) {
            ActionIdentification silentMarkovianActionIdentification(storm::jani::Model::SILENT_ACTION_INDEX, true);
            result.actions[silentMarkovianActionIdentification].conjunctGuardWith(!nonMarkovianActionGuards);
        }

        for (uint64_t locationIndex = 0; locationIndex < automaton.getNumberOfLocations(); ++locationIndex) {
            auto const& location = automaton.getLocation(locationIndex);
            performTransientAssignments(
                location.getAssignments().getTransientAssignments(), [this, &automatonName, locationIndex, &result](storm::jani::Assignment const& assignment) {
                    storm::dd::Add<Type, ValueType> assignedValues =
                        this->variables.manager->getEncoding(this->variables.automatonToLocationDdVariableMap.at(automatonName).first, locationIndex)
                            .template toAdd<ValueType>() *
                        this->variables.rowExpressionAdapter->translateExpression(assignment.getAssignedExpression());

                    auto it = result.transientLocationAssignments.find(assignment.getExpressionVariable());
                    if (it != result.transientLocationAssignments.end()) {
                        it->second += assignedValues;
                    } else {
                        result.transientLocationAssignments[assignment.getExpressionVariable()] = assignedValues;
                    }
                });
        }

        return result;
    }

    void addMissingGlobalVariableIdentities(ActionDd& action) {
        // Build a DD that we can multiply to the transitions and adds all missing global variable identities that way.
        storm::dd::Add<Type, ValueType> missingIdentities = this->variables.manager->template getAddOne<ValueType>();

        for (auto const& variable : this->variables.allGlobalVariables) {
            auto it = action.variableToWritingFragment.find(variable);
            if (it != action.variableToWritingFragment.end()) {
                missingIdentities *=
                    (it->second).ite(this->variables.manager->template getAddOne<ValueType>(), this->variables.variableToIdentityMap.at(variable));
            } else {
                missingIdentities *= this->variables.variableToIdentityMap.at(variable);
            }
        }

        action.transitions *= missingIdentities;
    }

    ComposerResult<Type, ValueType> buildSystemFromAutomaton(AutomatonDd& automaton) {
        STORM_LOG_TRACE("Building system from final automaton.");

        auto modelType = this->model.getModelType();

        // If the model is an MDP, we need to encode the nondeterminism using additional variables.
        if (modelType == storm::jani::ModelType::MDP || modelType == storm::jani::ModelType::MA || modelType == storm::jani::ModelType::LTS) {
            storm::dd::Add<Type, ValueType> result = this->variables.manager->template getAddZero<ValueType>();
            storm::dd::Bdd<Type> illegalFragment = this->variables.manager->getBddZero();

            // First, determine the highest number of nondeterminism variables that is used in any action and make
            // all actions use the same amout of nondeterminism variables.
            uint64_t numberOfUsedNondeterminismVariables = automaton.getHighestLocalNondeterminismVariable();
            STORM_LOG_TRACE("Building system from composed automaton; number of used nondeterminism variables is " << numberOfUsedNondeterminismVariables
                                                                                                                   << ".");

            // Add missing global variable identities, action and nondeterminism encodings.
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
            std::unordered_set<ActionIdentification, ActionIdentificationHash> containedActions;
            for (auto& action : automaton.actions) {
                STORM_LOG_TRACE("Treating action with index " << action.first.actionIndex << (action.first.isMarkovian() ? " (Markovian)" : "") << ".");

                uint64_t actionIndex = action.first.actionIndex;
                bool markovian = action.first.isMarkovian();
                ActionIdentification identificationWithoutSynchVector(actionIndex, markovian);

                STORM_LOG_THROW(containedActions.find(identificationWithoutSynchVector) == containedActions.end(), storm::exceptions::WrongFormatException,
                                "Duplicate action " << actionInformation.getActionName(actionIndex));
                containedActions.insert(identificationWithoutSynchVector);
                illegalFragment |= action.second.illegalFragment;
                addMissingGlobalVariableIdentities(action.second);
                storm::dd::Add<Type, ValueType> actionEncoding =
                    encodeAction(actionIndex != storm::jani::Model::SILENT_ACTION_INDEX ? boost::make_optional(actionIndex) : boost::none,
                                 this->model.getModelType() == storm::jani::ModelType::MA ? boost::make_optional(markovian) : boost::none, this->variables);

                storm::dd::Add<Type, ValueType> missingNondeterminismEncoding =
                    encodeIndex(0, action.second.getHighestLocalNondeterminismVariable(),
                                numberOfUsedNondeterminismVariables - action.second.getHighestLocalNondeterminismVariable(), this->variables);
                storm::dd::Add<Type, ValueType> extendedTransitions = actionEncoding * missingNondeterminismEncoding * action.second.transitions;
                for (auto const& transientAssignment : action.second.transientEdgeAssignments) {
                    addToTransientAssignmentMap(transientEdgeAssignments, transientAssignment.first,
                                                actionEncoding * missingNondeterminismEncoding * transientAssignment.second);
                }

                result += extendedTransitions;
            }

            return ComposerResult<Type, ValueType>(result, automaton.transientLocationAssignments, transientEdgeAssignments, illegalFragment,
                                                   numberOfUsedNondeterminismVariables);
        } else if (modelType == storm::jani::ModelType::DTMC || modelType == storm::jani::ModelType::CTMC) {
            // Simply add all actions, but make sure to include the missing global variable identities.

            storm::dd::Add<Type, ValueType> result = this->variables.manager->template getAddZero<ValueType>();
            storm::dd::Bdd<Type> illegalFragment = this->variables.manager->getBddZero();
            std::map<storm::expressions::Variable, storm::dd::Add<Type, ValueType>> transientEdgeAssignments;
            std::unordered_set<uint64_t> actionIndices;
            for (auto& action : automaton.actions) {
                STORM_LOG_THROW(actionIndices.find(action.first.actionIndex) == actionIndices.end(), storm::exceptions::WrongFormatException,
                                "Duplication action " << actionInformation.getActionName(action.first.actionIndex));
                actionIndices.insert(action.first.actionIndex);
                illegalFragment |= action.second.illegalFragment;
                addMissingGlobalVariableIdentities(action.second);
                addToTransientAssignmentMap(transientEdgeAssignments, action.second.transientEdgeAssignments);
                result += action.second.transitions;
            }

            return ComposerResult<Type, ValueType>(result, automaton.transientLocationAssignments, transientEdgeAssignments, illegalFragment, 0);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Model type '" << this->model.getModelType() << "' not supported.");
        }
    }
};

template<storm::dd::DdType Type, typename ValueType>
struct ModelComponents {
    storm::dd::Bdd<Type> reachableStates;
    storm::dd::Bdd<Type> initialStates;
    storm::dd::Bdd<Type> deadlockStates;
    storm::dd::Add<Type, ValueType> transitionMatrix;
    std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> rewardModels;
    std::map<std::string, storm::expressions::Expression> labelToExpressionMap;
};

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> createModel(storm::jani::ModelType const& modelType,
                                                                             CompositionVariables<Type, ValueType> const& variables,
                                                                             ModelComponents<Type, ValueType> const& modelComponents) {
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> result;
    if (modelType == storm::jani::ModelType::DTMC) {
        result = std::make_shared<storm::models::symbolic::Dtmc<Type, ValueType>>(
            variables.manager, modelComponents.reachableStates, modelComponents.initialStates, modelComponents.deadlockStates, modelComponents.transitionMatrix,
            variables.rowMetaVariables, variables.rowExpressionAdapter, variables.columnMetaVariables, variables.rowColumnMetaVariablePairs,
            modelComponents.labelToExpressionMap, modelComponents.rewardModels);
    } else if (modelType == storm::jani::ModelType::CTMC) {
        result = std::make_shared<storm::models::symbolic::Ctmc<Type, ValueType>>(
            variables.manager, modelComponents.reachableStates, modelComponents.initialStates, modelComponents.deadlockStates, modelComponents.transitionMatrix,
            variables.rowMetaVariables, variables.rowExpressionAdapter, variables.columnMetaVariables, variables.rowColumnMetaVariablePairs,
            modelComponents.labelToExpressionMap, modelComponents.rewardModels);
    } else if (modelType == storm::jani::ModelType::MDP || modelType == storm::jani::ModelType::LTS) {
        result = std::make_shared<storm::models::symbolic::Mdp<Type, ValueType>>(
            variables.manager, modelComponents.reachableStates, modelComponents.initialStates, modelComponents.deadlockStates, modelComponents.transitionMatrix,
            variables.rowMetaVariables, variables.rowExpressionAdapter, variables.columnMetaVariables, variables.rowColumnMetaVariablePairs,
            variables.allNondeterminismVariables, modelComponents.labelToExpressionMap, modelComponents.rewardModels);
    } else if (modelType == storm::jani::ModelType::MA) {
        result = std::make_shared<storm::models::symbolic::MarkovAutomaton<Type, ValueType>>(
            variables.manager, !variables.probabilisticMarker, modelComponents.reachableStates, modelComponents.initialStates, modelComponents.deadlockStates,
            modelComponents.transitionMatrix, variables.rowMetaVariables, variables.rowExpressionAdapter, variables.columnMetaVariables,
            variables.rowColumnMetaVariablePairs, variables.allNondeterminismVariables, modelComponents.labelToExpressionMap, modelComponents.rewardModels);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Model type '" << modelType << "' not supported.");
    }

    if (std::is_same<ValueType, storm::RationalFunction>::value) {
        result->addParameters(variables.parameters);
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
void postprocessVariables(storm::jani::ModelType const& modelType, ComposerResult<Type, ValueType>& system, CompositionVariables<Type, ValueType>& variables) {
    // Add all action/row/column variables to the DD. If we omitted multiplying edges in the construction, this will
    // introduce the variables so they can later be abstracted without raising an error.
    system.transitions.addMetaVariables(variables.rowMetaVariables);
    system.transitions.addMetaVariables(variables.columnMetaVariables);

    // If the model is an MDP, we also add all action variables.
    if (modelType == storm::jani::ModelType::MDP || modelType == storm::jani::ModelType::LTS) {
        for (auto const& actionVariablePair : variables.actionVariablesMap) {
            system.transitions.addMetaVariable(actionVariablePair.second);
        }
    }

    // Get rid of the local nondeterminism variables that were not used.
    for (uint64_t index = system.numberOfNondeterminismVariables; index < variables.localNondeterminismVariables.size(); ++index) {
        variables.allNondeterminismVariables.erase(variables.localNondeterminismVariables[index]);
    }
    variables.localNondeterminismVariables.resize(system.numberOfNondeterminismVariables);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> postprocessSystem(storm::jani::Model const& model, ComposerResult<Type, ValueType>& system,
                                       CompositionVariables<Type, ValueType> const& variables,
                                       typename DdJaniModelBuilder<Type, ValueType>::Options const& options,
                                       std::map<std::string, storm::expressions::Expression> const& labelsToExpressionMap) {
    // For DTMCs, we normalize each row to 1 (to account for non-determinism).
    if (model.getModelType() == storm::jani::ModelType::DTMC) {
        storm::dd::Add<Type, ValueType> stateToNumberOfChoices = system.transitions.sumAbstract(variables.columnMetaVariables);
        system.transitions = system.transitions / stateToNumberOfChoices;

        // Scale all state-action rewards.
        for (auto& entry : system.transientEdgeAssignments) {
            entry.second = entry.second / stateToNumberOfChoices;
        }
    }

    // If we were asked to treat some states as terminal states, we cut away their transitions now.
    storm::dd::Bdd<Type> terminalStatesBdd = variables.manager->getBddZero();
    if (!options.terminalStates.empty()) {
        storm::expressions::Expression terminalExpression = options.terminalStates.asExpression([&model, &labelsToExpressionMap](std::string const& labelName) {
            auto exprIt = labelsToExpressionMap.find(labelName);
            if (exprIt != labelsToExpressionMap.end()) {
                return exprIt->second;
            } else {
                STORM_LOG_THROW(labelName == "init" || labelName == "deadlock", storm::exceptions::InvalidArgumentException,
                                "Terminal states refer to illegal label '" << labelName << "'.");
                // If the label name is "init" we can abort 'exploration' directly at the initial state. If it is deadlock, we do not have to abort.
                return model.getExpressionManager().boolean(labelName == "init");
            }
        });
        terminalExpression = terminalExpression.substitute(model.getConstantsSubstitution());
        terminalStatesBdd = variables.rowExpressionAdapter->translateExpression(terminalExpression).toBdd();
        system.transitions *= (!terminalStatesBdd).template toAdd<ValueType>();
    }
    return terminalStatesBdd;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> computeInitialStates(storm::jani::Model const& model, CompositionVariables<Type, ValueType> const& variables) {
    std::vector<std::reference_wrapper<storm::jani::Automaton const>> allAutomata;
    for (auto const& automaton : model.getAutomata()) {
        allAutomata.push_back(automaton);
    }
    storm::dd::Bdd<Type> initialStates = variables.rowExpressionAdapter->translateExpression(model.getInitialStatesExpression(allAutomata)).toBdd();
    for (auto const& automaton : model.getAutomata()) {
        storm::dd::Bdd<Type> initialLocationIndices = variables.manager->getBddZero();
        for (auto const& locationIndex : automaton.getInitialLocationIndices()) {
            initialLocationIndices |= variables.manager->getEncoding(variables.automatonToLocationDdVariableMap.at(automaton.getName()).first, locationIndex);
        }
        initialStates &= initialLocationIndices;
    }
    for (auto const& metaVariable : variables.rowMetaVariables) {
        initialStates &= variables.variableToRangeMap.at(metaVariable);
    }
    return initialStates;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> fixDeadlocks(storm::jani::ModelType const& modelType, storm::dd::Add<Type, ValueType>& transitionMatrix,
                                  storm::dd::Bdd<Type> const& transitionMatrixBdd, storm::dd::Bdd<Type> const& reachableStates,
                                  CompositionVariables<Type, ValueType> const& variables) {
    // Detect deadlocks and 1) fix them if requested 2) throw an error otherwise.
    storm::dd::Bdd<Type> statesWithTransition = transitionMatrixBdd.existsAbstract(variables.columnMetaVariables);
    storm::dd::Bdd<Type> deadlockStates = reachableStates && !statesWithTransition;

    if (!deadlockStates.isZero()) {
        // If we need to fix deadlocks, we do so now.
        if (!storm::settings::getModule<storm::settings::modules::BuildSettings>().isDontFixDeadlocksSet()) {
            STORM_LOG_INFO("Fixing deadlocks in " << deadlockStates.getNonZeroCount() << " states. The first three of these states are: ");

            storm::dd::Add<Type, ValueType> deadlockStatesAdd = deadlockStates.template toAdd<ValueType>();
            uint_fast64_t count = 0;
            for (auto it = deadlockStatesAdd.begin(), ite = deadlockStatesAdd.end(); it != ite && count < 3; ++it, ++count) {
                STORM_LOG_INFO((*it).first.toPrettyString(variables.rowMetaVariables) << '\n');
            }

            // Create a global identity DD.
            storm::dd::Add<Type, ValueType> globalIdentity = variables.manager->template getAddOne<ValueType>();
            for (auto const& identity : variables.automatonToIdentityMap) {
                globalIdentity *= identity.second;
            }
            for (auto const& variable : variables.allGlobalVariables) {
                globalIdentity *= variables.variableToIdentityMap.at(variable);
            }

            if (modelType == storm::jani::ModelType::DTMC || modelType == storm::jani::ModelType::CTMC) {
                // For DTMCs, we can simply add the identity of the global module for all deadlock states.
                transitionMatrix += deadlockStatesAdd * globalIdentity;
            } else if (modelType == storm::jani::ModelType::MDP || modelType == storm::jani::ModelType::LTS || modelType == storm::jani::ModelType::MA) {
                // For nondeterministic models, however, we need to select an action associated with the self-loop, if we do not
                // want to attach a lot of self-loops to the deadlock states.
                storm::dd::Add<Type, ValueType> action =
                    encodeAction(boost::none, modelType == storm::jani::ModelType::MA ? boost::make_optional(true) : boost::none, variables);

                for (auto const& variable : variables.localNondeterminismVariables) {
                    action *= variables.manager->getEncoding(variable, 0).template toAdd<ValueType>();
                }

                transitionMatrix += deadlockStatesAdd * globalIdentity * action;
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "The model contains " << deadlockStates.getNonZeroCount()
                                                  << " deadlock states. Please unset the option to not fix deadlocks, if you want to fix them automatically.");
        }
    }
    return deadlockStates;
}

template<storm::dd::DdType Type, typename ValueType>
std::vector<storm::expressions::Variable> selectRewardVariables(storm::jani::Model const& model,
                                                                typename DdJaniModelBuilder<Type, ValueType>::Options const& options) {
    std::vector<storm::expressions::Variable> rewardVariables;
    if (options.isBuildAllRewardModelsSet()) {
        for (auto const& rewExpr : model.getAllRewardModelExpressions()) {
            STORM_LOG_THROW(!model.isNonTrivialRewardModelExpression(rewExpr.first), storm::exceptions::NotSupportedException,
                            "The DD-builder can not build the non-trivial reward expression '" << rewExpr.second << "'.");
            rewardVariables.push_back(rewExpr.second.getBaseExpression().asVariableExpression().getVariable());
        }
    } else {
        for (auto const& rewardModelName : options.getRewardModelNames()) {
            STORM_LOG_THROW(!model.isNonTrivialRewardModelExpression(rewardModelName), storm::exceptions::NotSupportedException,
                            "The DD-builder can not build the non-trivial reward expression '" << rewardModelName << "'.");
            auto const& rewExpr = model.getRewardModelExpression(rewardModelName);
            rewardVariables.push_back(rewExpr.getBaseExpression().asVariableExpression().getVariable());
        }
    }
    // Sort the reward variables to match the order in the ordered assignments
    std::sort(rewardVariables.begin(), rewardVariables.end());

    return rewardVariables;
}

template<storm::dd::DdType Type, typename ValueType>
std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> buildRewardModels(
    storm::dd::Add<Type, ValueType> const& reachableStates, storm::dd::Add<Type, ValueType> const& transitionMatrix, storm::jani::ModelType const& modelType,
    CompositionVariables<Type, ValueType> const& variables, ComposerResult<Type, ValueType> const& system,
    std::vector<storm::expressions::Variable> const& rewardVariables) {
    std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> result;

    // For CTMCs, we need to scale the state-action rewards with the total exit rates.
    boost::optional<storm::dd::Add<Type, ValueType>> exitRates;
    if (modelType == storm::jani::ModelType::CTMC || modelType == storm::jani::ModelType::DTMC) {
        exitRates = transitionMatrix.sumAbstract(variables.columnMetaVariables);
    }

    for (auto const& variable : rewardVariables) {
        boost::optional<storm::dd::Add<Type, ValueType>> stateRewards = boost::none;
        boost::optional<storm::dd::Add<Type, ValueType>> stateActionRewards = boost::none;
        boost::optional<storm::dd::Add<Type, ValueType>> transitionRewards = boost::none;

        auto it = system.transientLocationAssignments.find(variable);
        if (it != system.transientLocationAssignments.end()) {
            stateRewards = reachableStates * it->second;
        }

        it = system.transientEdgeAssignments.find(variable);
        if (it != system.transientEdgeAssignments.end()) {
            stateActionRewards = reachableStates * it->second;
            if (exitRates) {
                stateActionRewards.get() = stateActionRewards.get() / exitRates.get();
            }
        }

        result.emplace(variable.getName(), storm::models::symbolic::StandardRewardModel<Type, ValueType>(stateRewards, stateActionRewards, transitionRewards));
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::map<std::string, storm::expressions::Expression> buildLabelExpressions(storm::jani::Model const& model,
                                                                            CompositionVariables<Type, ValueType> const& variables,
                                                                            typename DdJaniModelBuilder<Type, ValueType>::Options const& options) {
    std::map<std::string, storm::expressions::Expression> result;

    // Create a list of composed automata to restrict the labels to locations of these automata.
    std::vector<std::reference_wrapper<storm::jani::Automaton const>> composedAutomata;
    for (auto const& entry : variables.automatonToIdentityMap) {
        composedAutomata.emplace_back(model.getAutomaton(entry.first));
    }

    for (auto const& variable : model.getGlobalVariables().getTransientVariables()) {
        if (variable.getType().isBasicType() && variable.getType().asBasicType().isBooleanType()) {
            if (options.buildAllLabels || options.labelNames.find(variable.getName()) != options.labelNames.end()) {
                result[variable.getName()] = model.getLabelExpression(variable, composedAutomata);
            }
        }
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> buildInternal(storm::jani::Model const& model,
                                                                               typename DdJaniModelBuilder<Type, ValueType>::Options const& options,
                                                                               std::shared_ptr<storm::dd::DdManager<Type>> const& manager) {
    // Determine the actions that will appear in the parallel composition.
    storm::jani::CompositionInformationVisitor visitor(model, model.getSystemComposition());
    storm::jani::CompositionInformation actionInformation = visitor.getInformation();

    // Create all necessary variables.
    CompositionVariableCreator<Type, ValueType> variableCreator(model, actionInformation);
    CompositionVariables<Type, ValueType> variables = variableCreator.create(manager);

    // Determine which transient assignments need to be considered in the building process.
    std::vector<storm::expressions::Variable> rewardVariables = selectRewardVariables<Type, ValueType>(model, options);

    // Create a builder to compose and build the model.
    bool applyMaximumProgress = options.applyMaximumProgressAssumption && model.getModelType() == storm::jani::ModelType::MA;
    CombinedEdgesSystemComposer<Type, ValueType> composer(model, actionInformation, variables, rewardVariables, applyMaximumProgress);
    ComposerResult<Type, ValueType> system = composer.compose();

    // Postprocess the variables in place.
    postprocessVariables(model.getModelType(), system, variables);

    // Build the label to expressions mapping.
    auto labelsToExpressionMap = buildLabelExpressions(model, variables, options);

    // Postprocess the system in place and get the states that were terminal (i.e. whose transitions were cut off).
    storm::dd::Bdd<Type> terminalStates = postprocessSystem(model, system, variables, options, labelsToExpressionMap);

    // Start creating the model components.
    ModelComponents<Type, ValueType> modelComponents;

    // Set the label expressions
    modelComponents.labelToExpressionMap = std::move(labelsToExpressionMap);

    // Build initial states.
    modelComponents.initialStates = computeInitialStates(model, variables);

    // Perform reachability analysis to obtain reachable states.
    storm::dd::Bdd<Type> transitionMatrixBdd = system.transitions.notZero();
    if (model.getModelType() == storm::jani::ModelType::MDP || model.getModelType() == storm::jani::ModelType::LTS ||
        model.getModelType() == storm::jani::ModelType::MA) {
        transitionMatrixBdd = transitionMatrixBdd.existsAbstract(variables.allNondeterminismVariables);
    }
    modelComponents.reachableStates = storm::utility::dd::computeReachableStates(modelComponents.initialStates, transitionMatrixBdd, variables.rowMetaVariables,
                                                                                 variables.columnMetaVariables)
                                          .first;

    // Check that the reachable fragment does not overlap with the illegal fragment.
    storm::dd::Bdd<Type> reachableIllegalFragment = modelComponents.reachableStates && system.illegalFragment;
    STORM_LOG_THROW(reachableIllegalFragment.isZero(), storm::exceptions::WrongFormatException,
                    "There are reachable states in the model that have synchronizing edges enabled that write the same global variable.");

    // Cut transitions to reachable states.
    storm::dd::Add<Type, ValueType> reachableStatesAdd = modelComponents.reachableStates.template toAdd<ValueType>();
    modelComponents.transitionMatrix = system.transitions * reachableStatesAdd;

    // Fix deadlocks if existing.
    modelComponents.deadlockStates =
        fixDeadlocks(model.getModelType(), modelComponents.transitionMatrix, transitionMatrixBdd, modelComponents.reachableStates, variables);

    // Cut the deadlock states by removing all states that we 'converted' to deadlock states by making them terminal.
    modelComponents.deadlockStates = modelComponents.deadlockStates && !terminalStates;

    // Build the reward models.
    modelComponents.rewardModels =
        buildRewardModels(reachableStatesAdd, modelComponents.transitionMatrix, model.getModelType(), variables, system, rewardVariables);

    // Finally, create the model.
    return createModel(model.getModelType(), variables, modelComponents);
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> DdJaniModelBuilder<Type, ValueType>::build(storm::jani::Model const& model,
                                                                                                            Options const& options) {
    // Prepare the model and do some sanity checks
    if (!std::is_same<ValueType, storm::RationalFunction>::value && model.hasUndefinedConstants()) {
        std::vector<std::reference_wrapper<storm::jani::Constant const>> undefinedConstants = model.getUndefinedConstants();
        std::vector<std::string> strings;
        for (auto const& constant : undefinedConstants) {
            std::stringstream stream;
            stream << constant.get().getName() << " (" << constant.get().getType() << ")";
            strings.push_back(stream.str());
        }
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                        "Model still contains these undefined constants: " << boost::join(strings, ", ") << ".");
    }

    STORM_LOG_THROW(!model.usesAssignmentLevels(), storm::exceptions::WrongFormatException,
                    "The symbolic JANI model builder currently does not support assignment levels.");
    auto features = model.getModelFeatures();
    features.remove(storm::jani::ModelFeature::DerivedOperators);
    features.remove(storm::jani::ModelFeature::StateExitRewards);
    features.remove(storm::jani::ModelFeature::MultiObjectiveProperties);

    storm::jani::Model preparedModel = model;
    preparedModel.simplifyComposition();
    if (features.hasArrays()) {
        STORM_LOG_ERROR(
            "The jani model still considers arrays. These should have been eliminated before calling the dd builder. The arrays are eliminated now, but "
            "occurrences in properties will not be handled properly.");
        preparedModel.eliminateArrays();
        features.remove(storm::jani::ModelFeature::Arrays);
    }
    if (features.hasFunctions()) {
        STORM_LOG_ERROR(
            "The jani model still considers functions. These should have been substituted before calling the dd builder. The functions are substituted now, "
            "but occurrences in properties will not be handled properly.");
        preparedModel.substituteFunctions();
        features.remove(storm::jani::ModelFeature::Functions);
    }
    STORM_LOG_THROW(features.empty(), storm::exceptions::InvalidStateException,
                    "The dd jani model builder does not support the following model feature(s): " << features.toString() << ".");

    // Lift the transient edge destinations. We can do so, as we know that there are no assignment levels (because that's not supported anyway).
    if (preparedModel.hasTransientEdgeDestinationAssignments()) {
        // This operation is correct as we are asserting that there are no assignment levels and no non-trivial reward expressions.
        preparedModel.liftTransientEdgeDestinationAssignments();
    }

    STORM_LOG_THROW(!preparedModel.hasTransientEdgeDestinationAssignments(), storm::exceptions::WrongFormatException,
                    "The symbolic JANI model builder currently does not support transient edge destination assignments.");

    // Create the manager
    auto manager = std::make_shared<storm::dd::DdManager<Type>>();

    // Prepare a result
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> result;

    // invoke the builder
    manager->execute([&preparedModel, &options, &manager, &result]() { result = buildInternal<Type, ValueType>(preparedModel, options, manager); });

    return result;
}

template class DdJaniModelBuilder<storm::dd::DdType::CUDD, double>;
template class DdJaniModelBuilder<storm::dd::DdType::Sylvan, double>;

template class DdJaniModelBuilder<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class DdJaniModelBuilder<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace builder
}  // namespace storm
