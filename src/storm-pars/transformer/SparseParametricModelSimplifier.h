#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <optional>
#include <string>

#include "storm/logic/Formulas.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace transformer {

/*!
 * This class performs different steps to simplify the given (parametric) model.
 * Checking the obtained simplified formula on the simplified model yields the same result as checking the original formula on the original model (wrt. to the
 * initial states of the two models) End Components of nondeterministic models are removed whenever this is valid for the corresponding formula. This allows us
 * to apply, e.g., value iteration that does not start from the 0,...,0 vector.
 */
template<typename SparseModelType>
class SparseParametricModelSimplifier {
   public:
    SparseParametricModelSimplifier(SparseModelType const& model);

    /*
     * Invokes the simplification of the model w.r.t. the given formula.
     * Returns true, iff simplification was successful
     */
    bool simplify(storm::logic::Formula const& formula);

    /*
     * Retrieves the simplified model.
     * Note that simplify(formula) needs to be called first and has to return true. Otherwise an exception is thrown
     */
    std::shared_ptr<SparseModelType> getSimplifiedModel() const;

    /*
     * Retrieves the simplified formula.
     * Note that simplify(formula) needs to be called first and has to return true. Otherwise an exception is thrown
     */
    std::shared_ptr<storm::logic::Formula const> getSimplifiedFormula() const;

    /**
     * Set whether to preserve parametric transions (i.e. for robust PLA, where it returns unfavourable MCs).
     *
     * @param preserveParametricTransitions
     */
    void setPreserveParametricTransitions(bool preserveParametricTransitions);

    /**
     * Whether this SparseParametricModelSimplifier preserves parametric transitions.
     */
    bool isPreserveParametricTransitionsSet() const;

   private:
    SparseModelType const& originalModel;

    std::shared_ptr<SparseModelType> simplifiedModel;
    std::shared_ptr<storm::logic::Formula const> simplifiedFormula;
    bool preserveParametricTransitions = false;
};
}  // namespace transformer
}  // namespace storm
