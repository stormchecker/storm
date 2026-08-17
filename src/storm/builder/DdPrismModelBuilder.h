#pragma once

#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <map>

#include "storm/storage/prism/Program.h"

#include "storm/builder/TerminalStatesGetter.h"

#include "storm/logic/Formulas.h"
#include "storm/utility/macros.h"

#include "storm/storage/dd/DdType.h"

namespace storm {
class Environment;
namespace dd {
template<storm::dd::DdType T>
class Bdd;
template<storm::dd::DdType LibraryType, typename ValueType>
class Add;
template<storm::dd::DdType T>
class DdManager;
}  // namespace dd

namespace models {
namespace symbolic {
template<storm::dd::DdType T, typename ValueType>
class Model;

template<storm::dd::DdType T, typename ValueType>
class StandardRewardModel;
}  // namespace symbolic
}  // namespace models

namespace builder {

template<storm::dd::DdType Type, typename ValueType = double>
class DdPrismModelBuilder {
   public:
    /*!
     * A quick check to detect whether the given model is not supported.
     * This method only over-approximates the set of models that can be handled, i.e., if this
     * returns true, the model might still be unsupported.
     */
    static bool canHandle(storm::prism::Program const& program);

    struct Options {
        /*!
         * Creates an object representing the default building options.
         */
        Options();

        /*! Creates an object representing the suggested building options assuming that the given formula is the
         * only one to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
         *
         * @param formula The formula based on which to choose the building options.
         */
        Options(storm::logic::Formula const& formula);

        /*! Creates an object representing the suggested building options assuming that the given formulas are
         * the only ones to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
         *
         * @param formula Thes formula based on which to choose the building options.
         */
        Options(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas);

        /*!
         * Changes the options in a way that ensures that the given formula can be checked on the model once it
         * has been built.
         *
         * @param formula The formula that is to be ''preserved''.
         */
        void preserveFormula(storm::logic::Formula const& formula);

        /*!
         * Analyzes the given formula and sets an expression for the states states of the model that can be
         * treated as terminal states. Note that this may interfere with checking properties different than the
         * one provided.
         *
         * @param formula The formula used to (possibly) derive an expression for the terminal states of the
         * model.
         */
        void setTerminalStatesFromFormula(storm::logic::Formula const& formula);

        // A flag that indicates whether or not all reward models are to be build.
        bool buildAllRewardModels;

        // A list of reward models to be build in case not all reward models are to be build.
        std::set<std::string> rewardModelsToBuild;

        // A flag indicating whether all labels are to be build.
        bool buildAllLabels;

        // An optional set of labels that, if given, restricts the labels that are built.
        boost::optional<std::set<std::string>> labelsToBuild;

        // An optional set of expression or labels that characterizes (a subset of) the terminal states of the model.
        // If this is set, the outgoing transitions of these states are replaced with a self-loop.
        storm::builder::TerminalStates terminalStates;

        // A flag that indicates whether deadlock states should be fixed by inserting a self-loop. If not set,
        // an error is raised whenever a deadlock state is encountered.
        bool fixDeadlocks = true;
    };

    /*!
     * Translates the given program into a symbolic model (i.e. one that stores the transition relation as a
     * decision diagram).
     *
     * @param env The environment providing the settings for the DD library (e.g. Sylvan or CUDD).
     * @param program The program to translate.
     * @param options The options to use when building the model.
     * @return A pointer to the resulting model.
     */
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> build(storm::Environment const& env, storm::prism::Program const& program,
                                                                           Options const& options = Options());

   private:
    // This structure can store the decision diagrams representing a particular action.
    struct UpdateDecisionDiagram {
        UpdateDecisionDiagram() : updateDd(), assignedGlobalVariables() {
            // Intentionally left empty.
        }

        UpdateDecisionDiagram(storm::dd::Add<Type, ValueType> const& updateDd, std::set<storm::expressions::Variable> const& assignedGlobalVariables)
            : updateDd(updateDd), assignedGlobalVariables(assignedGlobalVariables) {
            // Intentionally left empty.
        }

        // The DD representing the update behaviour.
        storm::dd::Add<Type, ValueType> updateDd;

        // Keep track of the global variables that were written by this update.
        std::set<storm::expressions::Variable> assignedGlobalVariables;
    };

    // This structure can store the decision diagrams representing a particular action.
    struct ActionDecisionDiagram {
        ActionDecisionDiagram() : guardDd(), transitionsDd(), numberOfUsedNondeterminismVariables(0) {
            // Intentionally left empty.
        }

