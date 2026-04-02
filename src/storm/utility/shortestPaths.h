#ifndef STORM_UTIL_SHORTESTPATHS_H_
#define STORM_UTIL_SHORTESTPATHS_H_

#include <algorithm>
#include <boost/optional/optional.hpp>
#include <cassert>
#include <iosfwd>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "constants.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace storage {
template<typename ValueType>
class SparseMatrix;

namespace sparse {
typedef uint_fast64_t state_type;
}
}  // namespace storage

namespace models {
namespace sparse {
template<typename ValueType>
class StandardRewardModel;

template<class CValueType, class CRewardModelType>
class Model;
}  // namespace sparse
}  // namespace models

namespace utility {
namespace ksp {
using state_t = storage::sparse::state_type;
using OrderedStateList = std::vector<state_t>;
using BitVector = storage::BitVector;

// -- helper structs/classes -----------------------------------------------------------------------------

template<typename T>
struct Path {
    boost::optional<state_t> predecessorNode;
    unsigned long predecessorK;
    T distance;

    // arbitrary order for std::set
    bool operator<(const Path<T>& rhs) const {
        if (predecessorNode != rhs.predecessorNode) {
            return predecessorNode < rhs.predecessorNode;
        }
        return predecessorK < rhs.predecessorK;
    }

    bool operator==(const Path<T>& rhs) const {
        return (predecessorNode == rhs.predecessorNode) && (predecessorK == rhs.predecessorK);
    }
};

template<typename T>
std::ostream& operator<<(std::ostream& out, Path<T> const& p);

// when using the raw matrix/vector invocation, this enum parameter
// forces the caller to declare whether the matrix has the evil I-P
// format, which requires back-conversion of the entries
enum class MatrixFormat { straight, iMinusP };

// -------------------------------------------------------------------------------------------------------

template<typename T>
class ShortestPathsGenerator {
   public:
    using Matrix = storage::SparseMatrix<T>;
    using StateProbMap = std::unordered_map<state_t, T>;
    using Model = models::sparse::Model<T, models::sparse::StandardRewardModel<T>>;

    /*!
     * Performs precomputations (including meta-target insertion and Dijkstra).
     * Modifications are done locally, `model` remains unchanged.
     * Target (group) cannot be changed.
     */
    ShortestPathsGenerator(Model const& model, BitVector const& targetBV);

    // allow alternative ways of specifying the target,
    // all of which will be converted to BitVector and delegated to constructor above
    ShortestPathsGenerator(Model const& model, state_t singleTarget);
    ShortestPathsGenerator(Model const& model, std::vector<state_t> const& targetList);
    ShortestPathsGenerator(Model const& model, std::string const& targetLabel = "target");

    // a further alternative: use transition matrix of maybe-states
    // combined with target vector (e.g., the instantiated matrix/vector from SamplingModel);
    // in this case separately specifying a target makes no sense
    ShortestPathsGenerator(Matrix const& transitionMatrix, std::vector<T> const& targetProbVector, BitVector const& initialStates, MatrixFormat matrixFormat);
    ShortestPathsGenerator(Matrix const& maybeTransitionMatrix, StateProbMap const& targetProbMap, BitVector const& initialStates, MatrixFormat matrixFormat);

    inline ~ShortestPathsGenerator() {}

    /*!
     * Returns distance (i.e., probability) of the KSP.
     * Computes KSP if not yet computed.
     * @throws std::invalid_argument if no such k-shortest path exists
     */
    T getDistance(unsigned long k);

    /*!
     * Returns the states that occur in the KSP.
     * For a path-traversal (in order and with duplicates), see `getPathAsList`.
     * Computes KSP if not yet computed.
     * @throws std::invalid_argument if no such k-shortest path exists
     */
    storage::BitVector getStates(unsigned long k);

    /*!
     * Returns the states of the KSP as back-to-front traversal.
     * Computes KSP if not yet computed.
     * @throws std::invalid_argument if no such k-shortest path exists
     */
    OrderedStateList getPathAsList(unsigned long k);

   private:
    Matrix const& transitionMatrix;
    state_t numStates;  // includes meta-target, i.e. states in model + 1
    state_t metaTarget;
    BitVector initialStates;
    StateProbMap targetProbMap;

