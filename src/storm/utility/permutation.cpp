#include "storm/utility/permutation.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <random>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm::utility::permutation {

/*!
 * Converts the given order to a string.
 */
std::string orderKindtoString(OrderKind order) {
    switch (order) {
        case OrderKind::Bfs:
            return "bfs";
        case OrderKind::Dfs:
            return "dfs";
        case OrderKind::ReverseBfs:
            return "reverse-bfs";
        case OrderKind::ReverseDfs:
            return "reverse-dfs";
        case OrderKind::Random:
            return "random";
    }
    STORM_LOG_ASSERT(false, "unreachable");
    return "";
}

OrderKind orderKindFromString(std::string const& order) {
    for (auto kind : {OrderKind::Bfs, OrderKind::Dfs, OrderKind::ReverseBfs, OrderKind::ReverseDfs, OrderKind::Random}) {
        if (order == orderKindtoString(kind)) {
            return kind;
        }
    }
    STORM_LOG_ASSERT(false, "Unknown order kind");
    return OrderKind::Bfs;
}

std::vector<std::string> orderKinds() {
    std::vector<std::string> kinds;
    for (auto kind : {OrderKind::Bfs, OrderKind::Dfs, OrderKind::ReverseBfs, OrderKind::ReverseDfs, OrderKind::Random}) {
        kinds.push_back(orderKindtoString(kind));
    }
    return kinds;
}

std::vector<index_type> createRandomPermutation(index_type size) {
    std::random_device rd;
    return createRandomPermutation(size, rd());
}

std::vector<index_type> createRandomPermutation(index_type size, index_type seed) {
    std::vector<index_type> permutation(size);
    std::iota(permutation.begin(), permutation.end(), 0);
    // The random number engine below assumes that it operates on 64-bit unsigned integers.
    static_assert(std::is_same_v<index_type, uint64_t>, "index_type is assumed to be a 64-bit unsigned integer");
    std::shuffle(permutation.begin(), permutation.end(), std::mt19937_64(seed));
    return permutation;
}

template<typename ValueType>
std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                          storm::storage::BitVector const& initialStates) {
    if (order == OrderKind::Random) {
        return createRandomPermutation(transitionMatrix.getRowGroupCount());
    }
    STORM_LOG_ASSERT((order == OrderKind::Bfs || order == OrderKind::Dfs || order == OrderKind::ReverseBfs || order == OrderKind::ReverseDfs),
                     "Unknown order kind");
    STORM_LOG_ASSERT(initialStates.size() == transitionMatrix.getRowGroupCount(), "Unexpected dimensions of initial states and transition matrix.");
    STORM_LOG_ASSERT(initialStates.size() == transitionMatrix.getColumnCount(), "Unexpected dimensions of initial states and transition matrix.");
    std::vector<index_type> permutation;
    permutation.reserve(transitionMatrix.getRowGroupCount());

    std::deque<index_type> stack(initialStates.begin(), initialStates.end());
    storm::storage::BitVector discoveredStates = initialStates;
    bool const useFifoStack = order == OrderKind::Bfs || order == OrderKind::ReverseBfs;

    while (!stack.empty()) {
        auto current = stack.front();
        stack.pop_front();
        permutation.push_back(current);
        for (auto const& entry : transitionMatrix.getRowGroup(current)) {
            if (auto const successor = entry.getColumn(); !discoveredStates.get(successor)) {
                discoveredStates.set(successor, true);
                if (useFifoStack) {
                    stack.push_back(successor);
                } else {
                    stack.push_front(successor);
                }
            }
        }
    }
    if (order == OrderKind::ReverseDfs || order == OrderKind::ReverseBfs) {
        reversePermutationInPlace(permutation);
    }
    return permutation;
}

std::vector<index_type> invertPermutation(std::vector<index_type> const& permutation) {
    std::vector<index_type> inverted(permutation.size());
    for (index_type i = 0; i < permutation.size(); ++i) {
        inverted[permutation[i]] = i;
    }
    return inverted;
}

void reversePermutationInPlace(std::vector<index_type>& permutation) {
    auto const max = permutation.size() - 1;
    for (auto& i : permutation) {
        i = max - i;
    }
}

bool isValidPermutation(std::vector<index_type> const& permutation) {
    storage::BitVector occurring(permutation.size(), false);
    for (auto i : permutation) {
        if (occurring.get(i)) {
            return false;
        }
        occurring.set(i, true);
    }
    return occurring.full();
}

template std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                   storm::storage::BitVector const& initialStates);
template std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                   storm::storage::BitVector const& initialStates);
template std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                   storm::storage::BitVector const& initialStates);
template std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                   storm::storage::BitVector const& initialStates);
template std::vector<index_type> createPermutation(OrderKind order, storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                   storm::storage::BitVector const& initialStates);

}  // namespace storm::utility::permutation