        ActionDecisionDiagram(storm::dd::DdManager<Type> const& manager,
                              std::set<storm::expressions::Variable> const& assignedGlobalVariables = std::set<storm::expressions::Variable>(),
                              uint_fast64_t numberOfUsedNondeterminismVariables = 0)
            : guardDd(manager.getBddZero()),
              transitionsDd(manager.template getAddZero<ValueType>()),
              numberOfUsedNondeterminismVariables(numberOfUsedNondeterminismVariables),
              assignedGlobalVariables(assignedGlobalVariables) {
            // Intentionally left empty.
        }

        ActionDecisionDiagram(storm::dd::Bdd<Type> guardDd, storm::dd::Add<Type, ValueType> transitionsDd,
                              std::set<storm::expressions::Variable> const& assignedGlobalVariables = std::set<storm::expressions::Variable>(),
                              uint_fast64_t numberOfUsedNondeterminismVariables = 0)
            : guardDd(guardDd),
              transitionsDd(transitionsDd),
              numberOfUsedNondeterminismVariables(numberOfUsedNondeterminismVariables),
              assignedGlobalVariables(assignedGlobalVariables) {
            // Intentionally left empty.
        }

        void ensureContainsVariables(std::set<storm::expressions::Variable> const& rowMetaVariables,
                                     std::set<storm::expressions::Variable> const& columnMetaVariables) {
            guardDd.addMetaVariables(rowMetaVariables);
            transitionsDd.addMetaVariables(rowMetaVariables);
            transitionsDd.addMetaVariables(columnMetaVariables);
        }

        ActionDecisionDiagram(ActionDecisionDiagram const& other) = default;
        ActionDecisionDiagram& operator=(ActionDecisionDiagram const& other) = default;

        // The guard of the action.
        storm::dd::Bdd<Type> guardDd;

        // The actual transitions (source and target states).
        storm::dd::Add<Type, ValueType> transitionsDd;

        // The number of variables that are used to encode the nondeterminism.
        uint_fast64_t numberOfUsedNondeterminismVariables;

        // Keep track of the global variables that were written by this action.
        std::set<storm::expressions::Variable> assignedGlobalVariables;
    };

    // This structure holds all decision diagrams related to a module.
    struct ModuleDecisionDiagram {
        ModuleDecisionDiagram() : independentAction(), synchronizingActionToDecisionDiagramMap(), identity(), numberOfUsedNondeterminismVariables(0) {
            // Intentionally left empty.
        }

        ModuleDecisionDiagram(storm::dd::DdManager<Type> const& manager)
            : independentAction(manager),
              synchronizingActionToDecisionDiagramMap(),
              identity(manager.template getAddZero<ValueType>()),
              numberOfUsedNondeterminismVariables(0) {
            // Intentionally left empty.
        }

        ModuleDecisionDiagram(ActionDecisionDiagram const& independentAction,
                              std::map<uint_fast64_t, ActionDecisionDiagram> const& synchronizingActionToDecisionDiagramMap,
                              storm::dd::Add<Type, ValueType> const& identity, uint_fast64_t numberOfUsedNondeterminismVariables = 0)
            : independentAction(independentAction),
              synchronizingActionToDecisionDiagramMap(synchronizingActionToDecisionDiagramMap),
              identity(identity),
              numberOfUsedNondeterminismVariables(numberOfUsedNondeterminismVariables) {
            // Intentionally left empty.
        }

        ModuleDecisionDiagram(ModuleDecisionDiagram const& other) = default;
        ModuleDecisionDiagram& operator=(ModuleDecisionDiagram const& other) = default;

        bool hasSynchronizingAction(uint_fast64_t actionIndex) {
            return synchronizingActionToDecisionDiagramMap.find(actionIndex) != synchronizingActionToDecisionDiagramMap.end();
        }

        std::set<uint_fast64_t> getSynchronizingActionIndices() const {
            std::set<uint_fast64_t> result;
            for (auto const& entry : synchronizingActionToDecisionDiagramMap) {
                result.insert(entry.first);
            }
            return result;
        }

        // The decision diagram for the independent action.
        ActionDecisionDiagram independentAction;

        // A mapping from synchronizing action indices to the decision diagram.
        std::map<uint_fast64_t, ActionDecisionDiagram> synchronizingActionToDecisionDiagramMap;

        // A decision diagram that represents the identity of this module.
        storm::dd::Add<Type, ValueType> identity;

        // The number of variables encoding the nondeterminism that were actually used.
        uint_fast64_t numberOfUsedNondeterminismVariables;
    };

