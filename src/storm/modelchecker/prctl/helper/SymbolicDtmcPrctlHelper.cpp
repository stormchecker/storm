#include "storm/modelchecker/prctl/helper/SymbolicDtmcPrctlHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/solver/SymbolicLinearEquationSolver.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates, bool qualitative,
    boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues) {
    // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
    // probability 0 and 1 of satisfying the until-formula.
    STORM_LOG_TRACE("Found " << phiStates.getNonZeroCount() << " phi states and " << psiStates.getNonZeroCount() << " psi states.");
    std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01 =
        storm::utility::graph::performProb01(model, transitionMatrix.notZero(), phiStates, psiStates);
    storm::dd::Bdd<DdType> maybeStates = !statesWithProbability01.first && !statesWithProbability01.second && model.getReachableStates();

    STORM_LOG_INFO("Preprocessing: " << statesWithProbability01.first.getNonZeroCount() << " states with probability 0, "
                                     << statesWithProbability01.second.getNonZeroCount() << " with probability 1 (" << maybeStates.getNonZeroCount()
                                     << " states remaining).");

    // Check whether we need to compute exact probabilities for some states.
    if (qualitative) {
        // Set the values for all maybe-states to 0.5 to indicate that their probability values are neither 0 nor 1.
        return statesWithProbability01.second.template toAdd<ValueType>() +
               maybeStates.template toAdd<ValueType>() * model.getManager().getConstant(storm::utility::convertNumber<ValueType>(0.5));
    } else {
        // If there are maybe states, we need to solve an equation system.
        if (!maybeStates.isZero()) {
            return computeUntilProbabilities(env, model, transitionMatrix, maybeStates, statesWithProbability01.second,
                                             startValues ? maybeStates.ite(startValues.get(), model.getManager().template getAddZero<ValueType>())
                                                         : model.getManager().template getAddZero<ValueType>());
        } else {
            return statesWithProbability01.second.template toAdd<ValueType>();
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& maybeStates, storm::dd::Bdd<DdType> const& statesWithProbability1,
    boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues) {
    // Create the matrix and the vector for the equation system.
    storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

    // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
    // non-maybe states in the matrix.
    storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * maybeStatesAdd;

    // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
    // maybe states.
    storm::dd::Add<DdType, ValueType> prob1StatesAsColumn =
        statesWithProbability1.template toAdd<ValueType>().swapVariables(model.getRowColumnMetaVariablePairs());
    storm::dd::Add<DdType, ValueType> subvector = submatrix * prob1StatesAsColumn;
    subvector = subvector.sumAbstract(model.getColumnVariables());

    // Finally cut away all columns targeting non-maybe states and convert the matrix into the matrix needed
    // for solving the equation system (i.e. compute (I-A)).
    submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());
    storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType> linearEquationSolverFactory;
    if (linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem) {
        submatrix = (model.getRowColumnIdentity() * maybeStatesAdd) - submatrix;
    }

    // Solve the equation system.
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = linearEquationSolverFactory.create(
        env, submatrix, maybeStates, model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs());
    solver->setBounds(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
    storm::dd::Add<DdType, ValueType> result = solver->solveEquations(env, model.getManager().template getAddZero<ValueType>(), subvector);

    return statesWithProbability1.template toAdd<ValueType>() + result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeGloballyProbabilities(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& psiStates, bool qualitative) {
    storm::dd::Add<DdType, ValueType> result =
        computeUntilProbabilities(env, model, transitionMatrix, model.getReachableStates(), !psiStates && model.getReachableStates(), qualitative);
    result = result.getDdManager().template getAddOne<ValueType>() - result;
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeNextProbabilities(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& nextStates) {
    storm::dd::Add<DdType, ValueType> result = transitionMatrix * nextStates.swapVariables(model.getRowColumnMetaVariablePairs()).template toAdd<ValueType>();
    return result.sumAbstract(model.getColumnVariables());
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeBoundedUntilProbabilities(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates, uint_fast64_t stepBound) {
    // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
    // probability 0 or 1 of satisfying the until-formula.
    storm::dd::Bdd<DdType> statesWithProbabilityGreater0 =
        storm::utility::graph::performProbGreater0(model, transitionMatrix.notZero(), phiStates, psiStates, stepBound);
    storm::dd::Bdd<DdType> maybeStates = statesWithProbabilityGreater0 && !psiStates && model.getReachableStates();

    // If there are maybe states, we need to perform matrix-vector multiplications.
    if (!maybeStates.isZero()) {
        // Create the matrix and the vector for the equation system.
        storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

        // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
        // non-maybe states in the matrix.
        storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * maybeStatesAdd;

        // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
        // maybe states.
        storm::dd::Add<DdType, ValueType> prob1StatesAsColumn = psiStates.template toAdd<ValueType>().swapVariables(model.getRowColumnMetaVariablePairs());
        storm::dd::Add<DdType, ValueType> subvector = (submatrix * prob1StatesAsColumn).sumAbstract(model.getColumnVariables());

        // Finally cut away all columns targeting non-maybe states.
        submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());

        // Perform the matrix-vector multiplication.
        storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType> linearEquationSolverFactory;
        std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = linearEquationSolverFactory.create(
            env, submatrix, maybeStates, model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs());
        storm::dd::Add<DdType, ValueType> result = solver->multiply(model.getManager().template getAddZero<ValueType>(), &subvector, stepBound);

        return psiStates.template toAdd<ValueType>() + result;
    } else {
        return psiStates.template toAdd<ValueType>();
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeCumulativeRewards(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // Compute the reward vector to add in each step based on the available reward models.
    storm::dd::Add<DdType, ValueType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix, model.getColumnVariables());

    // Perform the matrix-vector multiplication.
    storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType> linearEquationSolverFactory;
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = linearEquationSolverFactory.create(
        env, transitionMatrix, model.getReachableStates(), model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs());
    return solver->multiply(model.getManager().template getAddZero<ValueType>(), &totalRewardVector, stepBound);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeInstantaneousRewards(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                    "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");

    // Perform the matrix-vector multiplication.
    storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType> linearEquationSolverFactory;
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = linearEquationSolverFactory.create(
        env, transitionMatrix, model.getReachableStates(), model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs());
    return solver->multiply(rewardModel.getStateRewardVector(), nullptr, stepBound);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates, bool qualitative,
    boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues) {
    // Only compute the result if there is at least one reward model.
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // Determine which states have a reward of infinity by definition.
    storm::dd::Bdd<DdType> infinityStates = storm::utility::graph::performProb1(model, transitionMatrix.notZero(), model.getReachableStates(), targetStates);
    infinityStates = !infinityStates && model.getReachableStates();
    storm::dd::Bdd<DdType> maybeStates = (!targetStates && !infinityStates) && model.getReachableStates();

    STORM_LOG_INFO("Preprocessing: " << infinityStates.getNonZeroCount() << " states with reward infinity, " << targetStates.getNonZeroCount()
                                     << " target states (" << maybeStates.getNonZeroCount() << " states remaining).");

    // Check whether we need to compute exact rewards for some states.
    if (qualitative) {
        // Set the values for all maybe-states to 1 to indicate that their reward values
        // are neither 0 nor infinity.
        return infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()), model.getManager().template getAddZero<ValueType>()) +
               maybeStates.template toAdd<ValueType>() * model.getManager().getConstant(storm::utility::one<ValueType>());
    } else {
        // If there are maybe states, we need to solve an equation system.
        if (!maybeStates.isZero()) {
            return computeReachabilityRewards(env, model, transitionMatrix, rewardModel, maybeStates, targetStates, infinityStates,
                                              startValues ? maybeStates.ite(startValues.get(), model.getManager().template getAddZero<ValueType>())
                                                          : model.getManager().template getAddZero<ValueType>());
        } else {
            return infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()),
                                      model.getManager().getConstant(storm::utility::zero<ValueType>()));
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& maybeStates, storm::dd::Bdd<DdType> const& targetStates,
    storm::dd::Bdd<DdType> const& infinityStates, boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues) {
    // Create the matrix and the vector for the equation system.
    storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

    // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
    // non-maybe states in the matrix.
    storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * maybeStatesAdd;

    // Then compute the state reward vector to use in the computation.
    storm::dd::Add<DdType, ValueType> subvector = rewardModel.getTotalRewardVector(maybeStatesAdd, submatrix, model.getColumnVariables());

    // Finally cut away all columns targeting non-maybe states and convert the matrix into the matrix needed
    // for solving the equation system (i.e. compute (I-A)).
    submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());
    storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType> linearEquationSolverFactory;
    if (linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem) {
        submatrix = (model.getRowColumnIdentity() * maybeStatesAdd) - submatrix;
    }

    // Solve the equation system.
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = linearEquationSolverFactory.create(
        env, submatrix, maybeStates, model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs());
    solver->setLowerBound(storm::utility::zero<ValueType>());
    storm::dd::Add<DdType, ValueType> result =
        solver->solveEquations(env, startValues ? startValues.get() : maybeStatesAdd.getDdManager().template getAddZero<ValueType>(), subvector);

    return infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()), result);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityTimes(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& transitionMatrix,
    storm::dd::Bdd<DdType> const& targetStates, bool qualitative, boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues) {
    RewardModelType rewardModel(model.getManager().getConstant(storm::utility::one<ValueType>()), boost::none, boost::none);
    return computeReachabilityRewards(env, model, transitionMatrix, rewardModel, targetStates, qualitative, startValues);
}

template class SymbolicDtmcPrctlHelper<storm::dd::DdType::CUDD, double>;
template class SymbolicDtmcPrctlHelper<storm::dd::DdType::Sylvan, double>;

template class SymbolicDtmcPrctlHelper<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicDtmcPrctlHelper<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
