#pragma once

#include "storm/models/sparse/Model.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/Signatures.h"
#include "storm/transformer/bisimulation/WeakBisimulationData.h"
#include "storm/utility/OptionalRef.h"

namespace storm::bisimulation {

/*!
 * The kind of bisimulation that splitter-based refinement computes.
 */
enum class SplitterRefinementMode {
    Strong,             // Strong bisimulation.
    WeakDiscreteTime,   // Weak bisimulation on a discrete-time model, i.e., transition probabilities are considered conditioned on leaving the own block.
    WeakContinuousTime  // Weak bisimulation on a continuous-time model, i.e., transition rates into the own block are ignored.
};

/*!
 * Performs splitter-based partition refinement.
 * @note only applicable to deterministic models.
 * @param backwardTransitions the transposed transition matrix of the model.
 * @param weakData must be given iff a weak Mode is used, cf. Initialization::getWeakBisimulationData. Its silentStates are kept up to date with the
 * partition; the other members are only read.
 */
template<typename ValueType, SplitterRefinementMode Mode = SplitterRefinementMode::Strong>
void performSplitterBasedRefinement(storm::models::sparse::Model<ValueType> const& model,
                                    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::bisimulation::Partition& partition,
                                    ValueType const tolerance, storm::OptionalRef<WeakBisimulationData> weakData = storm::NullRef);

/*!
 * Performs signature-based partition refinement.
 * @note only applicable to nondeterministic models.
 * @note upon return, the signature of every state (as cached by `signatures`) is up to date with respect to the final partition.
 */
template<typename ValueType, SignatureMode Mode>
void performSignatureBasedRefinement(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition& partition,
                                     Signatures<ValueType, Mode>& signatures);

}  // namespace storm::bisimulation
