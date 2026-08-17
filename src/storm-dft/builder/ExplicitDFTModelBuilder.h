#pragma once

#include <boost/optional/optional.hpp>
#include <limits>
#include <stack>
#include <unordered_set>

#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/StateStorage.h"

#include "storm-dft/builder/DftExplorationHeuristic.h"
#include "storm-dft/generator/DftNextStateGenerator.h"
#include "storm-dft/storage/BucketPriorityQueue.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/DftSymmetries.h"

namespace storm::dft {
namespace builder {

/*!
 * Build a Markov chain from DFT.
 */
template<typename ValueType, typename StateType = uint32_t>
class ExplicitDFTModelBuilder {
    using DFTStatePointer = std::shared_ptr<storm::dft::storage::DFTState<ValueType>>;
    using ExplorationHeuristic = DFTExplorationHeuristic<ValueType>;
    using ExplorationHeuristicPointer = std::shared_ptr<ExplorationHeuristic>;

    // A structure holding the individual components of a model.
    struct ModelComponents {
        // Constructor
        ModelComponents();

        // The transition matrix.
        storm::storage::SparseMatrix<ValueType> transitionMatrix;

        // The state labeling.
        storm::models::sparse::StateLabeling stateLabeling;

        // The Markovian states.
        storm::storage::BitVector markovianStates;

        // The exit rates.
        std::vector<ValueType> exitRates;

        // A vector that stores a labeling for each choice.
        boost::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;

        // A flag indicating if the model is deterministic.
        bool deterministicModel;
    };

    // A class holding the information for building the transition matrix.
    class MatrixBuilder {
       public:
        // Constructor
        MatrixBuilder(bool canHaveNondeterminism);

        /*!
         * Set a mapping from a state id to the index in the matrix.
         *
         * @param id Id of the state.
         */
        void setRemapping(StateType id) {
            STORM_LOG_ASSERT(id < stateRemapping.size(), "Invalid index for remapping.");
            stateRemapping[id] = currentRowGroup;
        }

        /*!
         * Create a new row group if the model is nondeterministic.
         */
        void newRowGroup() {
            if (canHaveNondeterminism) {
                builder.newRowGroup(currentRow);
            }
            ++currentRowGroup;
        }

        /*!
         * Add a transition from the current row.
         *
         * @param index Target index
         * @param value Value of transition
         */
        void addTransition(StateType index, ValueType value) {
            builder.addNextValue(currentRow, index, value);
        }

        /*!
         * Finish the current row.
         */
        void finishRow() {
            ++currentRow;
        }

        /*!
         * Remap the columns in the matrix.
         */
        void remap() {
            builder.replaceColumns(stateRemapping, mappingOffset);
        }

        /*!
         * Get the current row group.
         *
         * @return The current row group.
         */
        StateType getCurrentRowGroup() {
            return currentRowGroup;
        }

        /*!
         * Get the remapped state for the given id.
         *
         * @param id State.
         *
         * @return Remapped index.
         */
        StateType getRemapping(StateType id) {
            STORM_LOG_ASSERT(id < stateRemapping.size(), "Invalid index for remapping.");
            return stateRemapping[id];
        }

        // Matrix builder.
        storm::storage::SparseMatrixBuilder<ValueType> builder;

        // Offset to distinguish states which will not be remapped anymore and those which will.
        size_t mappingOffset;

        // A mapping from state ids to the row group indices in which they actually reside.
        // TODO: avoid hack with fixed int type
        std::vector<uint_fast64_t> stateRemapping;

       private:
        // Index of the current row group.
        StateType currentRowGroup;

        // Index of the current row.
        StateType currentRow;

        // Flag indicating if row groups are needed.
        bool canHaveNondeterminism;
    };

   public:
    /*!
     * Constructor.
     *
     * @param dft DFT.
     * @param symmetries Symmetries in the dft.
     */
    ExplicitDFTModelBuilder(storm::dft::storage::DFT<ValueType> const& dft, storm::dft::storage::DftSymmetries const& symmetries);

    /*!
     * Build model from DFT.
     *
     * @param iteration Current number of iteration.
     * @param approximationThreshold Threshold determining when to skip exploring states.
     * @param approximationHeuristic Heuristic used for exploring states.
     */
    void buildModel(size_t iteration, double approximationThreshold = 0.0,
                    storm::dft::builder::ApproximationHeuristic approximationHeuristic = storm::dft::builder::ApproximationHeuristic::DEPTH);

