#pragma once

#include <optional>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/decomposition/Decomposition.h"
#include "storm/storage/decomposition/StronglyConnectedComponent.h"
#include "storm/utility/OptionalRef.h"
namespace storm {

namespace storage {

template<typename ValueType>
class SparseMatrix;

struct StronglyConnectedComponentDecompositionOptions {
    /// Sets a bit vector indicating which subsystem to consider for the decomposition into SCCs.
    StronglyConnectedComponentDecompositionOptions& subsystem(storm::storage::BitVector const& subsystem);

    /// Sets a bit vector indicating which choices of the states are contained in the subsystem.
    StronglyConnectedComponentDecompositionOptions& choices(storm::storage::BitVector const& choices);

    /// Sets if trivial SCCs (i.e. SCCs consisting of just one state without a self-loop) are to be kept in the decomposition.
    StronglyConnectedComponentDecompositionOptions& dropNaiveSccs(bool value = true);

    /// Sets if only bottom SCCs, i.e. SCCs in which all states have no way of leaving the SCC), are kept.
    StronglyConnectedComponentDecompositionOptions& onlyBottomSccs(bool value = true);

    /// Enforces that the returned SCCs are sorted in a topological order.
    StronglyConnectedComponentDecompositionOptions& forceTopologicalSort(bool value = true);

    /// Sets if scc depths can be retrieved.
    StronglyConnectedComponentDecompositionOptions& computeSccDepths(bool value = true);

    storm::OptionalRef<storm::storage::BitVector const> optSubsystem;
    storm::OptionalRef<storm::storage::BitVector const> optChoices;
    bool areNaiveSccsDropped = false;
    bool areOnlyBottomSccsConsidered = false;
    bool isTopologicalSortForced = false;
    bool isComputeSccDepthsSet = false;
};

/*!
 * Holds temporary computation data used during the SCC decomposition algorithm.
 * Can be kept in memory to avoid re-allocations, which is useful if multiple SCC decompositions are computed (e.g. as part of an end component decomposition
 * algorithm).
 */
struct SccDecompositionMemoryCache {
    void initialize(uint64_t numStates);
    bool hasPreorderNumber(uint64_t stateIndex) const;
    std::vector<uint64_t> preorderNumbers, recursionStateStack, s, p;
};

/*!
 * Holds the result data of the implemented SCC decomposition algorithm.
 */
struct SccDecompositionResult {
    void initialize(uint64_t numStates, bool computeSccDepths);
    bool stateHasScc(uint64_t stateIndex) const;     /// True if an SCC is associated to the given state
    uint64_t sccCount;                               /// Number of found SCCs
    std::vector<uint64_t> stateToSccMapping;         /// Mapping from states to the SCC it belongs to
    storm::storage::BitVector nonTrivialStates;      /// Keep track of trivial states (singleton SCCs without selfloop).
    std::optional<std::vector<uint64_t>> sccDepths;  /// Holds SCC depths if requested. @see getSccDepth()
};

/*!
 * Computes an SCC decomposition for the given matrix and options.
 *
 * @note This method does initialize the given result data. This means that if multiple SCC decompositions (e.g. with different options) are computed, the
 * result memory can be re-used to avoid expensive reallocations.
 *
 * @param transitionMatrix transition matrix of the model
 * @param options options for the decomposition
 * @param result The resulting information will be stored into this struct.
 */
template<typename ValueType>
void performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, StronglyConnectedComponentDecompositionOptions const& options,
                             SccDecompositionResult& result);

/*!
 * Computes an SCC decomposition for the given matrix and options.
 *
 * @note This method does initialize the given result and cache data. This means that if multiple SCC decompositions (e.g. with different options) are computed,
 * the result and cache memory can be re-used to avoid expensive reallocations.
 *
 * @param transitionMatrix transition matrix of the model
 * @param options options for the decomposition
 * @param result The resulting information will be stored into this struct.
 * @param cache memory used by the underlying algorithm
 */
template<typename ValueType>
void performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, StronglyConnectedComponentDecompositionOptions const& options,
                             SccDecompositionResult& result, SccDecompositionMemoryCache& cache);

/*!
 * This class represents the decomposition of a graph-like structure into its strongly connected components.
 */
template<typename ValueType>
class StronglyConnectedComponentDecomposition : public Decomposition<StronglyConnectedComponent> {
   public:
    /*!
     * Creates an empty SCC decomposition.
     */
    StronglyConnectedComponentDecomposition();

    /*!
     * Creates an SCC decomposition of the given subsystem in the given system (whose transition relation is
     * given by a sparse matrix).
     *
     * @param transitionMatrix The transition matrix of the system to decompose.
     * @param options options for the decomposition

     */
    StronglyConnectedComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                            StronglyConnectedComponentDecompositionOptions const& options = StronglyConnectedComponentDecompositionOptions());

    /*!
     * Creates an SCC decomposition by copying the given SCC decomposition.
     *
     * @oaram other The SCC decomposition to copy.
     */
    StronglyConnectedComponentDecomposition(StronglyConnectedComponentDecomposition const& other);

    /*!
     * Assigns the contents of the given SCC decomposition to the current one by copying its contents.
     *
     * @oaram other The SCC decomposition from which to copy-assign.
     */
    StronglyConnectedComponentDecomposition& operator=(StronglyConnectedComponentDecomposition const& other);

    /*!
     * Creates an SCC decomposition by moving the given SCC decomposition.
     *
     * @oaram other The SCC decomposition to move.
     */
    StronglyConnectedComponentDecomposition(StronglyConnectedComponentDecomposition&& other);

    /*!
     * Assigns the contents of the given SCC decomposition to the current one by moving its contents.
     *
     * @oaram other The SCC decomposition from which to copy-assign.
     */
    StronglyConnectedComponentDecomposition& operator=(StronglyConnectedComponentDecomposition&& other);

    /*!
     * Retrieves whether SCCDepths have been computed during construction of this.
     */
    bool hasSccDepth() const;

    /*!
     * Gets the depth of the SCC with the given index. This is the number of different SCCs a path starting in the given SCC can reach.
     * E.g., bottom SCCs have depth 0, SCCs from which only bottom SCCs are reachable have depth 1, ...
     * This requires that SCCDepths are computed upon construction of this.
     * @param sccIndex The index of the SCC.
     */
    uint_fast64_t getSccDepth(uint_fast64_t const& sccIndex) const;

    /*!
     * Gets the maximum depth of an SCC.
     */
    uint_fast64_t getMaxSccDepth() const;

    /*!
     * Computes a vector that for each state has the index of the scc of that state in it.
     * If a state has no SCC in this decomposition (e.g. because we considered a subsystem), they will get SCC index std::numeric_limits<uint64_t>::max()
     *
     * @param numberOfStates the total number of states
     */
    std::vector<uint64_t> computeStateToSccIndexMap(uint64_t numberOfStates) const;

   private:
    /*
     * Performs the SCC decomposition of the given block in the given model. As a side-effect this fills
     * the vector of blocks of the decomposition.
     *
     * @param transitionMatrix The transition matrix of the system to decompose.
     */
    void performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                 StronglyConnectedComponentDecompositionOptions const& options);

    std::optional<std::vector<uint_fast64_t>> sccDepths;
};
}  // namespace storage
}  // namespace storm