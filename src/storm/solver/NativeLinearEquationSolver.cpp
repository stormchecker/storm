#include "storm/solver/NativeLinearEquationSolver.h"

#include <limits>

#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/OviSolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/solver/helper/GuessingValueIterationHelper.h"
#include "storm/solver/helper/IntervalterationHelper.h"
#include "storm/solver/helper/OptimisticValueIterationHelper.h"
#include "storm/solver/helper/RationalSearchHelper.h"
#include "storm/solver/helper/SoundValueIterationHelper.h"
#include "storm/solver/helper/ValueIterationHelper.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
NativeLinearEquationSolver<ValueType>::NativeLinearEquationSolver() : localA(nullptr), A(nullptr) {
    // Intentionally left empty.
}

template<typename ValueType>
NativeLinearEquationSolver<ValueType>::NativeLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A) : localA(nullptr), A(nullptr) {
    this->setMatrix(A);
}

template<typename ValueType>
NativeLinearEquationSolver<ValueType>::NativeLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A) : localA(nullptr), A(nullptr) {
    this->setMatrix(std::move(A));
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::setMatrix(storm::storage::SparseMatrix<ValueType> const& A) {
    localA.reset();
    this->A = &A;
    clearCache();
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::setMatrix(storm::storage::SparseMatrix<ValueType>&& A) {
    localA = std::make_unique<storm::storage::SparseMatrix<ValueType>>(std::move(A));
    this->A = localA.get();
    clearCache();
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::setUpViOperator() const {
    if (!viOperator) {
        viOperator = std::make_shared<helper::ValueIterationOperator<ValueType, true>>();
        viOperator->setMatrixBackwards(*this->A);
    }
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsSOR(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b,
                                                              ValueType const& omega) const {
    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with NativeLinearEquationSolver (Gauss-Seidel, SOR omega = " << omega << ")");

    if (!this->cachedRowVector) {
        this->cachedRowVector = std::make_unique<std::vector<ValueType>>(getMatrixRowCount());
    }

    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    uint64_t maxIter = env.solver().native().getMaximalNumberOfIterations();
    bool relative = env.solver().native().getRelativeTerminationCriterion();

    // Set up additional environment variables.
    uint_fast64_t iterations = 0;
    SolverStatus status = SolverStatus::InProgress;

    this->startMeasureProgress();
    while (status == SolverStatus::InProgress && iterations < maxIter) {
        A->performSuccessiveOverRelaxationStep(omega, x, b);

        // Now check if the process already converged within our precision.
        if (storm::utility::vector::equalModuloPrecision<ValueType>(*this->cachedRowVector, x, precision, relative)) {
            status = SolverStatus::Converged;
        }
        // If we did not yet converge, we need to backup the contents of x.
        if (status != SolverStatus::Converged) {
            *this->cachedRowVector = x;
        }

        // Potentially show progress.
        this->showProgressIterative(iterations);

        // Increase iteration count so we can abort if convergence is too slow.
        ++iterations;

        status = this->updateStatus(status, x, SolverGuarantee::None, iterations, maxIter);
    }

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    this->reportStatus(status, iterations);

    return status == SolverStatus::Converged;
}

template<typename ValueType>
NativeLinearEquationSolver<ValueType>::JacobiDecomposition::JacobiDecomposition(Environment const& env, storm::storage::SparseMatrix<ValueType> const& A) {
    auto decomposition = A.getJacobiDecomposition();
    this->LUMatrix = std::move(decomposition.first);
    this->DVector = std::move(decomposition.second);
    this->multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, this->LUMatrix);
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsJacobi(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with NativeLinearEquationSolver (Jacobi)");

    if (!this->cachedRowVector) {
        this->cachedRowVector = std::make_unique<std::vector<ValueType>>(getMatrixRowCount());
    }

    // Get a Jacobi decomposition of the matrix A.
    if (!jacobiDecomposition) {
        jacobiDecomposition = std::make_unique<JacobiDecomposition>(env, *A);
    }

    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    uint64_t maxIter = env.solver().native().getMaximalNumberOfIterations();
    bool relative = env.solver().native().getRelativeTerminationCriterion();

    std::vector<ValueType>* currentX = &x;
    std::vector<ValueType>* nextX = this->cachedRowVector.get();

    // Set up additional environment variables.
    uint_fast64_t iterations = 0;
    SolverStatus status = SolverStatus::InProgress;

    this->startMeasureProgress();
    while (status == SolverStatus::InProgress && iterations < maxIter) {
        // Compute D^-1 * (b - LU * x) and store result in nextX.
        jacobiDecomposition->multiplier->multiply(env, *currentX, nullptr, *nextX);
        storm::utility::vector::subtractVectors(b, *nextX, *nextX);
        storm::utility::vector::multiplyVectorsPointwise(jacobiDecomposition->DVector, *nextX, *nextX);

        // Now check if the process already converged within our precision.
        if (storm::utility::vector::equalModuloPrecision<ValueType>(*currentX, *nextX, precision, relative)) {
            status = SolverStatus::Converged;
        }
        // Swap the two pointers as a preparation for the next iteration.
        std::swap(nextX, currentX);

        // Potentially show progress.
        this->showProgressIterative(iterations);

        // Increase iteration count so we can abort if convergence is too slow.
        ++iterations;

        status = this->updateStatus(status, *currentX, SolverGuarantee::None, iterations, maxIter);
    }

    // If the last iteration did not write to the original x we have to swap the contents, because the
    // output has to be written to the input parameter x.
    if (currentX == this->cachedRowVector.get()) {
        std::swap(x, *currentX);
    }

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    this->reportStatus(status, iterations);

    return status == SolverStatus::Converged;
}

template<typename ValueType>
NativeLinearEquationSolver<ValueType>::WalkerChaeData::WalkerChaeData(Environment const& env, storm::storage::SparseMatrix<ValueType> const& originalMatrix,
                                                                      std::vector<ValueType> const& originalB)
    : t(storm::utility::convertNumber<ValueType>(1000.0)) {
    computeWalkerChaeMatrix(originalMatrix);
    computeNewB(originalB);
    precomputeAuxiliaryData();
    multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, this->matrix);
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::WalkerChaeData::computeWalkerChaeMatrix(storm::storage::SparseMatrix<ValueType> const& originalMatrix) {
    storm::storage::BitVector columnsWithNegativeEntries(originalMatrix.getColumnCount());
    ValueType zero = storm::utility::zero<ValueType>();
    for (auto const& e : originalMatrix) {
        if (e.getValue() < zero) {
            columnsWithNegativeEntries.set(e.getColumn());
        }
    }
    std::vector<uint64_t> columnsWithNegativeEntriesBefore = columnsWithNegativeEntries.getNumberOfSetBitsBeforeIndices();

    // We now build an extended equation system matrix that only has non-negative coefficients.
    storm::storage::SparseMatrixBuilder<ValueType> builder;

    uint64_t row = 0;
    for (; row < originalMatrix.getRowCount(); ++row) {
        for (auto const& entry : originalMatrix.getRow(row)) {
            if (entry.getValue() < zero) {
                builder.addNextValue(row, originalMatrix.getRowCount() + columnsWithNegativeEntriesBefore[entry.getColumn()], -entry.getValue());
            } else {
                builder.addNextValue(row, entry.getColumn(), entry.getValue());
            }
        }
    }
    ValueType one = storm::utility::one<ValueType>();
    for (uint64_t column : columnsWithNegativeEntries) {
        builder.addNextValue(row, column, one);
        builder.addNextValue(row, originalMatrix.getRowCount() + columnsWithNegativeEntriesBefore[column], one);
        ++row;
    }

    matrix = builder.build();
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::WalkerChaeData::computeNewB(std::vector<ValueType> const& originalB) {
    b = std::vector<ValueType>(originalB);
    b.resize(matrix.getRowCount());
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::WalkerChaeData::precomputeAuxiliaryData() {
    columnSums = std::vector<ValueType>(matrix.getColumnCount());
    for (auto const& e : matrix) {
        columnSums[e.getColumn()] += e.getValue();
    }

    newX.resize(matrix.getRowCount());
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsWalkerChae(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with NativeLinearEquationSolver (WalkerChae)");

    // (1) Compute an equivalent equation system that has only non-negative coefficients.
    if (!walkerChaeData) {
        walkerChaeData = std::make_unique<WalkerChaeData>(env, *this->A, b);
    }

    // (2) Enlarge the vectors x and b to account for additional variables.
    x.resize(walkerChaeData->matrix.getRowCount());

    // Square the error bound, so we can use it to check for convergence. We take the squared error, because we
    // do not want to compute the root in the 2-norm computation.
    ValueType squaredErrorBound = storm::utility::pow(storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision()), 2);

    uint64_t maxIter = env.solver().native().getMaximalNumberOfIterations();

    // Set up references to the x-vectors used in the iteration loop.
    std::vector<ValueType>* currentX = &x;
    std::vector<ValueType>* nextX = &walkerChaeData->newX;

    std::vector<ValueType> tmp = walkerChaeData->matrix.getRowSumVector();
    storm::utility::vector::applyPointwise(tmp, walkerChaeData->b, walkerChaeData->b,
                                           [this](ValueType const& first, ValueType const& second) -> ValueType { return walkerChaeData->t * first + second; });

    // Add t to all entries of x.
    storm::utility::vector::applyPointwise(x, x, [this](ValueType const& value) -> ValueType { return value + walkerChaeData->t; });

    // Create a vector that always holds Ax.
    std::vector<ValueType> currentAx(x.size());
    walkerChaeData->multiplier->multiply(env, *currentX, nullptr, currentAx);

    // (3) Perform iterations until convergence.
    SolverStatus status = SolverStatus::InProgress;
    uint64_t iterations = 0;
    this->startMeasureProgress();
    while (status == SolverStatus::InProgress && iterations < maxIter) {
        // Perform one Walker-Chae step.
        walkerChaeData->matrix.performWalkerChaeStep(*currentX, walkerChaeData->columnSums, walkerChaeData->b, currentAx, *nextX);

        // Compute new Ax.
        walkerChaeData->multiplier->multiply(env, *nextX, nullptr, currentAx);

        // Check for convergence.
        if (storm::utility::vector::computeSquaredNorm2Difference(currentAx, walkerChaeData->b) <= squaredErrorBound) {
            status = SolverStatus::Converged;
        }

        // Swap the x vectors for the next iteration.
        std::swap(currentX, nextX);

        // Potentially show progress.
        this->showProgressIterative(iterations);

        // Increase iteration count so we can abort if convergence is too slow.
        ++iterations;

        if (storm::utility::resources::isTerminate()) {
            status = SolverStatus::Aborted;
        }
    }

    // If the last iteration did not write to the original x we have to swap the contents, because the
    // output has to be written to the input parameter x.
    if (currentX == &walkerChaeData->newX) {
        std::swap(x, *currentX);
    }

    // Resize the solution to the right size.
    x.resize(this->A->getRowCount());

    // Finalize solution vector.
    storm::utility::vector::applyPointwise(x, x, [this](ValueType const& value) -> ValueType { return value - walkerChaeData->t; });

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    this->reportStatus(status, iterations);

    return status == SolverStatus::Converged;
}

template<typename ValueType>
typename NativeLinearEquationSolver<ValueType>::PowerIterationResult NativeLinearEquationSolver<ValueType>::performPowerIteration(
    Environment const& env, std::vector<ValueType>*& currentX, std::vector<ValueType>*& newX, std::vector<ValueType> const& b, ValueType const& precision,
    bool relative, SolverGuarantee const& guarantee, uint64_t currentIterations, uint64_t maxIterations,
    storm::solver::MultiplicationStyle const& multiplicationStyle) const {
    bool useGaussSeidelMultiplication = multiplicationStyle == storm::solver::MultiplicationStyle::GaussSeidel;

    uint64_t iterations = currentIterations;
    SolverStatus status = this->terminateNow(*currentX, guarantee) ? SolverStatus::TerminatedEarly : SolverStatus::InProgress;
    while (status == SolverStatus::InProgress && iterations < maxIterations) {
        if (useGaussSeidelMultiplication) {
            *newX = *currentX;
            this->multiplier->multiplyGaussSeidel(env, *newX, &b);
        } else {
            this->multiplier->multiply(env, *currentX, &b, *newX);
        }

        // Check for convergence.
        if (storm::utility::vector::equalModuloPrecision<ValueType>(*currentX, *newX, precision, relative)) {
            status = SolverStatus::Converged;
        }

        // Check for termination.
        std::swap(currentX, newX);
        ++iterations;

        status = this->updateStatus(status, *currentX, guarantee, iterations, maxIterations);

        // Potentially show progress.
        this->showProgressIterative(iterations);
    }

    return PowerIterationResult(iterations - currentIterations, status);
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsPower(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with NativeLinearEquationSolver (Power)");
    // Prepare the solution vectors.
    setUpViOperator();

    SolverGuarantee guarantee = SolverGuarantee::None;
    if (this->hasCustomTerminationCondition()) {
        if (this->getTerminationCondition().requiresGuarantee(SolverGuarantee::LessOrEqual) && this->hasLowerBound()) {
            this->createLowerBoundsVector(x);
            guarantee = SolverGuarantee::LessOrEqual;
        } else if (this->getTerminationCondition().requiresGuarantee(SolverGuarantee::GreaterOrEqual) && this->hasUpperBound()) {
            this->createUpperBoundsVector(x);
            guarantee = SolverGuarantee::GreaterOrEqual;
        }
    }

    storm::solver::helper::ValueIterationHelper<ValueType, true> viHelper(viOperator);
    uint64_t numIterations{0};
    auto viCallback = [&](SolverStatus const& current) {
        this->showProgressIterative(numIterations);
        return this->updateStatus(current, x, guarantee, numIterations, env.solver().native().getMaximalNumberOfIterations());
    };
    this->startMeasureProgress();
    auto status = viHelper.VI(x, b, numIterations, env.solver().native().getRelativeTerminationCriterion(),
                              storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision()), {}, viCallback,
                              env.solver().native().getPowerMethodMultiplicationStyle());

    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
void preserveOldRelevantValues(std::vector<ValueType> const& allValues, storm::storage::BitVector const& relevantValues, std::vector<ValueType>& oldValues) {
    storm::utility::vector::selectVectorValues(oldValues, relevantValues, allValues);
}

template<typename ValueType>
ValueType computeMaxAbsDiff(std::vector<ValueType> const& allValues, storm::storage::BitVector const& relevantValues, std::vector<ValueType> const& oldValues) {
    ValueType result = storm::utility::zero<ValueType>();
    auto oldValueIt = oldValues.begin();
    for (uint64_t value : relevantValues) {
        result = storm::utility::max<ValueType>(result, storm::utility::abs<ValueType>(allValues[value] - *oldValueIt));
    }
    return result;
}

template<typename ValueType>
ValueType computeMaxAbsDiff(std::vector<ValueType> const& allOldValues, std::vector<ValueType> const& allNewValues,
                            storm::storage::BitVector const& relevantValues) {
    ValueType result = storm::utility::zero<ValueType>();
    for (uint64_t value : relevantValues) {
        result = storm::utility::max<ValueType>(result, storm::utility::abs<ValueType>(allNewValues[value] - allOldValues[value]));
    }
    return result;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsIntervalIteration(Environment const& env, std::vector<ValueType>& x,
                                                                            std::vector<ValueType> const& b) const {
    STORM_LOG_THROW(this->hasLowerBound(), storm::exceptions::UnmetRequirementException, "Solver requires lower bound, but none was given.");
    STORM_LOG_THROW(this->hasUpperBound(), storm::exceptions::UnmetRequirementException, "Solver requires upper bound, but none was given.");
    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with NativeLinearEquationSolver (IntervalIteration)");
    setUpViOperator();
    helper::IntervalIterationHelper<ValueType, true> iiHelper(viOperator);
    auto prec = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    auto lowerBoundsCallback = [&](std::vector<ValueType>& vector) { this->createLowerBoundsVector(vector); };
    auto upperBoundsCallback = [&](std::vector<ValueType>& vector) { this->createUpperBoundsVector(vector); };

    uint64_t numIterations{0};
    auto iiCallback = [&](helper::IIData<ValueType> const& data) {
        this->showProgressIterative(numIterations);
        bool terminateEarly = this->hasCustomTerminationCondition() && this->getTerminationCondition().terminateNow(data.x, SolverGuarantee::LessOrEqual) &&
                              this->getTerminationCondition().terminateNow(data.y, SolverGuarantee::GreaterOrEqual);
        return this->updateStatus(data.status, terminateEarly, numIterations, env.solver().native().getMaximalNumberOfIterations());
    };
    std::optional<storm::storage::BitVector> optionalRelevantValues;
    if (this->hasRelevantValues()) {
        optionalRelevantValues = this->getRelevantValues();
    }
    this->startMeasureProgress();
    auto status = iiHelper.II(x, b, numIterations, env.solver().native().getRelativeTerminationCriterion(), prec, lowerBoundsCallback, upperBoundsCallback, {},
                              iiCallback, optionalRelevantValues);
    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsSoundValueIteration(Environment const& env, std::vector<ValueType>& x,
                                                                              std::vector<ValueType> const& b) const {
    // Prepare the solution vectors and the helper.
    assert(x.size() == this->A->getRowGroupCount());

    std::optional<ValueType> lowerBound, upperBound;
    if (this->hasLowerBound()) {
        lowerBound = this->getLowerBound(true);
    }
    if (this->hasUpperBound()) {
        upperBound = this->getUpperBound(true);
    }

    setUpViOperator();

    auto precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    uint64_t numIterations{0};
    auto sviCallback = [&](typename helper::SoundValueIterationHelper<ValueType, true>::SVIData const& current) {
        this->showProgressIterative(numIterations);
        return this->updateStatus(current.status,
                                  this->hasCustomTerminationCondition() && current.checkCustomTerminationCondition(this->getTerminationCondition()),
                                  numIterations, env.solver().native().getMaximalNumberOfIterations());
    };
    std::optional<storm::storage::BitVector> optionalRelevantValues;
    if (this->hasRelevantValues()) {
        optionalRelevantValues = this->getRelevantValues();
    }
    this->startMeasureProgress();
    helper::SoundValueIterationHelper<ValueType, true> sviHelper(viOperator);
    auto status = sviHelper.SVI(x, b, numIterations, env.solver().native().getRelativeTerminationCriterion(), precision, {}, lowerBound, upperBound,
                                sviCallback, optionalRelevantValues);

    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsOptimisticValueIteration(Environment const& env, std::vector<ValueType>& x,
                                                                                   std::vector<ValueType> const& b) const {
    if (!storm::utility::vector::hasNonZeroEntry(b)) {
        // If all entries are zero, OVI might run in an endless loop. However, the result is easy in this case.
        x.assign(x.size(), storm::utility::zero<ValueType>());
        return true;
    }

    setUpViOperator();

    helper::OptimisticValueIterationHelper<ValueType, true> oviHelper(viOperator);
    auto prec = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    std::optional<ValueType> lowerBound, upperBound;
    if (this->hasLowerBound()) {
        lowerBound = this->getLowerBound(true);
    }
    if (this->hasUpperBound()) {
        upperBound = this->getUpperBound(true);
    }
    uint64_t numIterations{0};
    auto oviCallback = [&](SolverStatus const& current, std::vector<ValueType> const& v) {
        this->showProgressIterative(numIterations);
        return this->updateStatus(current, v, SolverGuarantee::LessOrEqual, numIterations, env.solver().native().getMaximalNumberOfIterations());
    };
    this->createLowerBoundsVector(x);
    std::optional<ValueType> guessingFactor;
    if (env.solver().ovi().getUpperBoundGuessingFactor()) {
        guessingFactor = storm::utility::convertNumber<ValueType>(*env.solver().ovi().getUpperBoundGuessingFactor());
    }
    this->startMeasureProgress();
    auto status = oviHelper.OVI(x, b, env.solver().native().getRelativeTerminationCriterion(), prec, {}, guessingFactor, lowerBound, upperBound, oviCallback);
    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsGuessingValueIteration(const Environment& env, std::vector<ValueType>& x,
                                                                                 const std::vector<ValueType>& b) const {
    if (!this->cachedRowVector) {
        this->cachedRowVector = std::make_unique<std::vector<ValueType>>(this->A->getRowCount());
    }
    setUpViOperator();

    std::vector<ValueType>* lowerX = &x;
    this->createLowerBoundsVector(*lowerX);
    this->createUpperBoundsVector(this->cachedRowVector, this->getMatrixRowCount());
    std::vector<ValueType>* upperX = this->cachedRowVector.get();

    storm::solver::helper::GuessingValueIterationHelper<ValueType, true> helper(viOperator, *this->A);

    uint64_t numIterations{0};
    auto gviCallback = [&](helper::GVIData<ValueType> const& data) {
        this->showProgressIterative(numIterations);
        bool terminateEarly = this->hasCustomTerminationCondition() && this->getTerminationCondition().terminateNow(data.x, SolverGuarantee::LessOrEqual) &&
                              this->getTerminationCondition().terminateNow(data.y, SolverGuarantee::GreaterOrEqual);
        return this->updateStatus(data.status, terminateEarly, numIterations, env.solver().native().getMaximalNumberOfIterations());
    };

    this->startMeasureProgress();
    auto status = helper.solveEquations(*lowerX, *upperX, b, numIterations, storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision()),
                                        {},  // No optimization dir
                                        gviCallback);
    auto two = storm::utility::convertNumber<ValueType>(2.0);
    storm::utility::vector::applyPointwise<ValueType, ValueType, ValueType>(
        *lowerX, *upperX, x, [&two](ValueType const& first, ValueType const& second) -> ValueType { return (first + second) / two; });
    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }
    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::solveEquationsRationalSearch(Environment const& env, std::vector<ValueType>& x,
                                                                         std::vector<ValueType> const& b) const {
    // Set up two value iteration operators. One for exact and one for imprecise computations
    setUpViOperator();
    std::shared_ptr<helper::ValueIterationOperator<storm::RationalNumber, true>> exactOp;
    std::shared_ptr<helper::ValueIterationOperator<double, true>> impreciseOp;

    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        exactOp = viOperator;
        impreciseOp = std::make_shared<helper::ValueIterationOperator<double, true>>();
        impreciseOp->setMatrixBackwards(this->A->template toValueType<double>());
    } else {
        impreciseOp = viOperator;
        exactOp = std::make_shared<helper::ValueIterationOperator<storm::RationalNumber, true>>();
        exactOp->setMatrixBackwards(this->A->template toValueType<storm::RationalNumber>());
    }

    storm::solver::helper::RationalSearchHelper<ValueType, storm::RationalNumber, double, true> rsHelper(exactOp, impreciseOp);
    uint64_t numIterations{0};
    auto rsCallback = [&](SolverStatus const& current) {
        this->showProgressIterative(numIterations);
        return this->updateStatus(current, x, SolverGuarantee::None, numIterations, env.solver().native().getMaximalNumberOfIterations());
    };
    this->startMeasureProgress();
    auto status = rsHelper.RS(x, b, numIterations, storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision()), {}, rsCallback);

    this->reportStatus(status, numIterations);

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return status == SolverStatus::Converged || status == SolverStatus::TerminatedEarly;
}

template<typename ValueType>
NativeLinearEquationSolverMethod NativeLinearEquationSolver<ValueType>::getMethod(Environment const& env, bool isExactMode) const {
    // Adjust the method if none was specified and we want exact or sound computations
    auto method = env.solver().native().getMethod();

    if (isExactMode && method != NativeLinearEquationSolverMethod::RationalSearch) {
        if (env.solver().native().isMethodSetFromDefault()) {
            method = NativeLinearEquationSolverMethod::RationalSearch;
            STORM_LOG_INFO(
                "Selecting '" + toString(method) +
                "' as the solution technique to guarantee exact results. If you want to override this, please explicitly specify a different method.");
        } else {
            STORM_LOG_WARN("The selected solution method does not guarantee exact results.");
        }
    } else if (env.solver().isForceSoundness() && method != NativeLinearEquationSolverMethod::SoundValueIteration &&
               method != NativeLinearEquationSolverMethod::OptimisticValueIteration && method != NativeLinearEquationSolverMethod::IntervalIteration &&
               method != NativeLinearEquationSolverMethod::RationalSearch) {
        if (env.solver().native().isMethodSetFromDefault()) {
            method = NativeLinearEquationSolverMethod::OptimisticValueIteration;
            STORM_LOG_INFO(
                "Selecting '" + toString(method) +
                "' as the solution technique to guarantee sound results. If you want to override this, please explicitly specify a different method.");
        } else {
            STORM_LOG_WARN("The selected solution method does not guarantee sound results.");
        }
    }
    return method;
}

template<typename ValueType>
bool NativeLinearEquationSolver<ValueType>::internalSolveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    switch (getMethod(env, storm::NumberTraits<ValueType>::IsExact || env.solver().isForceExact())) {
        case NativeLinearEquationSolverMethod::SOR:
            return this->solveEquationsSOR(env, x, b, storm::utility::convertNumber<ValueType>(env.solver().native().getSorOmega()));
        case NativeLinearEquationSolverMethod::GaussSeidel:
            return this->solveEquationsSOR(env, x, b, storm::utility::one<ValueType>());
        case NativeLinearEquationSolverMethod::Jacobi:
            return this->solveEquationsJacobi(env, x, b);
        case NativeLinearEquationSolverMethod::WalkerChae:
            return this->solveEquationsWalkerChae(env, x, b);
        case NativeLinearEquationSolverMethod::Power:
            return this->solveEquationsPower(env, x, b);
        case NativeLinearEquationSolverMethod::SoundValueIteration:
            return this->solveEquationsSoundValueIteration(env, x, b);
        case NativeLinearEquationSolverMethod::OptimisticValueIteration:
            return this->solveEquationsOptimisticValueIteration(env, x, b);
        case NativeLinearEquationSolverMethod::GuessingValueIteration:
            return this->solveEquationsGuessingValueIteration(env, x, b);
        case NativeLinearEquationSolverMethod::IntervalIteration:
            return this->solveEquationsIntervalIteration(env, x, b);
        case NativeLinearEquationSolverMethod::RationalSearch:
            return this->solveEquationsRationalSearch(env, x, b);
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solving technique.");
    return false;
}

template<typename ValueType>
LinearEquationSolverProblemFormat NativeLinearEquationSolver<ValueType>::getEquationProblemFormat(Environment const& env) const {
    auto method = getMethod(env, storm::NumberTraits<ValueType>::IsExact || env.solver().isForceExact());
    if (method == NativeLinearEquationSolverMethod::Power || method == NativeLinearEquationSolverMethod::SoundValueIteration ||
        method == NativeLinearEquationSolverMethod::OptimisticValueIteration || method == NativeLinearEquationSolverMethod::RationalSearch ||
        method == NativeLinearEquationSolverMethod::IntervalIteration || method == NativeLinearEquationSolverMethod::GuessingValueIteration) {
        return LinearEquationSolverProblemFormat::FixedPointSystem;
    } else {
        return LinearEquationSolverProblemFormat::EquationSystem;
    }
}

template<typename ValueType>
LinearEquationSolverRequirements NativeLinearEquationSolver<ValueType>::getRequirements(Environment const& env) const {
    LinearEquationSolverRequirements requirements;
    auto method = getMethod(env, storm::NumberTraits<ValueType>::IsExact || env.solver().isForceExact());
    if (method == NativeLinearEquationSolverMethod::IntervalIteration || method == NativeLinearEquationSolverMethod::GuessingValueIteration) {
        requirements.requireBounds();
    } else if (method == NativeLinearEquationSolverMethod::RationalSearch || method == NativeLinearEquationSolverMethod::OptimisticValueIteration) {
        requirements.requireLowerBounds();
    } else if (method == NativeLinearEquationSolverMethod::SoundValueIteration) {
        requirements.requireBounds(false);
    }
    return requirements;
}

template<typename ValueType>
void NativeLinearEquationSolver<ValueType>::clearCache() const {
    jacobiDecomposition.reset();
    walkerChaeData.reset();
    multiplier.reset();
    viOperator.reset();
    LinearEquationSolver<ValueType>::clearCache();
}

template<typename ValueType>
uint64_t NativeLinearEquationSolver<ValueType>::getMatrixRowCount() const {
    return this->A->getRowCount();
}

template<typename ValueType>
uint64_t NativeLinearEquationSolver<ValueType>::getMatrixColumnCount() const {
    return this->A->getColumnCount();
}

template<typename ValueType>
std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> NativeLinearEquationSolverFactory<ValueType>::create(Environment const&) const {
    return std::make_unique<storm::solver::NativeLinearEquationSolver<ValueType>>();
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolverFactory<ValueType>> NativeLinearEquationSolverFactory<ValueType>::clone() const {
    return std::make_unique<NativeLinearEquationSolverFactory<ValueType>>(*this);
}

// Explicitly instantiate the linear equation solver.
template class NativeLinearEquationSolver<double>;
template class NativeLinearEquationSolverFactory<double>;

template class NativeLinearEquationSolver<storm::RationalNumber>;
template class NativeLinearEquationSolverFactory<storm::RationalNumber>;

}  // namespace solver
}  // namespace storm