    /*!
     * Get the built model.
     *
     * @return The model built from the DFT.
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType>> getModel();

    /*!
     * Get the built approximation model for either the lower or upper bound.
     *
     * @param lowerBound   If true, the lower bound model is returned, else the upper bound model
     * @param expectedTime If true, the bounds for expected time are computed, else the bounds for probabilities.
     *
     * @return The model built from the DFT.
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType>> getModelApproximation(bool lowerBound, bool expectedTime);

   private:
    /*!
     * Explore state space of DFT.
     *
     * @param approximationThreshold Threshold to determine when to skip states.
     */
    void exploreStateSpace(double approximationThreshold);

    /*!
     * Initialize the matrix for a refinement iteration.
     */
    void initializeNextIteration();

    /*!
     * Build the labeling.
     */
    void buildLabeling();

    /*!
     * Add a state to the explored states (if not already there). It also handles pseudo states.
     *
     * @param state The state to add.
     *
     * @return Id of state.
     */
    StateType getOrAddStateIndex(DFTStatePointer const& state);

    /*!
     * Set markovian flag for the current state.
     *
     * @param markovian Flag indicating if the state is markovian.
     */
    void setMarkovian(bool markovian);

    /**
     * Change matrix to reflect the lower or upper approximation bound.
     *
     * @param matrix       Matrix to change. The change are reflected here.
     * @param lowerBound   Flag indicating if the lower bound should be used. Otherwise the upper bound is used.
     */
    void changeMatrixBound(storm::storage::SparseMatrix<ValueType>& matrix, bool lowerBound) const;

    /*!
     * Get lower bound approximation for state.
     *
     * @param state        The state.
     *
     * @return Lower bound approximation.
     */
    ValueType getLowerBound(DFTStatePointer const& state) const;

    /*!
     * Get upper bound approximation for state.
     *
     * @param state        The state.
     *
     * @return Upper bound approximation.
     */
    ValueType getUpperBound(DFTStatePointer const& state) const;

    /*!
     * Compute the MTTF of an AND gate via a closed formula.
     * The used formula is 1/( 1/a + 1/b + 1/c + ... - 1/(a+b) - 1/(a+c) - ... + 1/(a+b+c) + ... - ...)
     *
     * @param rates List of rates of children of AND.
     * @param size  Only indices < size are considered in the vector.
     * @return MTTF.
     */
    ValueType computeMTTFAnd(std::vector<ValueType> const& rates, size_t size) const;

    void printNotExplored() const;

    /*!
     * Create the model model from the model components.
     *
     * @param copy If true, all elements of the model component are copied (used for approximation). If false
     *             they are moved to reduce the memory overhead.
     *
     * @return The model built from the model components.
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType>> createModel(bool copy);

    // Initial size of the bitvector.
    const size_t INITIAL_BITVECTOR_SIZE = 20000;
    // Offset used for pseudo states.
    const StateType OFFSET_PSEUDO_STATE = std::numeric_limits<StateType>::max() / 2;

    // Dft
    storm::dft::storage::DFT<ValueType> const& dft;

    // General information for state generation
    // TODO: use const reference
    std::shared_ptr<storm::dft::storage::DFTStateGenerationInfo> stateGenerationInfo;

    // Heuristic used for approximation
    storm::dft::builder::ApproximationHeuristic usedHeuristic;

    // Current id for new state
    size_t newIndex = 0;

    // Whether to use a unique state for all failed states
    // If used, the unique failed state has the id 0
    bool uniqueFailedState = false;

    // Id of initial state
    size_t initialStateIndex = 0;

    // Next state generator for exploring the state space
    storm::dft::generator::DftNextStateGenerator<ValueType, StateType> generator;

    // Structure for the components of the model.
    ModelComponents modelComponents;

    // Structure for the transition matrix builder.
    MatrixBuilder matrixBuilder;

    // Internal information about the states that were explored.
    storm::storage::sparse::StateStorage<StateType> stateStorage;

    // A priority queue of states that still need to be explored.
    storm::dft::storage::BucketPriorityQueue<ExplorationHeuristic> explorationQueue;

    // A mapping of not yet explored states from the id to the tuple (state object, heuristic values).
    std::map<StateType, std::pair<DFTStatePointer, ExplorationHeuristicPointer>> statesNotExplored;

    // Holds all skipped states which were not yet expanded. More concretely it is a mapping from matrix indices
    // to the corresponding skipped states.
    // Notice that we need an ordered map here to easily iterate in increasing order over state ids.
    // TODO remove again
    std::map<StateType, std::pair<DFTStatePointer, ExplorationHeuristicPointer>> skippedStates;

    // List of independent subtrees and the BEs contained in them.
    std::vector<std::vector<size_t>> subtreeBEs;
};

}  // namespace builder
}  // namespace storm::dft
