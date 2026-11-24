#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <utility>
#include <vector>

#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/transformer/Product.h"

namespace storm {
namespace transformer {

template<typename Model>
class ProductBuilder {
   public:
    typedef storm::storage::SparseMatrix<typename Model::ValueType> matrix_type;

    template<typename ProductOperator>
    static typename Product<Model>::ptr buildProduct(const matrix_type& originalMatrix, ProductOperator& prodOp,
                                                     const storm::storage::BitVector& statesOfInterest) {
        bool deterministic = originalMatrix.hasTrivialRowGrouping();

        typedef storm::storage::sparse::state_type state_type;
        typedef std::pair<state_type, state_type> product_state_type;

        state_type nextState = 0;
        std::map<product_state_type, state_type> productStateToProductIndex;
        std::vector<product_state_type> productIndexToProductState;
        std::vector<state_type> prodInitial;

        // use deque for todo so that the states are handled in the order
        // of their index in the product model, which is required due to the
        // use of the SparseMatrixBuilder that can only handle linear addNextValue
        // calls
        std::deque<state_type> todo;
        for (state_type s_0 : statesOfInterest) {
            state_type q_0 = prodOp.getInitialState(s_0);

            // std::cout << "Initial: " << s_0 << ", " << q_0 << " = " << nextState << "\n";

            product_state_type s_q(s_0, q_0);
            state_type index = nextState++;
            productStateToProductIndex[s_q] = index;
            productIndexToProductState.push_back(s_q);
            prodInitial.push_back(index);
            todo.push_back(index);
        }

        storm::storage::SparseMatrixBuilder<typename Model::ValueType> builder(0, 0, 0, false, !deterministic, 0);
        std::size_t curRow = 0;
        while (!todo.empty()) {
            state_type prodIndexFrom = todo.front();
            todo.pop_front();

            product_state_type from = productIndexToProductState.at(prodIndexFrom);
            // std::cout << "Handle " << from.first << "," << from.second << " (prodIndexFrom = " << prodIndexFrom << "):\n";
            if (deterministic) {
                typename matrix_type::const_rows row = originalMatrix.getRow(from.first);
                for (auto const& entry : row) {
                    state_type t = entry.getColumn();
                    state_type p = prodOp.getSuccessor(from.second, t);
                    // std::cout << " p = " << p << "\n";
                    product_state_type t_p(t, p);
                    state_type prodIndexTo;
                    auto it = productStateToProductIndex.find(t_p);
                    if (it == productStateToProductIndex.end()) {
                        prodIndexTo = nextState++;
                        todo.push_back(prodIndexTo);
                        productIndexToProductState.push_back(t_p);
                        productStateToProductIndex[t_p] = prodIndexTo;
                        // std::cout << " Adding " << t_p.first << "," << t_p.second << " as " << prodIndexTo << "\n";
                    } else {
                        prodIndexTo = it->second;
                    }
                    // std::cout << " " << t_p.first << "," << t_p.second << ": to = " << prodIndexTo << "\n";

                    // std::cout << " addNextValue(" << prodIndexFrom << "," << prodIndexTo << "," << entry.getValue() << ")\n";
                    builder.addNextValue(prodIndexFrom, prodIndexTo, entry.getValue());
                }
            } else {
                std::size_t numRows = originalMatrix.getRowGroupSize(from.first);
                builder.newRowGroup(curRow);
                for (std::size_t i = 0; i < numRows; i++) {
                    auto const& row = originalMatrix.getRow(from.first, i);
                    for (auto const& entry : row) {
                        state_type t = entry.getColumn();
                        state_type p = prodOp.getSuccessor(from.second, t);
                        // std::cout << " p = " << p << "\n";
                        product_state_type t_p(t, p);
                        state_type prodIndexTo;
                        auto it = productStateToProductIndex.find(t_p);
                        if (it == productStateToProductIndex.end()) {
                            prodIndexTo = nextState++;
                            todo.push_back(prodIndexTo);
                            productIndexToProductState.push_back(t_p);
                            productStateToProductIndex[t_p] = prodIndexTo;
                            // std::cout << " Adding " << t_p.first << "," << t_p.second << " as " << prodIndexTo << "\n";
                        } else {
                            prodIndexTo = it->second;
                        }
                        // std::cout << " " << t_p.first << "," << t_p.second << ": to = " << prodIndexTo << "\n";

                        // std::cout << " addNextValue(" << prodIndexFrom << "," << prodIndexTo << "," << entry.getValue() << ")\n";
                        builder.addNextValue(curRow, prodIndexTo, entry.getValue());
                    }
                    curRow++;
                }
            }
        }

        state_type numberOfProductStates = nextState;

        Model product(builder.build(), storm::models::sparse::StateLabeling(numberOfProductStates));
        storm::storage::BitVector productStatesOfInterest(product.getNumberOfStates());
        for (auto& s : prodInitial) {
            productStatesOfInterest.set(s);
        }
        std::string prodSoiLabel = product.getStateLabeling().addUniqueLabel("soi", productStatesOfInterest);

        // const storm::models::sparse::StateLabeling& orignalLabels = dtmc->getStateLabeling();
        // for (originalLabels.)

        return typename Product<Model>::ptr(
            new Product<Model>(std::move(product), std::move(prodSoiLabel), std::move(productStateToProductIndex), std::move(productIndexToProductState)));
    }
};
}  // namespace transformer
}  // namespace storm
