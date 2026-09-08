#pragma once

#include "storm/storage/BitVector.h"
#include "storm/transformer/bisimulation/Partition.h"

namespace storm::bisimulation {

/*!
 * The state-level information that weak bisimulation requires on top of the partition
 */
struct WeakBisimulationData {
    WeakBisimulationData(storm::storage::BitVector divergentStates, storm::storage::BitVector stepSensitiveStates, storm::storage::BitVector silentStates);

    /*!
     * The states from which no state outside of their own block can be reached. Such a block never exhibits observable behavior, so it is never split.
     * Divergent states are closed under transitions and a state that can leave a block can also leave every sub-block of it, so this never changes during the
     * refinement.
     */
    storm::storage::BitVector const divergentStates;

    /*!
     * The states for which the number of steps taken within their own block is observable, e.g., because they carry a non-zero preserved reward.
     * These are refined with respect to *strong* bisimulation.
     */
    storm::storage::BitVector const stepSensitiveStates;

    /*!
     * The states that cannot leave their block in a single step. This changes as the partition is refined (a state can only ever go from
     * silent to non-silent) and is kept up to date during refinement.
     */
    storm::storage::BitVector silentStates;

    /*!
     * @return true iff the states of the given block are divergent.
     */
    bool isDivergent(Partition::Block const& block) const;

    /*!
     * @return true iff the number of steps the states of the given block take within the block is observable.
     */
    bool isStepSensitive(Partition::Block const& block) const;

    /*!
     * Checks that every block of the given partition is homogeneous with respect to divergentStates and stepSensitiveStates.
     * Useful for sanity checks (e.g. via assertions). Does not trigger an assert by itself.
     */
    bool checkBlockHomogeneity(Partition const& partition) const;
};

}  // namespace storm::bisimulation
