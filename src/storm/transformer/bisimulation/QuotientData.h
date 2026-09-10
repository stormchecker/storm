#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "storm/models/sparse/ModelForward.h"
#include "storm/storage/BitVector.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/WeakBisimulationData.h"
#include "storm/utility/OptionalRef.h"

namespace storm::bisimulation {

/*!
 * The index mappings between an input model and its bisimulation quotient, derived from a partition of the input model's states.
 */
template<typename ValueType>
struct QuotientData {
    /*!
     * Computes the state index mappings between the given model and the quotient described by the given partition.
     * @param preferredRepresentatives if given, the representative of a block is picked among these states whenever the block contains such a state.
     * @note does not compute any choice mappings, cf. below.
     */
    QuotientData(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition const& partition,
                 storm::OptionalRef<storm::storage::BitVector const> preferredRepresentatives = storm::NullRef);

    std::vector<uint64_t> toQuotientState;        // assigns to each input model state the corresponding quotient state
    std::vector<uint64_t> toRepresentativeState;  // assigns to each quotient state the corresponding representative state in the input model
    std::optional<std::vector<uint64_t>>
        toQuotientChoice;  // assigns to each input model choice (model.getNumberOfChoices() entries) the quotient choice that represents it.

    // The following choice mappings are set iff signature-based refinement (and thus choice deduplication) was performed, cf. Signatures::moveToQuotientData.
    struct SignatureData {
        std::vector<uint64_t> toRepresentativeChoice;  // assigns to each quotient choice the corresponding representative choice in the input model.
        std::vector<std::map<uint64_t, ValueType>>
            quotientChoiceDistributions;  // assigns to each quotient choice (same length as toRepresentativeChoice) the block distribution it was derived from.
        std::vector<uint64_t>
            quotientChoiceGroupIndices;  // CSR-style: has toRepresentativeState.size() + 1 entries; for quotient state s, the associated quotient choices are
        // those in the (right-open) range [quotientChoiceGroupIndices->at(s), quotientChoiceGroupIndices->at(s + 1)).
        // quotientChoiceGroupIndices->front() == 0 and
        // quotientChoiceGroupIndices->back() == toRepresentativeChoice->size() == quotientChoiceSignatures->size().
    };
    std::optional<SignatureData> signatureData;

    // The state-level information that weak bisimulation works with. Set iff weak bisimulation was computed.
    std::optional<WeakBisimulationData> weakData;
};

}  // namespace storm::bisimulation
