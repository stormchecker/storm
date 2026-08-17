#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace transformer {
template<typename Model>
class Product {
   public:
    typedef std::shared_ptr<Product<Model>> ptr;

    typedef storm::storage::sparse::state_type state_type;
    typedef std::pair<state_type, state_type> product_state_type;
    typedef std::map<product_state_type, state_type> product_state_to_product_index_map;
    typedef std::vector<product_state_type> product_index_to_product_state_vector;

    Product(Model&& productModel, std::string&& productStateOfInterestLabel, product_state_to_product_index_map&& productStateToProductIndex,
            product_index_to_product_state_vector&& productIndexToProductState)
        : productModel(productModel),
          productStateOfInterestLabel(productStateOfInterestLabel),
          productStateToProductIndex(productStateToProductIndex),
          productIndexToProductState(productIndexToProductState) {}

    Product(Product<Model>&& product) = default;
    Product& operator=(Product<Model>&& product) = default;

    Model& getProductModel() {
        return productModel;
    }

    state_type getModelState(state_type productStateIndex) const {
        return productIndexToProductState.at(productStateIndex).first;
    }

    state_type getAutomatonState(state_type productStateIndex) const {
        return productIndexToProductState.at(productStateIndex).second;
    }

    state_type getProductStateIndex(state_type modelState, state_type automatonState) const {
        return productStateToProductIndex.at(product_state_type(modelState, automatonState));
    }

    bool isValidProductState(state_type modelState, state_type automatonState) const {
        return (productStateToProductIndex.count(product_state_type(modelState, automatonState)) > 0);
    }

    storm::storage::BitVector liftFromAutomaton(const storm::storage::BitVector& vector) const {
        state_type n = productModel.getNumberOfStates();
        storm::storage::BitVector lifted(n, false);
        for (state_type s = 0; s < n; s++) {
            if (vector.get(getAutomatonState(s))) {
                lifted.set(s);
            }
        }
        return lifted;
    }

    storm::storage::BitVector liftFromModel(const storm::storage::BitVector& vector) const {
        state_type n = productModel.getNumberOfStates();
        storm::storage::BitVector lifted(n, false);
        for (state_type s = 0; s < n; s++) {
            if (vector.get(getModelState(s))) {
                lifted.set(s);
            }
        }
        return lifted;
    }

    template<typename ValueType>
    std::vector<ValueType> projectToOriginalModel(const Model& originalModel, const std::vector<ValueType>& prodValues) {
        return projectToOriginalModel(originalModel.getNumberOfStates(), prodValues);
    }

    template<typename ValueType>
    std::vector<ValueType> projectToOriginalModel(std::size_t numberOfStates, const std::vector<ValueType>& prodValues) {
        std::vector<ValueType> origValues(numberOfStates);
        for (state_type productState : productModel.getStateLabeling().getStates(productStateOfInterestLabel)) {
            state_type originalState = getModelState(productState);
            origValues.at(originalState) = prodValues.at(productState);
        }
        return origValues;
    }

    const storm::storage::BitVector& getStatesOfInterest() const {
        return productModel.getStates(productStateOfInterestLabel);
    }

    void printMapping(std::ostream& out) const {
        out << "Mapping index -> product state\n";
        for (std::size_t i = 0; i < productIndexToProductState.size(); i++) {
            out << " " << i << ": " << productIndexToProductState.at(i).first << "," << productIndexToProductState.at(i).second << "\n";
        }
    }

   private:
    Model productModel;
    std::string productStateOfInterestLabel;
    product_state_to_product_index_map productStateToProductIndex;
    product_index_to_product_state_vector productIndexToProductState;
};
}  // namespace transformer
}  // namespace storm
