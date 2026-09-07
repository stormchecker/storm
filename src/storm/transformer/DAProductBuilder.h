#pragma once

#include <vector>

#include "storm/automata/DeterministicAutomaton.h"
#include "storm/storage/BitVector.h"
#include "storm/transformer/DAProduct.h"
#include "storm/transformer/Product.h"
#include "storm/transformer/ProductBuilder.h"

namespace storm {
namespace transformer {
class DAProductBuilder {
   public:
    DAProductBuilder(const storm::automata::DeterministicAutomaton& da, const std::vector<storm::storage::BitVector>& statesForAP)
        : da(da), statesForAP(statesForAP) {}

    template<typename Model>
    typename DAProduct<Model>::ptr build(const Model& originalModel, const storm::storage::BitVector& statesOfInterest) const {
        return build<Model>(originalModel.getTransitionMatrix(), statesOfInterest);
    }

    template<typename Model>
    typename DAProduct<Model>::ptr build(const storm::storage::SparseMatrix<typename Model::ValueType>& originalMatrix,
                                         const storm::storage::BitVector& statesOfInterest) const {
        typename Product<Model>::ptr product = ProductBuilder<Model>::buildProduct(originalMatrix, *this, statesOfInterest);
        storm::automata::AcceptanceCondition::ptr prodAcceptance = da.getAcceptance()->lift(
            product->getProductModel().getNumberOfStates(), [&product](std::size_t prodState) { return product->getAutomatonState(prodState); });

        return typename DAProduct<Model>::ptr(new DAProduct<Model>(std::move(*product), prodAcceptance));
    }

    storm::storage::sparse::state_type getInitialState(storm::storage::sparse::state_type modelState) const {
        return da.getSuccessor(da.getInitialState(), getLabelForState(modelState));
    }

    storm::storage::sparse::state_type getSuccessor(storm::storage::sparse::state_type automatonFrom, storm::storage::sparse::state_type modelTo) const {
        return da.getSuccessor(automatonFrom, getLabelForState(modelTo));
    }

   private:
    const storm::automata::DeterministicAutomaton& da;
    const std::vector<storm::storage::BitVector>& statesForAP;

    storm::automata::APSet::alphabet_element getLabelForState(storm::storage::sparse::state_type s) const {
        storm::automata::APSet::alphabet_element label = da.getAPSet().elementAllFalse();
        for (unsigned int ap = 0; ap < da.getAPSet().size(); ap++) {
            if (statesForAP.at(ap).get(s)) {
                label = da.getAPSet().elementAddAP(label, ap);
            }
        }
        return label;
    }
};
}  // namespace transformer
}  // namespace storm
