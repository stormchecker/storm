#include "SparseDerivativeInstantiationModelChecker.h"
#include "storm-pars/modelchecker/instantiation/SparseDtmcInstantiationModelChecker.h"
#include "storm/analysis/GraphConditions.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/Formula.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/solver/EliminationLinearEquationSolver.h"
#include "storm/solver/GmmxxLinearEquationSolver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/helper/SoundValueIterationHelper.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/vector.h"

namespace storm {
namespace derivative {

template<typename FunctionType>
using VariableType = typename utility::parametric::VariableType<FunctionType>::type;
template<typename FunctionType>
using CoefficientType = typename utility::parametric::CoefficientType<FunctionType>::type;

template<typename FunctionType, typename ConstantType>
std::unique_ptr<modelchecker::ExplicitQuantitativeCheckResult<ConstantType>> SparseDerivativeInstantiationModelChecker<FunctionType, ConstantType>::check(
    Environment const& env, storm::utility::parametric::Valuation<FunctionType> const& valuation, VariableType<FunctionType> const& parameter,
    boost::optional<std::vector<ConstantType>> const& valueVector) {
    std::vector<ConstantType> reachabilityProbabilities;
    if (!valueVector.is_initialized()) {
        storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<FunctionType>, ConstantType> instantiationModelChecker(model);
        instantiationModelChecker.specifyFormula(*currentCheckTask);
        std::unique_ptr<storm::modelchecker::CheckResult> result = instantiationModelChecker.check(env, valuation);
        reachabilityProbabilities = result->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
    } else {
        STORM_LOG_ASSERT(valueVector->size() == model.getNumberOfStates(), "Size of reachability probability vector must be equal to the size of the model.");
        reachabilityProbabilities = *valueVector;
    }

    // Convert reachabilityProbabilities into our format - we only care for the states of which the
    // bits are 1 in the next vector. The order is kept, so doing this is fine:
    std::vector<ConstantType> interestingReachabilityProbabilities;
    for (uint64_t i = 0; i < reachabilityProbabilities.size(); i++) {
        if (next.get(i)) {
            interestingReachabilityProbabilities.push_back(reachabilityProbabilities[i]);
        }
    }
    // Instantiate the matrices with the given instantiation

    instantiationWatch.start();

    // STORM_PRINT_AND_LOG(valuation << "\n");

    // Write results into the placeholders
    for (auto& functionResult : this->functionsUnderived) {
        functionResult.second = storm::utility::parametric::evaluate<ConstantType>(functionResult.first, valuation);
    }
    for (auto& functionResult : this->functionsDerived.at(parameter)) {
        functionResult.second = storm::utility::parametric::evaluate<ConstantType>(functionResult.first, valuation);
    }

    auto deltaConstrainedMatrixInstantiated = deltaConstrainedMatricesInstantiated->at(parameter);

    // Write the instantiated values to the matrices and vectors according to the stored mappings
    for (auto& entryValuePair : this->matrixMappingUnderived) {
        entryValuePair.first->setValue(*(entryValuePair.second));
    }
    for (auto& entryValuePair : this->matrixMappingsDerived.at(parameter)) {
        entryValuePair.first->setValue(*(entryValuePair.second));
    }

    std::vector<ConstantType> instantiatedDerivedOutputVec(derivedOutputVecs->at(parameter).size());
    for (uint_fast64_t i = 0; i < derivedOutputVecs->at(parameter).size(); i++) {
        instantiatedDerivedOutputVec[i] = storm::utility::parametric::evaluate<ConstantType>(derivedOutputVecs->at(parameter)[i], valuation);
    }

    instantiationWatch.stop();

    approximationWatch.start();

    std::vector<ConstantType> resultVec(interestingReachabilityProbabilities.size());
    deltaConstrainedMatrixInstantiated.multiplyWithVector(interestingReachabilityProbabilities, resultVec);
    for (uint_fast64_t i = 0; i < instantiatedDerivedOutputVec.size(); ++i) {
        resultVec[i] += instantiatedDerivedOutputVec[i];
    }

    // Here's where the real magic happens - the solver call!
    storm::solver::GeneralLinearEquationSolverFactory<ConstantType> factory;
    auto solver = factory.create(env);

    // Calculate (1-M)^-1 * resultVec
    // STORM_PRINT_AND_LOG(constrainedMatrixInstantiated << "\n");
    // STORM_PRINT_AND_LOG("calling solver\n");
    solver->setMatrix(constrainedMatrixInstantiated);
    std::vector<ConstantType> finalResult(resultVec.size());
    solver->solveEquations(env, finalResult, resultVec);

    approximationWatch.stop();

    return std::make_unique<modelchecker::ExplicitQuantitativeCheckResult<ConstantType>>(finalResult);
}

template<typename FunctionType, typename ConstantType>
void SparseDerivativeInstantiationModelChecker<FunctionType, ConstantType>::specifyFormula(
    Environment const& env, modelchecker::CheckTask<storm::logic::Formula, FunctionType> const& checkTask) {
    this->currentFormula = checkTask.getFormula().asSharedPointer();
    this->currentCheckTask = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, FunctionType>>(
        checkTask.substituteFormula(*currentFormula).template convertValueType<FunctionType>());
    this->parameters = storm::models::sparse::getProbabilityParameters(model);
    if (checkTask.getFormula().isRewardOperatorFormula()) {
        for (auto const& rewardParameter : storm::models::sparse::getRewardParameters(model)) {
            this->parameters.insert(rewardParameter);
        }
    }
    storm::logic::OperatorInformation opInfo(boost::none, boost::none);
    if (checkTask.getFormula().isRewardOperatorFormula()) {
        model.reduceToStateBasedRewards();
    } else {
        /* this->formulaWithoutBound = std::make_shared<storm::logic::ProbabilityOperatorFormula>( */
        /*         formula->asProbabilityOperatorFormula().getSubformula().asSharedPointer(), opInfo); */
    }

    generalSetupWatch.start();

    storm::solver::GeneralLinearEquationSolverFactory<ConstantType> factory;

    bool convertToEquationSystem = factory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;

    std::map<VariableType<FunctionType>, storage::SparseMatrix<FunctionType>> equationSystems;
    // Get initial and target states
    storm::modelchecker::SparsePropositionalModelChecker<models::sparse::Dtmc<FunctionType>> propositionalChecker(model);
    storage::BitVector target;
    storage::BitVector avoid(model.getNumberOfStates());
    if (this->currentFormula->isRewardOperatorFormula()) {
        auto subformula = modelchecker::CheckTask<storm::logic::Formula, FunctionType>(
            this->currentFormula->asRewardOperatorFormula().getSubformula().asEventuallyFormula().getSubformula());
        target = propositionalChecker.check(subformula)->template asExplicitQualitativeCheckResult<FunctionType>().getTruthValuesVector();
    } else {
        if (this->currentFormula->asProbabilityOperatorFormula().getSubformula().isUntilFormula()) {
            auto rightSubformula = modelchecker::CheckTask<storm::logic::Formula, FunctionType>(
                this->currentFormula->asProbabilityOperatorFormula().getSubformula().asUntilFormula().getRightSubformula());
            auto leftSubformula = modelchecker::CheckTask<storm::logic::Formula, FunctionType>(
                this->currentFormula->asProbabilityOperatorFormula().getSubformula().asUntilFormula().getLeftSubformula());
            target = propositionalChecker.check(rightSubformula)->template asExplicitQualitativeCheckResult<FunctionType>().getTruthValuesVector();
            avoid = propositionalChecker.check(leftSubformula)->template asExplicitQualitativeCheckResult<FunctionType>().getTruthValuesVector();
            avoid.complement();
        } else {
            auto subformula = modelchecker::CheckTask<storm::logic::Formula, FunctionType>(
                this->currentFormula->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula());
            target = propositionalChecker.check(subformula)->template asExplicitQualitativeCheckResult<FunctionType>().getTruthValuesVector();
        }
    }
    initialStateModel = model.getStates("init").getNextSetIndex(0);

    if (!checkTask.getFormula().isRewardOperatorFormula()) {
        next = target;
        next.complement();

        avoid.complement();
        next &= avoid;

        storm::storage::BitVector atSomePointTarget =
            storm::utility::graph::performProbGreater0(model.getBackwardTransitions(), storm::storage::BitVector(model.getNumberOfStates(), true), target);
        next &= atSomePointTarget;
    } else {
        next = target;
        next.complement();

        avoid.complement();
        next &= avoid;

        storm::storage::BitVector targetProbOne =
            storm::utility::graph::performProb1(model.getBackwardTransitions(), storm::storage::BitVector(model.getNumberOfStates(), true), target);
        next &= targetProbOne;
    }

    auto transitionMatrix = model.getTransitionMatrix();
    std::map<uint_fast64_t, uint_fast64_t> stateNumToEquationSystemRow;
    uint_fast64_t newRow = 0;
    for (uint_fast64_t row = 0; row < transitionMatrix.getRowCount(); ++row) {
        if (!next.get(row))
            continue;
        stateNumToEquationSystemRow[row] = newRow;
        newRow++;
    }
    initialStateEqSystem = stateNumToEquationSystemRow[initialStateModel];
    storage::SparseMatrix<FunctionType> constrainedMatrix = transitionMatrix.getSubmatrix(false, next, next, true);
    // If necessary, convert the matrix from the fixpoint notation to the form needed for the equation system.
    this->constrainedMatrixEquationSystem = constrainedMatrix;
    if (convertToEquationSystem) {
        // go from x = A*x + b to (I-A)x = b.
        constrainedMatrixEquationSystem.convertToEquationSystem();
    }

    // Setup instantiated constrained matrix
    storage::SparseMatrixBuilder<ConstantType> instantiatedSystemBuilder;
    const ConstantType dummyValue = storm::utility::one<ConstantType>();
    for (uint_fast64_t row = 0; row < constrainedMatrixEquationSystem.getRowCount(); ++row) {
        for (auto const& entry : constrainedMatrixEquationSystem.getRow(row)) {
            instantiatedSystemBuilder.addNextValue(row, entry.getColumn(), dummyValue);
        }
    }
    constrainedMatrixInstantiated = instantiatedSystemBuilder.build();
    initializeInstantiatedMatrix(constrainedMatrixEquationSystem, constrainedMatrixInstantiated, matrixMappingUnderived, functionsUnderived);
    // The resulting equation systems
    this->deltaConstrainedMatrices = std::make_unique<std::map<VariableType<FunctionType>, storage::SparseMatrix<FunctionType>>>();
    this->deltaConstrainedMatricesInstantiated = std::make_unique<std::map<VariableType<FunctionType>, storage::SparseMatrix<ConstantType>>>();
    this->derivedOutputVecs = std::make_unique<std::map<VariableType<FunctionType>, std::vector<FunctionType>>>();

    // The following is a nessecary optimization for deriving the constrainedMatrix w.r.t. all parameters.
    // We will traverse the constrainedMatrix once and inspect all entries, counting their number of parameters.
    // Then, we will insert the correct polynomials into the correct matrix builders.
    std::map<VariableType<FunctionType>, storage::SparseMatrixBuilder<FunctionType>> matrixBuilders;
    std::map<VariableType<FunctionType>, storage::SparseMatrixBuilder<ConstantType>> instantiatedMatrixBuilders;
    for (auto const& var : this->parameters) {
        matrixBuilders[var] = storage::SparseMatrixBuilder<FunctionType>(constrainedMatrix.getRowCount());
        instantiatedMatrixBuilders[var] = storage::SparseMatrixBuilder<ConstantType>(constrainedMatrix.getRowCount());
    }

    for (uint_fast64_t row = 0; row < constrainedMatrix.getRowCount(); ++row) {
        for (storage::MatrixEntry<uint_fast64_t, storm::RationalFunction> const& entry : constrainedMatrix.getRow(row)) {
            const storm::RationalFunction rationalFunction = entry.getValue();
            auto variables = rationalFunction.gatherVariables();
            for (auto const& var : variables) {
                matrixBuilders.at(var).addNextValue(row, entry.getColumn(), rationalFunction.derivative(var));
                instantiatedMatrixBuilders.at(var).addNextValue(row, entry.getColumn(), dummyValue);
            }
        }
    }

    for (auto const& var : this->parameters) {
        auto builtMatrix = matrixBuilders[var].build();
        auto builtMatrixInstantiated = instantiatedMatrixBuilders[var].build();
        initializeInstantiatedMatrix(builtMatrix, builtMatrixInstantiated, matrixMappingsDerived[var], functionsDerived[var]);
        deltaConstrainedMatrices->emplace(var, std::move(builtMatrix));
        deltaConstrainedMatricesInstantiated->emplace(var, std::move(builtMatrixInstantiated));
    }

    for (auto const& var : this->parameters) {
        (*derivedOutputVecs)[var] = std::vector<FunctionType>(constrainedMatrix.getRowCount());
    }

    // storage::BitVector constrainedTarget(next.size());
    // for (uint_fast64_t i = 0; i < transitionMatrix.getRowCount(); i++) {
    //     if (!stateNumToEquationSystemRow.count(i)) continue;
    //     constrainedTarget.set(stateNumToEquationSystemRow[i], target[i]);
    // }

    for (uint_fast64_t state = 0; state < transitionMatrix.getRowCount(); ++state) {
        if (!stateNumToEquationSystemRow.count(state))
            continue;
        uint_fast64_t row = stateNumToEquationSystemRow[state];
        // PROBABILITY -> For every state, the one-step probability to reach the target goes into the output vector
        // REWARD -> For every state, the reward goes into the output vector
        FunctionType rationalFunction;
        if (!checkTask.getFormula().isRewardOperatorFormula()) {
            FunctionType vectorValue = utility::zero<FunctionType>();
            for (auto const& entry : transitionMatrix.getRow(state)) {
                if (target.get(entry.getColumn())) {
                    vectorValue += entry.getValue();
                }
            }
            rationalFunction = vectorValue;
        } else {
            std::vector<FunctionType> stateRewards;
            if (checkTask.isRewardModelSet()) {
                stateRewards = model.getRewardModel(checkTask.getRewardModel()).getStateRewardVector();
            } else {
                stateRewards = model.getRewardModel("").getStateRewardVector();
            }

            rationalFunction = stateRewards[state];
        }
        for (auto const& var : rationalFunction.gatherVariables()) {
            (*derivedOutputVecs)[var][row] = rationalFunction.derivative(var);
        }
    }

    generalSetupWatch.stop();

    // for (auto const& param : this->parameters) {
    //     this->linearEquationSolvers[param] = factory.create(env);
    //     // this->linearEquationSolvers[param]->setCachingEnabled(true);
    //     // std::unique_ptr<solver::TerminationCondition<ConstantType>> terminationCondition =
    //     //     std::make_unique<SignedGradientDescentTerminationCondition<ConstantType>>(initialState);
    //     // this->linearEquationSolvers[param]->setTerminationCondition(std::move(terminationCondition));
    // }
}

template<typename FunctionType, typename ConstantType>
void SparseDerivativeInstantiationModelChecker<FunctionType, ConstantType>::initializeInstantiatedMatrix(
    storage::SparseMatrix<FunctionType>& matrix, storage::SparseMatrix<ConstantType>& matrixInstantiated,
    std::vector<std::pair<typename storm::storage::SparseMatrix<ConstantType>::iterator, ConstantType*>>& matrixMapping,
    std::unordered_map<FunctionType, ConstantType>& functions) {
    ConstantType dummyValue = storm::utility::one<ConstantType>();
    auto constantEntryIt = matrixInstantiated.begin();
    auto parametricEntryIt = matrix.begin();
    while (parametricEntryIt != matrix.end()) {
        STORM_LOG_ASSERT(parametricEntryIt->getColumn() == constantEntryIt->getColumn(),
                         "Entries of parametric and constant matrix are not at the same position.");
        if (storm::utility::isConstant(parametricEntryIt->getValue())) {
            // Constant entries can be inserted directly
            constantEntryIt->setValue(storm::utility::convertNumber<ConstantType>(parametricEntryIt->getValue()));
            // STORM_PRINT_AND_LOG("Setting constant entry\n");
        } else {
            // insert the new function and store that the current constantMatrix entry needs to be set to the value of this function
            auto functionsIt = functions.insert(std::make_pair(parametricEntryIt->getValue(), dummyValue)).first;
            matrixMapping.emplace_back(std::make_pair(constantEntryIt, &(functionsIt->second)));
            // Note that references to elements of an unordered map remain valid after calling unordered_map::insert.
            // STORM_PRINT_AND_LOG("Setting non-constant entry\n");
        }
        ++constantEntryIt;
        ++parametricEntryIt;
    }
    STORM_LOG_ASSERT(constantEntryIt == matrixInstantiated.end(), "Parametric matrix seems to have more or less entries then the constant matrix.");
}

template class SparseDerivativeInstantiationModelChecker<RationalFunction, RationalNumber>;
template class SparseDerivativeInstantiationModelChecker<RationalFunction, double>;
}  // namespace derivative
}  // namespace storm
