#include "storm-gamebased-ar/abstraction/prism/ModuleAbstractor.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm-gamebased-ar/abstraction/BottomStateResult.h"
#include "storm-gamebased-ar/abstraction/GameBddResult.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/prism/Module.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace abstraction {
namespace prism {

using storm::settings::modules::AbstractionSettings;

template<storm::dd::DdType DdType, typename ValueType>
ModuleAbstractor<DdType, ValueType>::ModuleAbstractor(storm::prism::Module const& module, AbstractionInformation<DdType>& abstractionInformation,
                                                      std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory, bool useDecomposition,
                                                      bool addPredicatesForValidBlocks, bool debug)
    : smtSolverFactory(smtSolverFactory), abstractionInformation(abstractionInformation), commands(), module(module) {
    // For each concrete command, we create an abstract counterpart.
    for (auto const& command : module.getCommands()) {
        commands.emplace_back(command, abstractionInformation, smtSolverFactory, useDecomposition, addPredicatesForValidBlocks, debug);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void ModuleAbstractor<DdType, ValueType>::refine(std::vector<uint_fast64_t> const& predicates) {
    for (uint_fast64_t index = 0; index < commands.size(); ++index) {
        STORM_LOG_TRACE("Refining command with index " << index << ".");
        commands[index].refine(predicates);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Expression const& ModuleAbstractor<DdType, ValueType>::getGuard(uint64_t player1Choice) const {
    return commands[player1Choice].getGuard();
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t ModuleAbstractor<DdType, ValueType>::getNumberOfUpdates(uint64_t player1Choice) const {
    return commands[player1Choice].getNumberOfUpdates(player1Choice);
}

template<storm::dd::DdType DdType, typename ValueType>
std::map<storm::expressions::Variable, storm::expressions::Expression> ModuleAbstractor<DdType, ValueType>::getVariableUpdates(uint64_t player1Choice,
                                                                                                                               uint64_t auxiliaryChoice) const {
    return commands[player1Choice].getVariableUpdates(auxiliaryChoice);
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<storm::expressions::Variable> const& ModuleAbstractor<DdType, ValueType>::getAssignedVariables(uint64_t player1Choice) const {
    return commands[player1Choice].getAssignedVariables();
}

template<storm::dd::DdType DdType, typename ValueType>
GameBddResult<DdType> ModuleAbstractor<DdType, ValueType>::abstract() {
    // First, we retrieve the abstractions of all commands.
    std::vector<GameBddResult<DdType>> commandDdsAndUsedOptionVariableCounts;
    uint_fast64_t maximalNumberOfUsedOptionVariables = 0;
    for (auto& command : commands) {
        commandDdsAndUsedOptionVariableCounts.push_back(command.abstract());
        maximalNumberOfUsedOptionVariables =
            std::max(maximalNumberOfUsedOptionVariables, commandDdsAndUsedOptionVariableCounts.back().numberOfPlayer2Variables);
    }

    // Then, we build the module BDD by adding the single command DDs. We need to make sure that all command
    // DDs use the same amount DD variable encoding the choices of player 2.
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();
    for (auto const& commandDd : commandDdsAndUsedOptionVariableCounts) {
        result |=
            commandDd.bdd && this->getAbstractionInformation().encodePlayer2Choice(1, commandDd.numberOfPlayer2Variables, maximalNumberOfUsedOptionVariables);
    }
    return GameBddResult<DdType>(result, maximalNumberOfUsedOptionVariables);
}

template<storm::dd::DdType DdType, typename ValueType>
BottomStateResult<DdType> ModuleAbstractor<DdType, ValueType>::getBottomStateTransitions(storm::dd::Bdd<DdType> const& reachableStates,
                                                                                         uint_fast64_t numberOfPlayer2Variables) {
    BottomStateResult<DdType> result(this->getAbstractionInformation().getDdManager().getBddZero(),
                                     this->getAbstractionInformation().getDdManager().getBddZero());

    for (auto& command : commands) {
        BottomStateResult<DdType> commandBottomStateResult = command.getBottomStateTransitions(reachableStates, numberOfPlayer2Variables);
        result.states |= commandBottomStateResult.states;
        result.transitions |= commandBottomStateResult.transitions;
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> ModuleAbstractor<DdType, ValueType>::getCommandUpdateProbabilitiesAdd() const {
    storm::dd::Add<DdType, ValueType> result = this->getAbstractionInformation().getDdManager().template getAddZero<ValueType>();
    for (auto const& command : commands) {
        result += command.getCommandUpdateProbabilitiesAdd();
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::vector<CommandAbstractor<DdType, ValueType>> const& ModuleAbstractor<DdType, ValueType>::getCommands() const {
    return commands;
}

template<storm::dd::DdType DdType, typename ValueType>
std::vector<CommandAbstractor<DdType, ValueType>>& ModuleAbstractor<DdType, ValueType>::getCommands() {
    return commands;
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType> const& ModuleAbstractor<DdType, ValueType>::getAbstractionInformation() const {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType, typename ValueType>
void ModuleAbstractor<DdType, ValueType>::notifyGuardsArePredicates() {
    for (auto& command : commands) {
        command.notifyGuardIsPredicate();
    }
}

template class ModuleAbstractor<storm::dd::DdType::CUDD, double>;
template class ModuleAbstractor<storm::dd::DdType::Sylvan, double>;
template class ModuleAbstractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace prism
}  // namespace abstraction
}  // namespace storm::gbar