    /*!
     * Structure to store all information required to generate the model from the program.
     */
    class GenerationInformation;

    /*!
     * Structure to store the result of the system creation phase.
     */
    struct SystemResult;

   private:
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> buildInternal(storm::prism::Program const& program, Options const& options,
                                                                                   std::shared_ptr<storm::dd::DdManager<Type>> const& manager);

    template<storm::dd::DdType TypePrime, typename ValueTypePrime>
    friend class ModuleComposer;

    static std::set<storm::expressions::Variable> equalizeAssignedGlobalVariables(GenerationInformation const& generationInfo, ActionDecisionDiagram& action1,
                                                                                  ActionDecisionDiagram& action2);

    static std::set<storm::expressions::Variable> equalizeAssignedGlobalVariables(GenerationInformation const& generationInfo,
                                                                                  std::vector<ActionDecisionDiagram>& actionDds);

    static storm::dd::Add<Type, ValueType> encodeChoice(GenerationInformation& generationInfo, uint_fast64_t nondeterminismVariableOffset,
                                                        uint_fast64_t numberOfBinaryVariables, int_fast64_t value);

    static UpdateDecisionDiagram createUpdateDecisionDiagram(GenerationInformation& generationInfo, storm::prism::Module const& module,
                                                             storm::dd::Add<Type, ValueType> const& guard, storm::prism::Update const& update);

    static ActionDecisionDiagram createCommandDecisionDiagram(GenerationInformation& generationInfo, storm::prism::Module const& module,
                                                              storm::prism::Command const& command);

    static ActionDecisionDiagram createActionDecisionDiagram(GenerationInformation& generationInfo, storm::prism::Module const& module,
                                                             uint_fast64_t synchronizationActionIndex, uint_fast64_t nondeterminismVariableOffset);

    static ActionDecisionDiagram combineCommandsToActionMarkovChain(GenerationInformation& generationInfo, std::vector<ActionDecisionDiagram>& commandDds);

    static ActionDecisionDiagram combineCommandsToActionMDP(GenerationInformation& generationInfo, std::vector<ActionDecisionDiagram>& commandDds,
                                                            uint_fast64_t nondeterminismVariableOffset);

    static ActionDecisionDiagram combineSynchronizingActions(ActionDecisionDiagram const& action1, ActionDecisionDiagram const& action2);

    static ActionDecisionDiagram combineUnsynchronizedActions(GenerationInformation const& generationInfo, ActionDecisionDiagram& action1,
                                                              ActionDecisionDiagram& action2, storm::dd::Add<Type, ValueType> const& identityDd1,
                                                              storm::dd::Add<Type, ValueType> const& identityDd2);

    static ActionDecisionDiagram combineUnsynchronizedActions(GenerationInformation const& generationInfo, ActionDecisionDiagram& action1,
                                                              ActionDecisionDiagram& action2);

    static ModuleDecisionDiagram createModuleDecisionDiagram(GenerationInformation& generationInfo, storm::prism::Module const& module,
                                                             std::map<uint_fast64_t, uint_fast64_t> const& synchronizingActionToOffsetMap);

    static storm::dd::Add<Type, ValueType> getSynchronizationDecisionDiagram(GenerationInformation& generationInfo, uint_fast64_t actionIndex = 0);

    static storm::dd::Add<Type, ValueType> createSystemFromModule(GenerationInformation& generationInfo, ModuleDecisionDiagram& module);

    static std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<Type, ValueType>> createRewardModelDecisionDiagrams(
        std::vector<std::reference_wrapper<storm::prism::RewardModel const>> const& selectedRewardModels, SystemResult& system,
        GenerationInformation& generationInfo, ModuleDecisionDiagram const& globalModule, storm::dd::Add<Type, ValueType> const& reachableStatesAdd,
        storm::dd::Add<Type, ValueType> const& transitionMatrix);

    static storm::models::symbolic::StandardRewardModel<Type, ValueType> createRewardModelDecisionDiagrams(
        GenerationInformation& generationInfo, storm::prism::RewardModel const& rewardModel, ModuleDecisionDiagram const& globalModule,
        storm::dd::Add<Type, ValueType> const& reachableStatesAdd, storm::dd::Add<Type, ValueType> const& transitionMatrix,
        boost::optional<storm::dd::Add<Type, ValueType>>& stateActionDd);

    static SystemResult createSystemDecisionDiagram(GenerationInformation& generationInfo);

    static storm::dd::Bdd<Type> createInitialStatesDecisionDiagram(GenerationInformation& generationInfo);
};

}  // namespace builder
}  // namespace storm
