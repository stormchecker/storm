#pragma once

#include <boost/optional.hpp>

#include "storm/logic/Bound.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/TimeBound.h"
#include "storm/logic/TimeBoundType.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {
template<typename ValueType>
struct Objective {
    // the original input formula
    std::shared_ptr<storm::logic::Formula const> originalFormula;

    // The preprocessed (simplified) formula
    std::shared_ptr<storm::logic::OperatorFormula const> formula;

    // True iff the complementary event is considered.
    // E.g. if we consider P<1-t [F !"safe"] instead of P>=t [ G "safe"]
    bool considersComplementaryEvent;

    // Limitations for the quantitative objective value (e.g. 0 <= value <= 1 for probabilities).
    // Can be used to guide the underlying solver
    boost::optional<ValueType> lowerResultBound, upperResultBound;

    ValueType clipResult(ValueType const& value) const {
        STORM_LOG_ASSERT(!lowerResultBound || !upperResultBound || *lowerResultBound <= *upperResultBound, "Invalid result bounds.");
        if (lowerResultBound && value < *lowerResultBound) {
            return *lowerResultBound;
        } else if (upperResultBound && value > *upperResultBound) {
            return *upperResultBound;
        } else {
            return value;
        }
    }

    void printToStream(std::ostream& out) const {
        out << "Original: " << *originalFormula;
        out << " \t";
        out << "Preprocessed: " << *formula;
        if (considersComplementaryEvent) {
            out << " (Complementary event)";
        }
        out << " \t";
        out << "result bounds: ";
        if (lowerResultBound && upperResultBound) {
            out << "[" << *lowerResultBound << ", " << *upperResultBound << "]";
        } else if (lowerResultBound) {
            out << ">=" << *lowerResultBound;
        } else if (upperResultBound) {
            out << "<=" << *upperResultBound;
        } else {
            out << " -none-    ";
        }
    }
};
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
