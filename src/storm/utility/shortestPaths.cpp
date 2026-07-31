#include "storm/utility/shortestPaths.h"

#include <ostream>
#include <queue>
#include <set>
#include <string>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Model.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

// FIXME: I've accidentally used k=0 *twice* now without realizing that k>=1 is required!
// (Also, did I document this? I think so, somewhere. I went with k>=1 because
// that's what the KSP paper used, but in retrospect k>=0 seems more intuitive ...)

namespace storm {
namespace utility {
namespace ksp {
template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(storage::SparseMatrix<T> const& transitionMatrix, std::unordered_map<state_t, T> const& targetProbMap,
                                                  BitVector const& initialStates, MatrixFormat matrixFormat)
    : transitionMatrix(transitionMatrix),
      numStates(transitionMatrix.getColumnCount() + 1),  // one more for meta-target
      metaTarget(transitionMatrix.getColumnCount()),     // first unused state index
      initialStates(initialStates),
      targetProbMap(targetProbMap),
      matrixFormat(matrixFormat) {
    computePredecessors();

    // gives us SP-predecessors, SP-distances
    performDijkstra();

    computeSPSuccessors();

    // constructs the recursive shortest path representations
    initializeShortestPaths();

    candidatePaths.resize(numStates);
}

template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(storage::SparseMatrix<T> const& transitionMatrix, std::vector<T> const& targetProbVector,
                                                  BitVector const& initialStates, MatrixFormat matrixFormat)
    : ShortestPathsGenerator<T>(transitionMatrix, vectorToMap(targetProbVector), initialStates, matrixFormat) {}

// extracts the relevant info from the model and delegates to ctor above
template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(Model const& model, BitVector const& targetBV)
    : ShortestPathsGenerator<T>(model.getTransitionMatrix(), allProbOneMap(targetBV), model.getInitialStates(), MatrixFormat::straight) {}

// several alternative ways to specify the targets are provided,
// each converts the targets and delegates to the ctor above
// I admit it's kind of ugly, but actually pretty convenient (and I've wanted to try C++11 delegation)
template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(Model const& model, state_t singleTarget)
    : ShortestPathsGenerator<T>(model, std::vector<state_t>{singleTarget}) {}

template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(Model const& model, std::vector<state_t> const& targetList)
    : ShortestPathsGenerator<T>(model, BitVector(model.getNumberOfStates(), targetList)) {}

template<typename T>
ShortestPathsGenerator<T>::ShortestPathsGenerator(Model const& model, std::string const& targetLabel)
    : ShortestPathsGenerator<T>(model, model.getStates(targetLabel)) {}

template<typename T>
T ShortestPathsGenerator<T>::getDistance(unsigned long k) {
    computeKSP(k);
    return kShortestPaths[metaTarget][k - 1].distance;
}

template<typename T>
BitVector ShortestPathsGenerator<T>::getStates(unsigned long k) {
    computeKSP(k);
    BitVector stateSet(numStates - 1, false);  // no meta-target

    Path<T> currentPath = kShortestPaths[metaTarget][k - 1];
    boost::optional<state_t> maybePredecessor = currentPath.predecessorNode;
    // this omits the first node, which is actually convenient since that's the meta-target

    while (maybePredecessor) {
        state_t predecessor = maybePredecessor.get();
        stateSet.set(predecessor, true);

        currentPath = kShortestPaths[predecessor][currentPath.predecessorK - 1];  // god damn you, index
        maybePredecessor = currentPath.predecessorNode;
    }

    return stateSet;
}

template<typename T>
std::vector<state_t> ShortestPathsGenerator<T>::getPathAsList(unsigned long k) {
    computeKSP(k);

    std::vector<state_t> backToFrontList;

    Path<T> currentPath = kShortestPaths[metaTarget][k - 1];
    boost::optional<state_t> maybePredecessor = currentPath.predecessorNode;
    // this omits the first node, which is actually convenient since that's the meta-target

    while (maybePredecessor) {
        state_t predecessor = maybePredecessor.get();
        backToFrontList.push_back(predecessor);

        currentPath = kShortestPaths[predecessor][currentPath.predecessorK - 1];
        maybePredecessor = currentPath.predecessorNode;
    }

    return backToFrontList;
}

template<typename T>
void ShortestPathsGenerator<T>::computePredecessors() {
    assert(transitionMatrix.hasTrivialRowGrouping());

    // one more for meta-target
    graphPredecessors.resize(numStates);

    for (state_t i = 0; i < numStates - 1; i++) {
        for (auto const& transition : transitionMatrix.getRowGroup(i)) {
            // to avoid non-minimal paths, the meta-target-predecessors are
            // *not* predecessors of any state but the meta-target
            if (!isMetaTargetPredecessor(i)) {
                graphPredecessors[transition.getColumn()].push_back(i);
            }
        }
    }

    // meta-target has exactly the meta-target-predecessors as predecessors
    // (duh. note that the meta-target-predecessors used to be called target,
    // but that's not necessarily true in the matrix/value invocation case)
    for (auto targetProbPair : targetProbMap) {
        graphPredecessors[metaTarget].push_back(targetProbPair.first);
    }
}

template<typename T>
void ShortestPathsGenerator<T>::performDijkstra() {
    // the existing Dijkstra isn't working anyway AND
    // doesn't fully meet our requirements, so let's roll our own

    T inftyDistance = zero<T>();
    T zeroDistance = one<T>();
    shortestPathDistances.resize(numStates, inftyDistance);
    shortestPathPredecessors.resize(numStates, boost::optional<state_t>());

    // set serves as priority queue with unique membership
    // default comparison on pair actually works fine if distance is the first entry
    std::set<std::pair<T, state_t>, std::greater<std::pair<T, state_t>>> dijkstraQueue;

    for (state_t initialState : initialStates) {
        shortestPathDistances[initialState] = zeroDistance;
        dijkstraQueue.emplace(zeroDistance, initialState);
    }

    while (!dijkstraQueue.empty()) {
        state_t currentNode = (*dijkstraQueue.begin()).second;
        dijkstraQueue.erase(dijkstraQueue.begin());

        if (!isMetaTargetPredecessor(currentNode)) {
            // non-target node, treated normally
            for (auto const& transition : transitionMatrix.getRowGroup(currentNode)) {
                state_t otherNode = transition.getColumn();

                // note that distances are probabilities, thus they are multiplied and larger is better
                T alternateDistance = shortestPathDistances[currentNode] * convertDistance(currentNode, otherNode, transition.getValue());
                assert((zero<T>() <= alternateDistance) && (alternateDistance <= one<T>()));
                if (alternateDistance > shortestPathDistances[otherNode]) {
                    shortestPathDistances[otherNode] = alternateDistance;
                    shortestPathPredecessors[otherNode] = boost::optional<state_t>(currentNode);
                    dijkstraQueue.emplace(alternateDistance, otherNode);
                }
            }
        } else {
            // node only has one "virtual edge" (with prob as per targetProbMap) to meta-target
            T alternateDistance = shortestPathDistances[currentNode] * targetProbMap[currentNode];
            if (alternateDistance > shortestPathDistances[metaTarget]) {
                shortestPathDistances[metaTarget] = alternateDistance;
                shortestPathPredecessors[metaTarget] = boost::optional<state_t>(currentNode);
            }
            // no need to enqueue meta-target
        }
    }
}

template<typename T>
void ShortestPathsGenerator<T>::computeSPSuccessors() {
    shortestPathSuccessors.resize(numStates);

    for (state_t i = 0; i < numStates; i++) {
        if (shortestPathPredecessors[i]) {
            state_t predecessor = shortestPathPredecessors[i].get();
            shortestPathSuccessors[predecessor].push_back(i);
        }
    }
}

template<typename T>
void ShortestPathsGenerator<T>::initializeShortestPaths() {
    kShortestPaths.resize(numStates);

    // BFS in Dijkstra-SP order
    std::queue<state_t> bfsQueue;
    for (state_t initialState : initialStates) {
        bfsQueue.push(initialState);
    }

    while (!bfsQueue.empty()) {
        state_t currentNode = bfsQueue.front();
        bfsQueue.pop();

        if (!kShortestPaths[currentNode].empty()) {
            continue;  // already visited
        }

        for (state_t successorNode : shortestPathSuccessors[currentNode]) {
            bfsQueue.push(successorNode);
        }

        // note that `shortestPathPredecessor` may not be present
        // if current node is an initial state
        // in this case, the boost::optional copy of an uninitialized optional is hopefully also uninitialized
        kShortestPaths[currentNode].push_back(Path<T>{shortestPathPredecessors[currentNode], 1, shortestPathDistances[currentNode]});
    }
}

template<typename T>
T ShortestPathsGenerator<T>::getEdgeDistance(state_t tailNode, state_t headNode) const {
    // just to be clear, head is where the arrow points (obviously)
    if (headNode != metaTarget) {
        // edge is "normal", not to meta-target

        for (auto const& transition : transitionMatrix.getRowGroup(tailNode)) {
            if (transition.getColumn() == headNode) {
                return convertDistance(tailNode, headNode, transition.getValue());
            }
        }

        // there is no such edge
        // let's disallow that for now, because I'm not expecting it to happen
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Should not happen.");
    } else {
        // edge must be "virtual edge" to meta-target
        assert(isMetaTargetPredecessor(tailNode));
        return targetProbMap.at(tailNode);
    }
}

template<typename T>
void ShortestPathsGenerator<T>::computeNextPath(state_t node, unsigned long k) {
    assert(k >= 2);                                // Dijkstra is used for k=1
    assert(kShortestPaths[node].size() == k - 1);  // if not, the previous SP must not exist

    // TODO: I could extract the candidate generation to make this function more succinct
    if (k == 2) {
        // Step B.1 in J&M paper

        Path<T> shortestPathToNode = kShortestPaths[node][1 - 1];  // never forget index shift :-|

        for (state_t predecessor : graphPredecessors[node]) {
            // add shortest paths to predecessors plus edge to current node
            Path<T> pathToPredecessorPlusEdge = {boost::optional<state_t>(predecessor), 1,
                                                 shortestPathDistances[predecessor] * getEdgeDistance(predecessor, node)};
            candidatePaths[node].insert(pathToPredecessorPlusEdge);

            // ... but not the actual shortest path
            auto it = std::find(candidatePaths[node].begin(), candidatePaths[node].end(), shortestPathToNode);
            if (it != candidatePaths[node].end()) {
                candidatePaths[node].erase(it);
            }
        }
    }

    if (not(k == 2 && isInitialState(node))) {
        // Steps B.2-5 in J&M paper

        // the (k-1)th shortest path (i.e., one better than the one we want to compute)
        Path<T> previousShortestPath = kShortestPaths[node][k - 1 - 1];  // oh god, I forgot index shift AGAIN

        // the predecessor node on that path
        state_t predecessor = previousShortestPath.predecessorNode.get();
        // the path to that predecessor was the `tailK`-shortest
        unsigned long tailK = previousShortestPath.predecessorK;

        // i.e. source ~~tailK-shortest path~~> predecessor --> node

        // compute one-worse-shortest path to the predecessor (if it hasn't yet been computed)
        if (kShortestPaths[predecessor].size() < tailK + 1) {
            // TODO: investigate recursion depth and possible iterative alternative
            computeNextPath(predecessor, tailK + 1);
        }

        if (kShortestPaths[predecessor].size() >= tailK + 1) {
            // take that path, add an edge to the current node; that's a candidate
            Path<T> pathToPredecessorPlusEdge = {boost::optional<state_t>(predecessor), tailK + 1,
                                                 kShortestPaths[predecessor][tailK + 1 - 1].distance * getEdgeDistance(predecessor, node)};
            candidatePaths[node].insert(pathToPredecessorPlusEdge);
        }
        // else there was no path; TODO: does this need handling? -- yes, but not here (because the step B.1 may have added candidates)
    }

    // Step B.6 in J&M paper
    if (!candidatePaths[node].empty()) {
        Path<T> minDistanceCandidate = *(candidatePaths[node].begin());
        for (auto path : candidatePaths[node]) {
            if (path.distance > minDistanceCandidate.distance) {
                minDistanceCandidate = path;
            }
        }

        candidatePaths[node].erase(std::find(candidatePaths[node].begin(), candidatePaths[node].end(), minDistanceCandidate));
        kShortestPaths[node].push_back(minDistanceCandidate);
    } else {
        // TODO: kSP does not exist. this is handled later, but it would be nice to catch it as early as possble, wouldn't it?
        STORM_LOG_TRACE("KSP: no candidates, this will trigger nonexisting ksp after exiting these recursions. TODO: handle here");
    }
}

template<typename T>
void ShortestPathsGenerator<T>::computeKSP(unsigned long k) {
    STORM_LOG_THROW(k != 0, storm::exceptions::InvalidArgumentException, "Index 0 is invalid, since we use 1-based indices!");

    unsigned long alreadyComputedK = kShortestPaths[metaTarget].size();

    for (unsigned long nextK = alreadyComputedK + 1; nextK <= k; nextK++) {
        computeNextPath(metaTarget, nextK);
        if (kShortestPaths[metaTarget].size() < nextK) {
            unsigned long lastExistingK = nextK - 1;
            STORM_LOG_DEBUG("KSP throws (as expected) due to nonexistence -- maybe this is unhandled and causes the Python interface to segfault?");
            STORM_LOG_DEBUG("last existing k-SP has k=" + std::to_string(lastExistingK));
            STORM_LOG_DEBUG("maybe this is unhandled and causes the Python interface to segfault?");
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "k-SP does not exist for k=" << k);
        }
    }
}

template<typename T>
void ShortestPathsGenerator<T>::printKShortestPath(state_t targetNode, unsigned long k, bool head) const {
    // note the index shift! risk of off-by-one
    Path<T> p = kShortestPaths[targetNode][k - 1];

    if (head) {
        std::cout << "Path (reversed";
        if (targetNode == metaTarget) {
            std::cout << ", w/ meta-target";
        }
        std::cout << "), dist (prob)=" << p.distance << ": [";
    }

    std::cout << " " << targetNode;

    if (p.predecessorNode) {
        printKShortestPath(p.predecessorNode.get(), p.predecessorK, false);
    } else {
        std::cout << " ]\n";
    }
}

// only prints the info stored in the Path struct;
// does not traverse the actual path (see printKShortestPath for that)
template<typename T>
std::ostream& operator<<(std::ostream& out, Path<T> const& p) {
    out << "Path with predecessorNode: " << ((p.predecessorNode) ? std::to_string(p.predecessorNode.get()) : "None");
    out << " predecessorK: " << p.predecessorK << " distance: " << p.distance;
    return out;
}
template std::ostream& operator<<(std::ostream& out, Path<double> const& p);

template class ShortestPathsGenerator<double>;
template class ShortestPathsGenerator<storm::RationalNumber>;
}  // namespace ksp
}  // namespace utility
}  // namespace storm
