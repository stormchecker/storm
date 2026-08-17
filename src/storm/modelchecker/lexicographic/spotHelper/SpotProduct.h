#pragma once

#include <cstdint>

#include "storm/automata/DeterministicAutomaton.h"
#include "storm/logic/ExtractMaximalStateFormulasVisitor.h"
#include "storm/logic/MultiObjectiveFormula.h"

namespace storm::modelchecker::helper::lexicographic::spothelper {

/**
 * Function that creates a determinitistic automaton with Streett-acceptance condition. That is done based on a multi-objective formula.
 * For each subformula, a new automaton is created and directly merged into a big product-automaton.
 * Spot is used as tool for this, however, currently it has to be an adapted version of Spot.
 * @param formula the multi-objective formula
 * @param extracted extracted atomic propositions (is empty in the beginning, and will be filled in the function)
 * @param acceptanceConditions indication which formula has which streett pairs as acceptance condition
 * @return
 */
std::shared_ptr<storm::automata::DeterministicAutomaton> ltl2daSpotProduct(storm::logic::MultiObjectiveFormula const& formula,
                                                                           storm::logic::ExtractMaximalStateFormulasVisitor::ApToFormulaMap& extracted,
                                                                           std::vector<uint64_t>& acceptanceConditions);
}  // namespace storm::modelchecker::helper::lexicographic::spothelper