    MatrixFormat matrixFormat;

    std::vector<OrderedStateList> graphPredecessors;
    std::vector<boost::optional<state_t>> shortestPathPredecessors;
    std::vector<OrderedStateList> shortestPathSuccessors;
    std::vector<T> shortestPathDistances;

    std::vector<std::vector<Path<T>>> kShortestPaths;
    std::vector<std::set<Path<T>>> candidatePaths;

    /*!
     * Computes list of predecessors for all nodes.
     * Reachability is not considered; a predecessor is simply any node that has an edge leading to the node in question.
     * Requires `transitionMatrix`.
     * Modifies `graphPredecessors`.
     */
    void computePredecessors();

    /*!
     * Computes shortest path distances and predecessors.
     * Requires `model`, `numStates`, `transitionMatrix`.
     * Modifies `shortestPathPredecessors` and `shortestPathDistances`.
     */
    void performDijkstra();

    /*!
     * Computes list of shortest path successor nodes from predecessor list.
     * Requires `shortestPathPredecessors`, `numStates`.
     * Modifies `shortestPathSuccessors`.
     */
    void computeSPSuccessors();

    /*!
     * Constructs and stores the implicit shortest path representations (see `Path`) for the (1-)shortest paths.
     * Requires `shortestPathPredecessors`, `shortestPathDistances`, `model`, `numStates`.
     * Modifies `kShortestPaths`.
     */
    void initializeShortestPaths();

    /*!
     * Main step of REA algorithm. TODO: Document further.
     */
    void computeNextPath(state_t node, unsigned long k);

    /*!
     * Computes k-shortest path if not yet computed.
     * @throws std::invalid_argument if no such k-shortest path exists
     */
    void computeKSP(unsigned long k);

    /*!
     * Recurses over the path and prints the nodes. Intended for debugging.
     */
    void printKShortestPath(state_t targetNode, unsigned long k, bool head = true) const;

    /*!
     * Returns actual distance for real edges, 1 for edges to meta-target.
     */
    T getEdgeDistance(state_t tailNode, state_t headNode) const;

    // --- tiny helper fcts ---

    inline bool isInitialState(state_t node) const {
        return std::find(initialStates.begin(), initialStates.end(), node) != initialStates.end();
    }

    inline bool isMetaTargetPredecessor(state_t node) const {
        return targetProbMap.count(node) == 1;
    }

    // I dislike this. But it is necessary if we want to handle those ugly I-P matrices
    inline T convertDistance(state_t tailNode, state_t headNode, T distance) const {
        if (matrixFormat == MatrixFormat::straight) {
            return distance;
        } else {
            if (tailNode == headNode) {
                // diagonal: 1-p = dist
                return one<T>() - distance;
            } else {
                // non-diag: -p = dist
                return zero<T>() - distance;
            }
        }
    }

    /*!
     * Returns a map where each state of the input BitVector is mapped to 1 (`one<T>`).
     */
    inline StateProbMap allProbOneMap(BitVector bitVector) const {
        StateProbMap stateProbMap;
        for (state_t node : bitVector) {
            stateProbMap.emplace(node, one<T>());  // FIXME check rvalue warning (here and below)
        }
        return stateProbMap;
    }

    /*!
     * Given a vector of probabilities so that the `i`th entry corresponds to the
     * probability of state `i`, returns an equivalent map of only the non-zero entries.
     */
    inline std::unordered_map<state_t, T> vectorToMap(std::vector<T> probVector) const {
        // assert(probVector.size() == numStates); // numStates may not yet be initialized! // still true?

        std::unordered_map<state_t, T> stateProbMap;

        for (state_t i = 0; i < probVector.size(); i++) {
            T probEntry = probVector[i];

            // only non-zero entries (i.e. true transitions) are added to the map
            if (probEntry != 0) {
                assert(0 < probEntry);
                assert(probEntry <= 1);
                stateProbMap.emplace(i, probEntry);
            }
        }

        return stateProbMap;
    }

    // -----------------------
};
}  // namespace ksp
}  // namespace utility
}  // namespace storm

#endif  // STORM_UTIL_SHORTESTPATHS_H_
