#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/functional/hash.hpp>
#include <boost/variant.hpp>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>
#include "storm/models/sparse/StandardRewardModel.h"

#include "storm/logic/Formulas.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVectorHashMap.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/prism/Program.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/storage/sparse/StateStorage.h"

#include "storm/builder/ExplorationOrder.h"

#include "storm/generator/CompressedState.h"
#include "storm/generator/NextStateGenerator.h"
#include "storm/generator/VariableInformation.h"

namespace storm {

namespace builder {

using namespace storm::generator;

// Forward-declare classes.
template<typename ValueType>
class RewardModelBuilder;
class StateAndChoiceInformationBuilder;

template<typename StateType>
class ExplicitStateLookup {
   public:
    ExplicitStateLookup(VariableInformation const& varInfo, storm::storage::BitVectorHashMap<StateType> const& stateToId)
        : varInfo(varInfo), stateToId(stateToId) {
        // intentionally left empty.
    }

    /**
     * Lookup state
     * @param stateDescription A map describing the state
     * @return The id of the state, or size() when no state is found
     */
    StateType lookup(std::map<storm::expressions::Variable, storm::expressions::Expression> const& stateDescription) const;
    /**
     * How many states have been stored?
     */
    uint64_t size() const;

   private:
    VariableInformation varInfo;
    storm::storage::BitVectorHashMap<StateType> stateToId;
};

template<typename ValueType, typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>, typename StateType = uint32_t>
class ExplicitModelBuilder {
   public:
    struct Options {
        /*!
         * Creates an object representing the default building options.
         */
        Options();

        // The order in which to explore the model.
        ExplorationOrder explorationOrder;

        // If set, deadlocks states will be fixed by adding a self-loop with probability 1.
        bool fixDeadlocks;

        // If set, no further states will be explored once the given number is exceeded.
        std::optional<StateType> explorationStateLimit;
    };

    /*!
     * Creates an explicit model builder that uses the provided generator.
     *
     * @param generator The generator to use.
     */
    ExplicitModelBuilder(std::shared_ptr<storm::generator::NextStateGenerator<ValueType, StateType>> const& generator, Options const& options = Options());

    /*!
     * Creates an explicit model builder for the given PRISM program.
     *
     * @param program The program for which to build the model.
     */
    ExplicitModelBuilder(storm::prism::Program const& program,
                         storm::generator::NextStateGeneratorOptions const& generatorOptions = storm::generator::NextStateGeneratorOptions(),
                         Options const& builderOptions = Options());

    /*!
     * Creates an explicit model builder for the given JANI model.
     *
     * @param model The JANI model for which to build the model.
     */
    ExplicitModelBuilder(storm::jani::Model const& model,
                         storm::generator::NextStateGeneratorOptions const& generatorOptions = storm::generator::NextStateGeneratorOptions(),
                         Options const& builderOptions = Options())
        requires(!storm::IsIntervalType<ValueType>);

    /*!
     * Convert the program given at construction time to an abstract model. The type of the model is the one
     * specified in the program. The given reward model name selects the rewards that the model will contain.
     *
     * @return The explicit model that was given by the probabilistic program as well as additional
     *         information (if requested).
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> build();

    /*!
     * Export a wrapper that contains (a copy of) the internal information that maps states to ids.
     * This wrapper can be helpful to find states in later stages.
     * @return
     */
    ExplicitStateLookup<StateType> exportExplicitStateLookup() const;

   private:
    /*!
     * Retrieves the state id of the given state. If the state has not been encountered yet, it will be added to
     * the lists of all states with a new id. If the state was already known, the object that is pointed to by
     * the given state pointer is deleted and the old state id is returned. Note that the pointer should not be
     * used after invoking this method.
     *
     * @param state A pointer to a state for which to retrieve the index. This must not be used after the call.
     * @return A pair indicating whether the state was already discovered before and the state id of the state.
     */
    StateType getOrAddStateIndex(CompressedState const& state);

    /*!
     * Builds the transition matrix and the transition reward matrix based for the given program.
     *
     * @param transitionMatrixBuilder The builder of the transition matrix.
     * @param rewardModelBuilders The builders for the selected reward models.
     * @param stateAndChoiceInformationBuilder The builder for the requested information of the individual states and choices
     */
    void buildMatrices(storm::storage::SparseMatrixBuilder<ValueType>& transitionMatrixBuilder,
                       std::vector<RewardModelBuilder<typename RewardModelType::ValueType>>& rewardModelBuilders,
                       StateAndChoiceInformationBuilder& stateAndChoiceInformationBuilder);

    /*!
     * Explores the state space of the given program and returns the components of the model as a result.
     *
     * @return A structure containing the components of the resulting model.
     */
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> buildModelComponents();

    /*!
     * Builds the state labeling for the given program.
     *
     * @return The state labeling of the given program.
     */
    storm::models::sparse::StateLabeling buildStateLabeling();

    /// The generator to use for the building process.
    std::shared_ptr<storm::generator::NextStateGenerator<ValueType, StateType>> generator;

    /// The options to be used for the building process.
    Options options;

    /// Internal information about the states that were explored.
    storm::storage::sparse::StateStorage<StateType> stateStorage;

    /// A set of states that still need to be explored.
    std::deque<std::pair<CompressedState, StateType>> statesToExplore;

    /// An optional mapping from state indices to the row groups in which they actually reside. This needs to be
    /// built in case the exploration order is not BFS.
    boost::optional<std::vector<uint_fast64_t>> stateRemapping;
};

}  // namespace builder
}  // namespace storm
