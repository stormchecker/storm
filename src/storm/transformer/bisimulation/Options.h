#pragma once

#include <optional>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/constants.h"

namespace storm::bisimulation {

struct Options {
    enum class BisimulationType { Strong, Weak };

    // The model annotations that must be preserved.
    std::optional<bool> preserveAllStateLabels;  // If not specified, then all state labels are preserved iff no formula is given.
    std::optional<bool> preserveAllRewards;      // If not specified, then all rewards are preserved iff no formula is given.
    bool preserveChoiceLabels = true;            // Preserves the choice labels of the original model (if available).
    bool preserveChoiceOrigins = true;           // Preserves the choice origins of the original model (if available).

    // The kind of bisimulation that is applied.
    bool actionSensitive = false;  // If set, the i'th choice of state1 can only be matched with the i'th choice of state2.
    BisimulationType bisimulationType = BisimulationType::Strong;
    storm::RationalNumber tolerance = storm::utility::zero<RationalNumber>();  // Two values a and b are considered equal if |a-b| <= tolerance.

    // Algorithm Options
    bool createQuotientChoiceMapping = false;  // If set, a mapping from input choice index to quotient choice index is created and returned. This mapping can
                                               // be used, e.g. to map schedulers from the quotient model to the original model.
    bool preferSignatureRefinement =
        false;  // If set, signature-based refinement is used instead of splitter-based refinement (only applies to deterministic models).
};

}  // namespace storm::bisimulation
