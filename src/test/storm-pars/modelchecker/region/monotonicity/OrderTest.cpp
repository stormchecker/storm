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
