#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "storm/logic/FormulasForwardDeclarations.h"
#include "storm/models/sparse/ModelForward.h"
#include "storm/transformer/bisimulation/Options.h"

namespace storm::bisimulation {

template<typename ValueType>
struct ReturnType {
    std::shared_ptr<storm::models::sparse::Model<ValueType>> quotient;
    std::vector<uint64_t> toQuotientStateMapping;                  // assigns to each input model state the corresponding quotient state
    std::optional<std::vector<uint64_t>> toQuotientChoiceMapping;  // assigns to each input model choice the corresponding quotient choice. Only available if
                                                                   // requested through the bisimulation options.
};

/*!
 * Computes the bisimulation quotient of the given model.
 *
 * @param model the model to minimize.
 * @param formulas the formulas that need to be preserved by the minimization.
 * @param options the options to use for the minimization.
 */
template<typename ValueType>
ReturnType<ValueType> performBisimulationMinimization(storm::models::sparse::Model<ValueType> const& model,
                                                      std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = {},
                                                      Options const& options = {});

}  // namespace storm::bisimulation
