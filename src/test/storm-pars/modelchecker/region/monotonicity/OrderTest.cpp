#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/modelchecker/region/monotonicity/AssumptionChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"
#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/graph.h"

TEST(OrderTest, Simple) {
    auto numberOfStates = 7;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 0));
    EXPECT_EQ(nullptr, order.getNode(2));

    order.add(2);
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(2, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(2, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 2));

    order.add(3);
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(2, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(3, 2));

    order.addToNode(4, order.getNode(2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, order.compare(2, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, order.compare(4, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(4, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(4, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(4, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(3, 4));

    order.addBetween(5, order.getNode(0), order.getNode(3));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(5, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(5, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(3, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(5, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(5, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(2, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(5, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(4, 5));

    order.addBetween(6, order.getNode(5), order.getNode(3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(6, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(6, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(6, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(2, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(6, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(3, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(6, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(6, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(6, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(5, 6));

    order.addRelationNodes(order.getNode(6), order.getNode(4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(6, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(4, 6));
}

TEST(OrderTest, copy_order) {
    auto numberOfStates = 7;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);
    order.add(2);
    order.add(3);
    order.addToNode(4, order.getNode(2));
    order.addBetween(5, order.getNode(0), order.getNode(3));
    order.addBetween(6, order.getNode(5), order.getNode(3));

    auto orderCopy = storm::analysis::Order(order);
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(0, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(1, 0));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(2, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(2, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(1, 2));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(2, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(3, 2));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, orderCopy.compare(2, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, orderCopy.compare(4, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(4, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(4, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(4, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(3, 4));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(5, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(0, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(5, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(3, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(5, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(1, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(5, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(5, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(5, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, orderCopy.compare(5, 4));

    order.addRelationNodes(order.getNode(6), order.getNode(4));
    orderCopy = storm::analysis::Order(order);
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(6, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(0, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(6, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(1, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(6, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(2, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(6, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(3, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(6, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(4, 6));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, orderCopy.compare(6, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, orderCopy.compare(5, 6));
}

TEST(OrderTest, copy_is_deep) {
    // Regression test: Order's copy constructor must deep-copy every Node, not alias the
    // original's nodes. Mutating one Order (adding states, merging nodes) must not affect the
    // other.
    auto numberOfStates = 7;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);
    order.add(2);
    order.add(3);
    order.addToNode(4, order.getNode(2));

    auto orderCopy = storm::analysis::Order(order);

    // Mutating the copy (adding a new state) must not be visible in the original.
    orderCopy.addBetween(5, orderCopy.getNode(0), orderCopy.getNode(3));
    EXPECT_TRUE(orderCopy.contains(5));
    EXPECT_FALSE(order.contains(5));

    // Mutating the original (adding a different state) must not be visible in the copy.
    order.addBetween(6, order.getNode(0), order.getNode(3));
    EXPECT_TRUE(order.contains(6));
    EXPECT_FALSE(orderCopy.contains(6));

    // Merging nodes in the copy must not merge the corresponding nodes in the original: states 2
    // and 3 stay unrelated in the original, even after they are merged in the copy.
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(2, 3));
    orderCopy.mergeNodes(orderCopy.getNode(2), orderCopy.getNode(3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, orderCopy.compare(2, 3));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order.compare(2, 3));

    // Node pointers themselves must not be shared between the two Orders.
    EXPECT_NE(order.getNode(0), orderCopy.getNode(0));
    EXPECT_NE(order.getTop(), orderCopy.getTop());
    EXPECT_NE(order.getBottom(), orderCopy.getBottom());
}

TEST(OrderTest, merge_nodes) {
    auto numberOfStates = 7;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);
    order.add(2);
    order.add(3);
    order.addToNode(4, order.getNode(2));
    order.addBetween(5, order.getNode(0), order.getNode(3));
    order.addBetween(6, order.getNode(5), order.getNode(3));

    order.mergeNodes(order.getNode(4), order.getNode(5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, order.compare(2, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::SAME, order.compare(2, 5));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(0, 4));

    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(6, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(6, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(6, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(3, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::BELOW, order.compare(1, 5));
}

TEST(OrderTest, merge_nodes_inconsistent) {
    // Regression test: forcing a merge that contradicts an already-established chain of relations
    // must be reported gracefully (mergeNodes returns false, isInvalid() becomes true) rather than
    // crashing on an internal assertion when a later compare() call discovers the resulting cycle.
    auto numberOfStates = 6;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);

    // Build a strict chain: top(0) > 3 > 4 > 5 > 2 > bottom(1).
    order.add(2);
    order.addBetween(3, order.getNode(0), order.getNode(2));
    order.addBetween(4, order.getNode(3), order.getNode(2));
    order.addBetween(5, order.getNode(4), order.getNode(2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(4, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order.compare(5, 2));

    // Forcing 3 and 2 to be equal directly contradicts 3 > 4 > 5 > 2: mergeNodes must refuse this
    // (returning false, marking the order invalid) instead of leaving the order in a state where a
    // later compare() call would discover a state both above and below another.
    EXPECT_FALSE(order.mergeNodes(order.getNode(3), order.getNode(2)));
    EXPECT_TRUE(order.isInvalid());

    // Further compare() calls on the now-invalid order must not crash.
    EXPECT_NO_FATAL_FAILURE(order.compare(4, 5));
    EXPECT_NO_FATAL_FAILURE(order.compare(3, 4));
    EXPECT_NO_FATAL_FAILURE(order.compare(0, 1));
}

TEST(OrderTest, sort_states) {
    auto numberOfStates = 7;
    auto above = storm::storage::BitVector(numberOfStates);
    above.set(0);
    auto below = storm::storage::BitVector(numberOfStates);
    below.set(1);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> matrixBuilder(2, 2, 2);
    matrixBuilder.addNextValue(0, 0, storm::RationalFunction(1));
    matrixBuilder.addNextValue(1, 1, storm::RationalFunction(1));
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto matrix = matrixBuilder.build();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(matrix, options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix);
    auto order = storm::analysis::Order(above, below, numberOfStates, decomposition, statesSorted);
    order.add(2);
    order.add(3);
    order.addToNode(4, order.getNode(2));
    order.addBetween(5, order.getNode(0), order.getNode(3));
    order.addBetween(6, order.getNode(5), order.getNode(3));

    std::vector<uint_fast64_t> statesToSort = std::vector<uint_fast64_t>{0, 1, 5, 6};
    auto sortedStates = order.sortStates(&statesToSort);
    EXPECT_EQ(4ul, sortedStates.size());

    auto itr = sortedStates.begin();
    EXPECT_EQ(0ul, *itr);
    EXPECT_EQ(5ul, *(++itr));
    EXPECT_EQ(6ul, *(++itr));
    EXPECT_EQ(1ul, *(++itr));

    statesToSort = std::vector<uint_fast64_t>{0, 1, 5, 6, 2};
    sortedStates = order.sortStates(&statesToSort);
    EXPECT_EQ(5ul, sortedStates.size());

    itr = sortedStates.begin();
    EXPECT_EQ(0ul, *itr);
    EXPECT_EQ(5ul, *(++itr));
    EXPECT_EQ(6ul, *(++itr));
    EXPECT_EQ(1ul, *(++itr));
    EXPECT_EQ(7ul, *(++itr));

    statesToSort = std::vector<uint_fast64_t>{0, 2, 1, 5, 6};
    sortedStates = order.sortStates(&statesToSort);
    EXPECT_EQ(5ul, sortedStates.size());

    itr = sortedStates.begin();
    EXPECT_EQ(0ul, *itr);
    EXPECT_EQ(2ul, *(++itr));
    EXPECT_EQ(1ul, *(++itr));
    EXPECT_EQ(7ul, *(++itr));
    EXPECT_EQ(7ul, *(++itr));
}
