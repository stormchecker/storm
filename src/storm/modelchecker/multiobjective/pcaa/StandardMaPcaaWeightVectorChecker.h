#pragma once

#include <type_traits>
#include <vector>

#include "storm/modelchecker/multiobjective/pcaa/StandardPcaaWeightVectorChecker.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

/*!
 * Helper Class that takes preprocessed Pcaa data and a weight vector and ...
 * - computes the maximal expected reward w.r.t. the weighted sum of the rewards of the individual objectives
 * - extracts the scheduler that induces this maximum
 * - computes for each objective the value induced by this scheduler
 */
template<class SparseMaModelType>
class StandardMaPcaaWeightVectorChecker : public StandardPcaaWeightVectorChecker<SparseMaModelType> {
   public:
    typedef typename SparseMaModelType::ValueType ValueType;

    StandardMaPcaaWeightVectorChecker(preprocessing::SparseMultiObjectivePreprocessorResult<SparseMaModelType> const& preprocessorResult);

    virtual ~StandardMaPcaaWeightVectorChecker() = default;

   protected:
    virtual void initializeModelTypeSpecificData(SparseMaModelType const& model) override;
    virtual storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> createNondetInfiniteHorizonHelper(
        storm::storage::SparseMatrix<ValueType> const& transitions) const override;
    virtual storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> createDetInfiniteHorizonHelper(
        storm::storage::SparseMatrix<ValueType> const& transitions) const override;

    virtual ValueType getWeightedPrecisionUnboundedPhase() const override;
    virtual ValueType getWeightedPrecisionBoundedPhase() const override;
    virtual bool smallPrecisionsAreChallenging() const override;

   private:
    /*
     * Stores (digitized) time bounds in descending order
     */
    typedef std::map<uint_fast64_t, storm::storage::BitVector, std::greater<uint_fast64_t>> TimeBoundMap;

    /*
     * Stores the ingredients of a sub model
     */
    struct SubModel {
        storm::storage::BitVector states;   // The states that are part of this sub model
        storm::storage::BitVector choices;  // The choices that are part of this sub model

        storm::storage::SparseMatrix<ValueType> toMS;  // Transitions to Markovian states
        storm::storage::SparseMatrix<ValueType> toPS;  // Transitions to probabilistic states

        std::vector<ValueType> weightedRewardVector;
        std::vector<std::vector<ValueType>> objectiveRewardVectors;

        std::vector<ValueType> weightedSolutionVector;
        std::vector<std::vector<ValueType>> objectiveSolutionVectors;

        std::vector<ValueType> auxChoiceValues;  // stores auxiliary values for every choice

        uint_fast64_t getNumberOfStates() const {
            return toMS.getRowGroupCount();
        };
        uint_fast64_t getNumberOfChoices() const {
            return toMS.getRowCount();
        };
    };

    /*
     * Stores the data that is relevant to invoke the minMaxSolver and retrieve the result.
     */
    struct MinMaxSolverData {
        std::unique_ptr<Environment> env;
        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver;
        std::vector<ValueType> b;
    };

    struct LinEqSolverData {
        std::unique_ptr<Environment> env;
        bool acyclic;
        std::unique_ptr<storm::solver::LinearEquationSolverFactory<ValueType>> factory;
        std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver;
        std::vector<ValueType> b;
    };

    /*!
     *
     * @param weightVector the weight vector of the current check
     * @param weightedRewardVector the weighted rewards considering the unbounded objectives. Will be invalidated after calling this.
     */
    virtual void boundedPhase(Environment const& env, std::vector<ValueType> const& weightVector, std::vector<ValueType>& weightedRewardVector) override;

    /*!
     * Retrieves the data for a submodel of the data->preprocessedModel
     * @param createMS if true, the submodel containing the Markovian states is created.
     *                 if false, the submodel containing the probabilistic states is created.
     */
    SubModel createSubModel(bool createMS, std::vector<ValueType> const& weightedRewardVector) const;

    /*!
     * Retrieves the delta used for digitization
     */
    template<typename VT = ValueType, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    VT getDigitizationConstant(std::vector<ValueType> const& weightVector) const;
    template<typename VT = ValueType, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    VT getDigitizationConstant(std::vector<ValueType> const& weightVector) const;

    /*!
     * Digitizes the given matrix and vectors w.r.t. the given digitization constant and the given rate vector.
     */
    template<typename VT = ValueType, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    void digitize(SubModel& subModel, VT const& digitizationConstant) const;
    template<typename VT = ValueType, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    void digitize(SubModel& subModel, VT const& digitizationConstant) const;

    /*!
     * Fills the given map with the digitized time bounds. Also sets the offsetsToUnderApproximation / offsetsToOverApproximation values
     * according to the digitization error
     */
    void digitizeTimeBounds(TimeBoundMap& upperTimeBounds, ValueType const& digitizationConstant, std::vector<ValueType> const& weightVector);

    /*!
     * Initializes the data for the MinMax solver
     */
    std::unique_ptr<MinMaxSolverData> initMinMaxSolver(Environment const& env, SubModel const& PS, bool acyclic,
                                                       std::vector<ValueType> const& weightVector) const;

    /*!
     * Initializes the data for the LinEq solver
     */
    template<typename VT = ValueType, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    std::unique_ptr<LinEqSolverData> initLinEqSolver(Environment const& env, SubModel const& PS, bool acyclic) const;
    template<typename VT = ValueType, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type = 0>
    std::unique_ptr<LinEqSolverData> initLinEqSolver(Environment const& env, SubModel const& PS, bool acyclic) const;

    /*
     * Updates the reward vectors within the split model,
     * the reward vector of the reduced PStoPS model, and
     * objectives that are considered at the current time epoch.
     */
    void updateDataToCurrentEpoch(SubModel& MS, SubModel& PS, MinMaxSolverData& minMax, storm::storage::BitVector& consideredObjectives,
                                  uint_fast64_t const& currentEpoch, std::vector<ValueType> const& weightVector, TimeBoundMap::iterator& upperTimeBoundIt,
                                  TimeBoundMap const& upperTimeBounds);

    /*
     * Performs a step for the probabilistic states, that is
     * * Compute an optimal scheduler for the weighted reward sum
     * * Compute the values for the individual objectives w.r.t. that scheduler
     *
     * The resulting values represent the rewards at probabilistic states that are obtained at the current time epoch.
     */
    void performPSStep(Environment const& env, SubModel& PS, SubModel const& MS, MinMaxSolverData& minMax, LinEqSolverData& linEq,
                       std::vector<uint_fast64_t>& optimalChoicesAtCurrentEpoch, storm::storage::BitVector const& consideredObjectives,
                       std::vector<ValueType> const& weightVector) const;

    /*
     * Performs a step for the Markovian states, that is
     * * Compute values for the weighted reward sum as well as for the individual objectives
     *
     * The resulting values represent the rewards at Markovian states that are obtained after one (digitized) time unit has passed.
     */
    void performMSStep(Environment const& env, SubModel& MS, SubModel const& PS, storm::storage::BitVector const& consideredObjectives,
                       std::vector<ValueType> const& weightVector) const;

    // Data regarding the given Markov automaton
    storm::storage::BitVector markovianStates;
    std::vector<ValueType> exitRates;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
