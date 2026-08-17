#pragma once

#include <boost/container/flat_set.hpp>
#include <iostream>
#include <memory>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/StronglyConnectedComponent.h"

namespace storm {
namespace analysis {
class Order {
   public:
    /*!
     * Constants for comparison of nodes/states.
     */
    enum NodeComparison {
        UNKNOWN,
        BELOW,
        ABOVE,
        SAME,
    };

    /*!
     * Nodes of the Reachability Order. Contains all states with the same reachability.
     */
    struct Node {
        boost::container::flat_set<uint_fast64_t> states;
        storm::storage::BitVector statesAbove;
    };

    /*!
     * Constructs an order with the given top and bottom states.
     *
     * @param topStates A Bitvector with the top states of the resulting order.
     * @param bottomStates A Bitvector with the bottom states of the resulting order.
     * @param numberOfStates Maximum number of states in order.
     * @param statesSorted Pointer to a vector which contains the states which still need to added to the order.
     */
    Order(storm::storage::BitVector const& topStates, storm::storage::BitVector const& bottomStates, uint_fast64_t numberOfStates,
          storage::Decomposition<storage::StronglyConnectedComponent> sccsSorted, std::vector<uint_fast64_t> statesSorted);

    /*!
     * Constructs an order with the given top state and bottom state.
     *
     * @param top The top state of the resulting order.
     * @param bottom The bottom state of the resulting order.
     * @param numberOfStates Maximum number of states in order.
     * @param statesSorted Pointer to a vector which contains the states which still need to added to the order.
     */
    Order(uint_fast64_t top, uint_fast64_t bottom, uint_fast64_t numberOfStates, storage::Decomposition<storage::StronglyConnectedComponent> sccsSorted,
          std::vector<uint_fast64_t> statesSorted);

    /*!
     * Constructs a new Order.
     */
    Order();

    /*!
     * Adds state between the top and bottom node of the order.
     *
     * @param state The given state.
     */
    void add(uint_fast64_t state);

    /*!
     * Adds a node with the given state above the given node.
     *
     * @param state The state which is added.
     * @param node The pointer to the node above which the state is added, should not be nullptr.
     */
    void addAbove(uint_fast64_t state, Node* node);

    /*!
     * Adds a node with the given state below the given node.
     *
     * @param state The state which is added.
     * @param node The pointer to the node below which the state is added, should not be nullptr.
     */
    void addBelow(uint_fast64_t state, Node* node);

    /*!
     * Adds a node with the given state below node1 and above node2.
     *
     * @param state The given state.
     * @param node1 The pointer to the node below which a new node (with state) is added.
     * @param node2 The pointer to the node above which a new node (with state) is added.
     */
    void addBetween(uint_fast64_t state, Node* node1, Node* node2);

    /*!
     * Adds a node with the given state between the nodes of below and above.
     * Result: below -> state -> above
     *
     * @param state The given state.
     * @param above The state number of the state below which a new node (with state) is added.
     * @param below The state number of the state above which a new node (with state) is added.
     */
    void addBetween(uint_fast64_t state, uint_fast64_t above, uint_fast64_t below);

    /*!
     * Adds a new relation between two nodes to the order.
     *
     * @param above The node closest to the top Node of the Order.
     * @param below The node closest to the bottom Node of the Order.
     */
    void addRelationNodes(storm::analysis::Order::Node* above, storm::analysis::Order::Node* below, bool allowMerge = false);

    /*!
     * Adds a new relation between two states to the order.
     *
     * @param above The state closest to the top Node of the Order.
     * @param below The state closest to the bottom Node of the Order.
     */
    void addRelation(uint_fast64_t above, uint_fast64_t below, bool allowMerge = false);

    /*!
     * Adds state to the states of the given node.
     *
     * @param state The state which is added.
     * @param node The pointer to the node to which state is added, should not be nullptr.
     */
    void addToNode(uint_fast64_t state, Node* node);

    /*!
     * Merges node2 into node1.
     * @return false when merging leads to invalid order
     */
    bool mergeNodes(Node* node1, Node* node2);

    /*!
     * Merges node of var2 into node of var1.
     * @return false when merging leads to invalid order
     */
    bool merge(uint_fast64_t var1, uint_fast64_t var2);

    /*!
     * Compares the level of the nodes of the states.
     * Behaviour unknown when one or more of the states does not occur at any Node in the Order.
     *
     * @param State1 the first state.
     * @param State2 the second state.
     * @return SAME if the nodes are on the same level;
     *         ABOVE if the node of the first state is closer to top than the node of the second state;
     *         BELOW if the node of the second state is closer to top than the node of the first state;
     *         UNKNOWN if it is unclear from the structure of the order how the nodes relate.
     */
    Order::NodeComparison compare(uint_fast64_t state1, uint_fast64_t state2, NodeComparison hypothesis = UNKNOWN);
    Order::NodeComparison compareFast(uint_fast64_t state1, uint_fast64_t state2, NodeComparison hypothesis = UNKNOWN) const;

    /*!
     * Compares the level of the two nodes.
     *
     * @param node1 The first node.
     * @param node2 The second node.
     * @return SAME if the nodes are on the same level;
     *         ABOVE if node1 is closer to top than node2;
     *         BELOW if node2 is closer to top than node1;
     *         UNKNOWN if it is unclear from the structure of the order how the nodes relate.
     */
    NodeComparison compare(Node* node1, Node* node2, NodeComparison hypothesis = UNKNOWN);
    NodeComparison compareFast(Node* node1, Node* node2, NodeComparison hypothesis = UNKNOWN) const;

