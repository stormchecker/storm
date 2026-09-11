#pragma once

#include "storm/models/sparse/Model.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/Signatures.h"

namespace storm::bisimulation {

/*!
 * Performs signature-based partition refinement.
 * @note applicable to deterministic and nondeterministic models, in contrast to splitter-based refinement.
 * @note upon return, the signature of every state (as cached by `signatures`) is up to date with respect to the final partition.
 */
template<typename ValueType, SignatureMode Mode>
void performSignatureBasedRefinement(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition& partition,
                                     Signatures<ValueType, Mode>& signatures);

}  // namespace storm::bisimulation
