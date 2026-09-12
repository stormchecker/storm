#pragma once

#include <optional>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/transformer/bisimulation/BisimulationType.h"
#include "storm/utility/constants.h"

namespace storm::bisimulation {

/*!
 * The way state labels will be preserved by the bisimulation.
 */
enum class StateLabelPreservation {
    Default,               // FormulaPropositional if at least one formula is given, All otherwise.
    All,                   // Every state label of the model is preserved.
    None,                  // No state label is preserved (potentially invalidating the formulas).
    FormulaPropositional,  // Only the truth values of the maximal propositional subformulas of the formulas are preserved, e.g., P=? [F "a" & !"b"] does not
                           // require to distinguish !"a"-states from "b"-states.
    FormulaIndividual      // Each state label occurring in a formula is preserved.
};

struct Options {
    // The model annotations that must be preserved.
    StateLabelPreservation stateLabelPreservation = StateLabelPreservation::Default;
    std::optional<bool> preserveAllRewards = std::nullopt;  // If not specified, then all rewards are preserved iff no formula is given.
    bool preserveChoiceLabels = true;                       // Preserves the choice labels of the original model (if available).
    bool preserveChoiceOrigins = true;                      // Preserves the choice origins of the original model (if available).

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