    /*!
     * Check if state is already contained in order.
     */
    bool contains(uint_fast64_t state) const;

    /*!
     * Retrieves the bottom node of the order.
     *
     * @return The bottom node.
     */
    Node* getBottom() const;

    /*!
     * Returns true if done building the order.
     */
    bool getDoneBuilding() const;

    /*!
     * Returns the next done state of the order, returns the number of state if end of done states is reached.
     */
    uint_fast64_t getNextDoneState(uint_fast64_t state) const;

    uint_fast64_t getNumberOfDoneStates() const;

    /*!
     * Retrieves the pointer to a Node at which the state occurs.
     *
     * @param state The number of the state.
     * @return The pointer to the node of the state, nullptr if the node does not exist.
     */
    Node* getNode(uint_fast64_t state) const;

    /*!
     * Returns the vector with the nodes of the order.
     * Each index in the vector refers to a state.
     * When the state is not yet added at a node, it will contain the nullptr.
     *
     * @return The vector with nodes of the order.
     */
    std::vector<Node*> getNodes() const;

    std::vector<uint_fast64_t>& getStatesSorted();
    /*!
     * Retrieves the top node of the order.
     *
     * @return The top node.
     */
    Node* getTop() const;

    /*!
     * Returns the number of added states.
     */
    uint_fast64_t getNumberOfAddedStates() const;

    /*!
     * Returns the number of possible states in the order.
     */
    uint_fast64_t getNumberOfStates() const;

    /*!
     * Checks if the given state is a bottom state.
     */
    bool isBottomState(uint_fast64_t) const;

    /*!
     * Checks if the given state is a top state.
     */
    bool isTopState(uint_fast64_t) const;

    /*!
     * Returns if the order only consists of bottom and top states (so no in-between nodes).
     */
    bool isOnlyBottomTopOrder() const;

    /*!
     * Sorts the given states if possible.
     *
     * @param states Vector of the states to be sorted.
     * @return Vector with states sorted, length equals number of states to sort.
     * If states cannot be sorted, last state of the vector will always equal the length of the BitVector.
     */
    std::vector<uint_fast64_t> sortStates(std::vector<uint_fast64_t>* states);
    std::pair<bool, bool> allAboveBelow(std::vector<uint_fast64_t> const states, uint_fast64_t state);

    /*!
     * Sorts the given states if possible.
     *
     * @param states Vector of the states to be sorted.
     * @return s1, s2, vector
     * if s1 == numberOfSTates, all states could be sorted including current
     * if s1 < numberOfStates && s2 == numberOfStates, all states excluding s1 could be sorted, forward reasonging can be continued
     * else assumption is needed
     */
    std::pair<std::pair<uint_fast64_t, uint_fast64_t>, std::vector<uint_fast64_t>> sortStatesForForward(uint_fast64_t currentState,
                                                                                                        std::vector<uint_fast64_t> const& successors);

    /*!
     * Sorts the given states if possible.
     *
     * @param states Vector of the states to be sorted.
     * @return pair of unsortabe states, vector with states sorted (so far).
     * If all states could be sorted, both values of the pair are numberOfStates and the vectors length will equal the number of states to sort.
     */
    std::pair<std::pair<uint_fast64_t, uint_fast64_t>, std::vector<uint_fast64_t>> sortStatesUnorderedPair(const std::vector<uint_fast64_t>* states);

    /*!
     * Sorts the given states if possible.
     *
     * @param states Bitvector of the states to be sorted.
     * @return vector with states sorted, length equals number of states to sort.
     * If states cannot be sorted, last state of the vector will always equal the length of the BitVector.
     */
    std::vector<uint_fast64_t> sortStates(storm::storage::BitVector* states);

    bool isTrivial(uint_fast64_t state);
    std::pair<uint_fast64_t, bool> getNextStateNumber();

    //                    bool existsNextSCC();
    bool existsNextState();
    bool existsStateToHandle();

    std::pair<uint_fast64_t, bool> getStateToHandle();

    void addStateToHandle(uint_fast64_t state);
    void addStateSorted(uint_fast64_t state);

    /*!
     * Prints the dot output to stdout.
     */
    void toDotOutput() const;

    /*!
     * Writes dotoutput to the given file.
     *
     * @param dotOutfile
     */
    void dotOutputToFile(std::ofstream& dotOutfile) const;

    /*!
     * Creates a copy of the calling Order.
     *
     * @return Pointer to the copy.
     */
    std::shared_ptr<Order> copy() const;
    //                    void setAddedSCC(uint_fast64_t sccNumber);
    void setDoneState(uint_fast64_t sccNumber);

    bool isInvalid() const;

   private:
    bool above(Node* node1, Node* node2);

    bool aboveFast(Node* node1, Node* node2) const;

    void init(uint_fast64_t numberOfStates, storage::Decomposition<storage::StronglyConnectedComponent>, bool doneBuilding = false);

    std::string nodeName(Node n) const;

    std::string nodeLabel(Node n) const;

    bool invalid;

    bool doneBuilding;

    bool onlyBottomTopOrder;

    storm::storage::BitVector doneStates;
    storm::storage::BitVector trivialStates;

    std::vector<Node*> nodes;

    std::vector<uint_fast64_t> statesToHandle;

    Node* top;

    Node* bottom;

    uint_fast64_t numberOfStates;

    uint_fast64_t numberOfAddedStates;

    std::vector<uint_fast64_t> statesSorted;
};
}  // namespace analysis
}  // namespace storm
