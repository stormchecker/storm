#include "storm-gamebased-ar/abstraction/jani/AutomatonAbstractor.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm-gamebased-ar/abstraction/BottomStateResult.h"
#include "storm-gamebased-ar/abstraction/GameBddResult.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/Edge.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace abstraction {
namespace jani {

using storm::settings::modules::AbstractionSettings;

template<storm::dd::DdType DdType, typename ValueType>
AutomatonAbstractor<DdType, ValueType>::AutomatonAbstractor(storm::jani::Automaton const& automaton, AbstractionInformation<DdType>& abstractionInformation,
                                                            std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory,
                                                            bool useDecomposition, bool addPredicatesForValidBlocks, bool debug)
    : smtSolverFactory(smtSolverFactory), abstractionInformation(abstractionInformation), edges(), automaton(automaton) {
    // For each concrete command, we create an abstract counterpart.
    uint64_t edgeId = 0;
    for (auto const& edge : automaton.getEdges()) {
        edges.emplace_back(edgeId, edge, abstractionInformation, smtSolverFactory, useDecomposition, addPredicatesForValidBlocks, debug);
        ++edgeId;
    }

    if (automaton.getNumberOfLocations() > 1) {
        locationVariables = abstractionInformation.addLocationVariables(automaton.getLocationExpressionVariable(), automaton.getNumberOfLocations() - 1).first;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void AutomatonAbstractor<DdType, ValueType>::refine(std::vector<uint_fast64_t> const& predicates) {
    for (uint_fast64_t index = 0; index < edges.size(); ++index) {
        STORM_LOG_TRACE("Refining edge with index " << index << ".");
        edges[index].refine(predicates);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Expression const& AutomatonAbstractor<DdType, ValueType>::getGuard(uint64_t player1Choice) const {
    return edges[player1Choice].getGuard();
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t AutomatonAbstractor<DdType, ValueType>::getNumberOfUpdates(uint64_t player1Choice) const {
    return edges[player1Choice].getNumberOfUpdates(player1Choice);
}

template<storm::dd::DdType DdType, typename ValueType>
std::map<storm::expressions::Variable, storm::expressions::Expression> AutomatonAbstractor<DdType, ValueType>::getVariableUpdates(
    uint64_t player1Choice, uint64_t auxiliaryChoice) const {
    return edges[player1Choice].getVariableUpdates(auxiliaryChoice);
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<storm::expressions::Variable> const& AutomatonAbstractor<DdType, ValueType>::getAssignedVariables(uint64_t player1Choice) const {
    return edges[player1Choice].getAssignedVariables();
}

template<storm::dd::DdType DdType, typename ValueType>
GameBddResult<DdType> AutomatonAbstractor<DdType, ValueType>::abstract() {
    // First, we retrieve the abstractions of all commands.
    std::vector<GameBddResult<DdType>> edgeDdsAndUsedOptionVariableCounts;
    uint_fast64_t maximalNumberOfUsedOptionVariables = 0;
    for (auto& edge : edges) {
        edgeDdsAndUsedOptionVariableCounts.push_back(edge.abstract());
        maximalNumberOfUsedOptionVariables = std::max(maximalNumberOfUsedOptionVariables, edgeDdsAndUsedOptionVariableCounts.back().numberOfPlayer2Variables);
    }

    // Then, we build the module BDD by adding the single command DDs. We need to make sure that all command
    // DDs use the same amount DD variable encoding the choices of player 2.
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();
    for (auto const& edgeDd : edgeDdsAndUsedOptionVariableCounts) {
        result |= edgeDd.bdd && this->getAbstractionInformation().encodePlayer2Choice(1, edgeDd.numberOfPlayer2Variables, maximalNumberOfUsedOptionVariables);
    }
    return GameBddResult<DdType>(result, maximalNumberOfUsedOptionVariables);
}

template<storm::dd::DdType DdType, typename ValueType>
BottomStateResult<DdType> AutomatonAbstractor<DdType, ValueType>::getBottomStateTransitions(storm::dd::Bdd<DdType> const& reachableStates,
                                                                                            uint_fast64_t numberOfPlayer2Variables) {
    BottomStateResult<DdType> result(this->getAbstractionInformation().getDdManager().getBddZero(),
                                     this->getAbstractionInformation().getDdManager().getBddZero());

    for (auto& edge : edges) {
        BottomStateResult<DdType> commandBottomStateResult = edge.getBottomStateTransitions(reachableStates, numberOfPlayer2Variables, locationVariables);
        result.states |= commandBottomStateResult.states;
        result.transitions |= commandBottomStateResult.transitions;
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> AutomatonAbstractor<DdType, ValueType>::getEdgeDecoratorAdd() const {
    storm::dd::Add<DdType, ValueType> result = this->getAbstractionInformation().getDdManager().template getAddZero<ValueType>();
    for (auto const& edge : edges) {
        result += edge.getEdgeDecoratorAdd(locationVariables);
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> AutomatonAbstractor<DdType, ValueType>::getInitialLocationsBdd() const {
    if (automaton.get().getNumberOfLocations() == 1) {
        return this->getAbstractionInformation().getDdManager().getBddOne();
    }

    std::set<uint64_t> const& initialLocationIndices = automaton.get().getInitialLocationIndices();
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();
    for (auto const& initialLocationIndex : initialLocationIndices) {
        result |= this->getAbstractionInformation().encodeLocation(locationVariables.get().first, initialLocationIndex);
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::vector<EdgeAbstractor<DdType, ValueType>> const& AutomatonAbstractor<DdType, ValueType>::getEdges() const {
    return edges;
}

template<storm::dd::DdType DdType, typename ValueType>
std::vector<EdgeAbstractor<DdType, ValueType>>& AutomatonAbstractor<DdType, ValueType>::getEdges() {
    return edges;
}

template<storm::dd::DdType DdType, typename ValueType>
std::size_t AutomatonAbstractor<DdType, ValueType>::getNumberOfEdges() const {
    return edges.size();
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType> const& AutomatonAbstractor<DdType, ValueType>::getAbstractionInformation() const {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType, typename ValueType>
void AutomatonAbstractor<DdType, ValueType>::notifyGuardsArePredicates() {
    for (auto& edge : edges) {
        edge.notifyGuardIsPredicate();
    }
}

template class AutomatonAbstractor<storm::dd::DdType::CUDD, double>;
template class AutomatonAbstractor<storm::dd::DdType::Sylvan, double>;
template class AutomatonAbstractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace jani
}  // namespace abstraction
}  // namespace storm::gbar
