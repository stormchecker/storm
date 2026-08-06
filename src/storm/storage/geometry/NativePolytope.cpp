#include "storm/storage/geometry/NativePolytope.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/solver/Z3LpSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/geometry/nativepolytopeconversion/HyperplaneEnumeration.h"
#include "storm/storage/geometry/nativepolytopeconversion/QuickHull.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(std::vector<Halfspace<ValueType>> const& halfspaces) {
    if (halfspaces.empty()) {
        // The polytope is universal
        emptyStatus = EmptyStatus::Nonempty;
    } else {
        Eigen::Index maxCol = halfspaces.front().normalVector().size();
        Eigen::Index maxRow = halfspaces.size();
        A = EigenMatrix(maxRow, maxCol);
        b = EigenVector(maxRow);
        for (Eigen::Index row = 0; row < A.rows(); ++row) {
            assert((Eigen::Index)halfspaces[row].normalVector().size() == maxCol);
            b(row) = halfspaces[row].offset();
            A.row(row) = storm::adapters::EigenAdapter::toEigenVector(halfspaces[row].normalVector());
        }
        emptyStatus = EmptyStatus::Unknown;
    }
}

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(std::vector<Point> const& points) {
    if (points.empty()) {
        emptyStatus = EmptyStatus::Empty;
    } else {
        std::vector<EigenVector> eigenPoints;
        eigenPoints.reserve(points.size());
        for (auto const& p : points) {
            eigenPoints.emplace_back(storm::adapters::EigenAdapter::toEigenVector(p));
        }

        storm::storage::geometry::QuickHull<ValueType> qh;
        qh.generateHalfspacesFromPoints(eigenPoints, false);
        A = std::move(qh.getResultMatrix());
        b = std::move(qh.getResultVector());
        emptyStatus = EmptyStatus::Nonempty;
    }
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::create(boost::optional<std::vector<Halfspace<ValueType>>> const& halfspaces,
                                                                       boost::optional<std::vector<Point>> const& points) {
    if (halfspaces) {
        STORM_LOG_WARN_COND(!points, "Creating a NativePolytope where halfspaces AND points are given. The points will be ignored.");
        return std::make_shared<NativePolytope<ValueType>>(*halfspaces);
    } else if (points) {
        return std::make_shared<NativePolytope<ValueType>>(*points);
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Creating a NativePolytope but no representation was given.");
    return nullptr;
}

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(NativePolytope<ValueType> const& other) : emptyStatus(other.emptyStatus), A(other.A), b(other.b) {
    // Intentionally left empty
}

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(NativePolytope<ValueType>&& other)
    : emptyStatus(std::move(other.emptyStatus)), A(std::move(other.A)), b(std::move(other.b)) {
    // Intentionally left empty
}

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(EmptyStatus const& emptyStatus, EigenMatrix const& halfspaceMatrix, EigenVector const& halfspaceVector)
    : emptyStatus(emptyStatus), A(halfspaceMatrix), b(halfspaceVector) {
    // Intentionally left empty
}

template<typename ValueType>
NativePolytope<ValueType>::NativePolytope(EmptyStatus&& emptyStatus, EigenMatrix&& halfspaceMatrix, EigenVector&& halfspaceVector)
    : emptyStatus(emptyStatus), A(halfspaceMatrix), b(halfspaceVector) {
    // Intentionally left empty
}

template<typename ValueType>
NativePolytope<ValueType>::~NativePolytope() {
    // Intentionally left empty
}

template<typename ValueType>
std::vector<typename Polytope<ValueType>::Point> NativePolytope<ValueType>::getVertices() const {
    std::vector<EigenVector> eigenVertices = getEigenVertices();
    std::vector<Point> result;
    result.reserve(eigenVertices.size());
    for (auto const& p : eigenVertices) {
        result.push_back(storm::adapters::EigenAdapter::toStdVector(p));
    }
    return result;
}

template<typename ValueType>
std::vector<Halfspace<ValueType>> NativePolytope<ValueType>::getHalfspaces() const {
    std::vector<Halfspace<ValueType>> result;
    result.reserve(A.rows());

    for (Eigen::Index row = 0; row < A.rows(); ++row) {
        result.emplace_back(storm::adapters::EigenAdapter::toStdVector(EigenVector(A.row(row))), b(row));
    }
    return result;
}

template<typename ValueType>
bool NativePolytope<ValueType>::isEmpty() const {
    if (emptyStatus == EmptyStatus::Unknown) {
        std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
        std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::SmtSolverFactory().create(*manager);
        std::vector<storm::expressions::Expression> constraints = getConstraints(*manager, declareVariables(*manager, "x"));
        for (auto const& constraint : constraints) {
            solver->add(constraint);
        }
        switch (solver->check()) {
            case storm::solver::SmtSolver::CheckResult::Sat:
                emptyStatus = EmptyStatus::Nonempty;
                break;
            case storm::solver::SmtSolver::CheckResult::Unsat:
                emptyStatus = EmptyStatus::Empty;
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected result of SMT solver during emptyness-check of Polytope.");
                break;
        }
    }
    return emptyStatus == EmptyStatus::Empty;
}

template<typename ValueType>
bool NativePolytope<ValueType>::isUniversal() const {
    return A.rows() == 0;
}

template<typename ValueType>
bool NativePolytope<ValueType>::contains(Point const& point) const {
    EigenVector x = storm::adapters::EigenAdapter::toEigenVector(point);
    for (Eigen::Index row = 0; row < A.rows(); ++row) {
        if ((A.row(row) * x)(0) > b(row)) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool NativePolytope<ValueType>::contains(std::shared_ptr<Polytope<ValueType>> const& other) const {
    STORM_LOG_THROW(other->isNativePolytope(), storm::exceptions::InvalidArgumentException,
                    "Invoked operation between a NativePolytope and a different polytope implementation. This is not supported.");
    if (this->isUniversal()) {
        return true;
    } else if (other->isUniversal()) {
        return false;
    } else {
        // Check whether there is one point in other that is not in this
        std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
        std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::SmtSolverFactory().create(*manager);
        std::vector<storm::expressions::Variable> variables = declareVariables(*manager, "x");
        std::vector<storm::expressions::Expression> constraints = getConstraints(*manager, variables);
        storm::expressions::Expression constraintsThis = manager->boolean(true);
        for (auto const& constraint : constraints) {
            constraintsThis = constraintsThis && constraint;
        }
        solver->add(!constraintsThis);
        constraints = dynamic_cast<NativePolytope<ValueType> const&>(*other).getConstraints(*manager, variables);
        for (auto const& constraint : constraints) {
            solver->add(constraint);
        }
        switch (solver->check()) {
            case storm::solver::SmtSolver::CheckResult::Sat:
                return false;
            case storm::solver::SmtSolver::CheckResult::Unsat:
                return true;
            default:
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected result of SMT solver during containment check of two polytopes.");
                return false;
        }
    }
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::intersection(std::shared_ptr<Polytope<ValueType>> const& rhs) const {
    STORM_LOG_THROW(rhs->isNativePolytope(), storm::exceptions::InvalidArgumentException,
                    "Invoked operation between a NativePolytope and a different polytope implementation. This is not supported.");
    if (this->isUniversal()) {
        return rhs;
    } else if (rhs->isUniversal()) {
        return std::make_shared<NativePolytope<ValueType>>(*this);
    } else {
        NativePolytope<ValueType> const& nativeRhs = dynamic_cast<NativePolytope<ValueType> const&>(*rhs);
        EigenMatrix resultA(A.rows() + nativeRhs.A.rows(), A.cols());
        resultA << A, nativeRhs.A;
        EigenVector resultb(resultA.rows());
        resultb << b, nativeRhs.b;
        return std::make_shared<NativePolytope<ValueType>>(EmptyStatus::Unknown, std::move(resultA), std::move(resultb));
    }
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::intersection(Halfspace<ValueType> const& halfspace) const {
    if (A.rows() == 0) {
        // No constraints yet
        EigenMatrix resultA = storm::adapters::EigenAdapter::toEigenVector(halfspace.normalVector()).transpose();
        EigenVector resultb(1);
        resultb(0) = halfspace.offset();
        return std::make_shared<NativePolytope<ValueType>>(EmptyStatus::Unknown, std::move(resultA), std::move(resultb));
    }
    EigenMatrix resultA(A.rows() + 1, A.cols());
    resultA << A, storm::adapters::EigenAdapter::toEigenVector(halfspace.normalVector()).transpose();
    EigenVector resultb(resultA.rows());
    resultb << b, halfspace.offset();
    return std::make_shared<NativePolytope<ValueType>>(EmptyStatus::Unknown, std::move(resultA), std::move(resultb));
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::convexUnion(std::shared_ptr<Polytope<ValueType>> const& rhs) const {
    STORM_LOG_THROW(rhs->isNativePolytope(), storm::exceptions::InvalidArgumentException,
                    "Invoked operation between a NativePolytope and a different polytope implementation. This is not supported.");
    if (this->isEmpty()) {
        return std::make_shared<NativePolytope<ValueType>>(dynamic_cast<NativePolytope<ValueType> const&>(*rhs));
    } else if (rhs->isEmpty()) {
        return std::make_shared<NativePolytope<ValueType>>(*this);
    } else if (this->isUniversal() || rhs->isUniversal()) {
        return std::make_shared<NativePolytope<ValueType>>(std::vector<Halfspace<ValueType>>());
    }

    STORM_LOG_WARN_COND_DEBUG(false, "Implementation of convex union of two polytopes only works if the polytopes are bounded. This is not checked.");

    std::vector<EigenVector> rhsVertices = dynamic_cast<NativePolytope<ValueType> const&>(*rhs).getEigenVertices();
    std::vector<EigenVector> resultVertices = this->getEigenVertices();
    resultVertices.insert(resultVertices.end(), std::make_move_iterator(rhsVertices.begin()), std::make_move_iterator(rhsVertices.end()));

    storm::storage::geometry::QuickHull<ValueType> qh;
    qh.generateHalfspacesFromPoints(resultVertices, false);
    return std::make_shared<NativePolytope<ValueType>>(EmptyStatus::Nonempty, std::move(qh.getResultMatrix()), std::move(qh.getResultVector()));
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::minkowskiSum(std::shared_ptr<Polytope<ValueType>> const& rhs) const {
    STORM_LOG_THROW(rhs->isNativePolytope(), storm::exceptions::InvalidArgumentException,
                    "Invoked operation between a NativePolytope and a different polytope implementation. This is not supported.");
    NativePolytope<ValueType> const& nativeRhs = dynamic_cast<NativePolytope<ValueType> const&>(*rhs);

    if (this->isEmpty() || nativeRhs.isEmpty()) {
        return std::make_shared<NativePolytope<ValueType>>(std::vector<Point>());
    }

    std::vector<std::pair<EigenVector, ValueType>> resultConstraints;
    resultConstraints.reserve(A.rows() + nativeRhs.A.rows());

    // evaluation of rhs in directions of lhs
    for (Eigen::Index i = 0; i < A.rows(); ++i) {
        auto optimizationRes = nativeRhs.optimize(A.row(i));
        if (optimizationRes.second) {
            resultConstraints.emplace_back(A.row(i), b(i) + (A.row(i) * optimizationRes.first)(0));
        }
        // If optimizationRes.second is false, it means that rhs is unbounded in this direction, i.e., the current constraint is not inserted
    }

    // evaluation of lhs in directions of rhs
    for (Eigen::Index i = 0; i < nativeRhs.A.rows(); ++i) {
        auto optimizationRes = optimize(nativeRhs.A.row(i));
        if (optimizationRes.second) {
            resultConstraints.emplace_back(nativeRhs.A.row(i), nativeRhs.b(i) + (nativeRhs.A.row(i) * optimizationRes.first)(0));
        }
        // If optimizationRes.second is false, it means that rhs is unbounded in this direction, i.e., the current constraint is not inserted
    }

    if (resultConstraints.empty()) {
        return std::make_shared<NativePolytope<ValueType>>(std::vector<Halfspace<ValueType>>());
    } else {
        EigenMatrix newA(resultConstraints.size(), resultConstraints.front().first.rows());
        EigenVector newb(resultConstraints.size());
        for (Eigen::Index i = 0; i < newA.rows(); ++i) {
            newA.row(i) = resultConstraints[i].first;
            newb(i) = resultConstraints[i].second;
        }
        return std::make_shared<NativePolytope<ValueType>>(EmptyStatus::Nonempty, std::move(newA), std::move(newb));
    }
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::affineTransformation(std::vector<Point> const& matrix, Point const& vector) const {
    STORM_LOG_THROW(!matrix.empty(), storm::exceptions::InvalidArgumentException, "Invoked affine transformation with a matrix without rows.");
    Eigen::Index rows = matrix.size();
    Eigen::Index columns = matrix.front().size();
    EigenMatrix eigenMatrix(rows, columns);
    for (Eigen::Index row = 0; row < rows; ++row) {
        eigenMatrix.row(row) = storm::adapters::EigenAdapter::toEigenVector(matrix[row]);
    }
    EigenVector eigenVector = storm::adapters::EigenAdapter::toEigenVector(vector);

    Eigen::FullPivLU<EigenMatrix> luMatrix(eigenMatrix);
    STORM_LOG_THROW(luMatrix.isInvertible(), storm::exceptions::NotImplementedException,
                    "Affine Transformation of native polytope only implemented if the transformation matrix is invertable.");
    if (isUniversal()) {
        return std::make_shared<NativePolytope<ValueType>>(std::vector<Halfspace<ValueType>>());
    }
    EigenMatrix newA = A * luMatrix.inverse();
    EigenVector newb = b + (newA * eigenVector);
    return std::make_shared<NativePolytope<ValueType>>(emptyStatus, std::move(newA), std::move(newb));
}

template<typename ValueType>
std::pair<typename NativePolytope<ValueType>::Point, bool> NativePolytope<ValueType>::optimize(Point const& direction) const {
    if (isUniversal()) {
        return std::make_pair(Point(), false);
    }

    storm::solver::Z3LpSolver<ValueType> solver(storm::solver::OptimizationDirection::Maximize);
    std::vector<storm::expressions::Variable> variables;
    variables.reserve(A.cols());
    for (Eigen::Index i = 0; i < A.cols(); ++i) {
        variables.push_back(solver.addUnboundedContinuousVariable("x" + std::to_string(i), direction[i]));
    }
    std::vector<storm::expressions::Expression> constraints = getConstraints(solver.getManager(), variables);
    for (auto const& constraint : constraints) {
        solver.addConstraint("", constraint);
    }
    solver.update();
    solver.optimize();
    if (solver.isOptimal()) {
        auto result = std::make_pair(Point(), true);
        result.first.reserve(variables.size());
        for (auto const& var : variables) {
            result.first.push_back(solver.getContinuousValue(var));
        }
        return result;
    } else {
        // solution is infinity or infeasible
        return std::make_pair(Point(), false);
    }
}

template<typename ValueType>
std::pair<typename NativePolytope<ValueType>::EigenVector, bool> NativePolytope<ValueType>::optimize(EigenVector const& direction) const {
    if (isUniversal()) {
        return std::make_pair(EigenVector(), false);
    }

    storm::solver::Z3LpSolver<ValueType> solver(storm::solver::OptimizationDirection::Maximize);
    std::vector<storm::expressions::Variable> variables;
    variables.reserve(A.cols());
    for (Eigen::Index i = 0; i < A.cols(); ++i) {
        variables.push_back(solver.addUnboundedContinuousVariable("x" + std::to_string(i), static_cast<ValueType>(direction(i))));
    }
    std::vector<storm::expressions::Expression> constraints = getConstraints(solver.getManager(), variables);
    for (auto const& constraint : constraints) {
        solver.addConstraint("", constraint);
    }
    solver.update();
    solver.optimize();
    if (solver.isOptimal()) {
        auto result = std::make_pair(EigenVector(A.cols()), true);
        for (Eigen::Index i = 0; i < A.cols(); ++i) {
            result.first(i) = solver.getContinuousValue(variables[i]);
        }
        return result;
    } else {
        // solution is infinity or infeasible
        return std::make_pair(EigenVector(), false);
    }
}

template<typename ValueType>
bool NativePolytope<ValueType>::isNativePolytope() const {
    return true;
}
template<typename ValueType>
std::vector<typename NativePolytope<ValueType>::EigenVector> NativePolytope<ValueType>::getEigenVertices() const {
    storm::storage::geometry::HyperplaneEnumeration<ValueType> he;
    he.generateVerticesFromConstraints(A, b, false);
    return he.getResultVertices();
}

template<typename ValueType>
std::vector<storm::expressions::Variable> NativePolytope<ValueType>::declareVariables(storm::expressions::ExpressionManager& manager,
                                                                                      std::string const& namePrefix) const {
    std::vector<storm::expressions::Variable> result;
    result.reserve(A.cols());
    for (Eigen::Index col = 0; col < A.cols(); ++col) {
        result.push_back(manager.declareVariable(namePrefix + std::to_string(col), manager.getRationalType()));
    }
    return result;
}

template<typename ValueType>
std::vector<storm::expressions::Expression> NativePolytope<ValueType>::getConstraints(storm::expressions::ExpressionManager const& manager,
                                                                                      std::vector<storm::expressions::Variable> const& variables) const {
    std::vector<storm::expressions::Expression> result;
    for (Eigen::Index row = 0; row < A.rows(); ++row) {
        storm::expressions::Expression lhs = manager.rational(A(row, 0)) * variables[0].getExpression();
        for (Eigen::Index col = 1; col < A.cols(); ++col) {
            lhs = lhs + manager.rational(A(row, col)) * variables[col].getExpression();
        }
        result.push_back(lhs <= manager.rational(b(row)));
    }
    return result;
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> NativePolytope<ValueType>::clean() {
    if (isEmpty()) {
        return create(boost::none, {});
    }
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::SmtSolverFactory().create(*manager);
    auto variables = declareVariables(*manager, "x");
    std::vector<storm::expressions::Expression> constraints = getConstraints(*manager, variables);
    for (auto const& constraint : constraints) {
        solver->add(constraint);
    }
    storm::storage::BitVector keptConstraints(A.rows(), false);
    for (Eigen::Index row = 0; row < A.rows(); ++row) {
        storm::expressions::Expression lhs = manager->rational(A(row, 0)) * variables[0].getExpression();
        for (Eigen::Index col = 1; col < A.cols(); ++col) {
            lhs = lhs + manager->rational(A(row, col)) * variables[col].getExpression();
        }
        solver->push();
        solver->add(lhs >= manager->rational(b(row)));
        switch (solver->check()) {
            case storm::solver::SmtSolver::CheckResult::Sat:
                keptConstraints.set(row, true);
                break;
            case storm::solver::SmtSolver::CheckResult::Unsat:
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected result of SMT solver during emptyness-check of Polytope.");
                break;
        }
        solver->pop();
    }
    std::vector<Halfspace<ValueType>> newHalfspaces;
    newHalfspaces.reserve(keptConstraints.getNumberOfSetBits());
    for (auto row : keptConstraints) {
        newHalfspaces.emplace_back(storm::adapters::EigenAdapter::toStdVector(EigenVector(A.row(row))), b(row));
    }
    return create(newHalfspaces, boost::none);
}

template class NativePolytope<double>;
template class NativePolytope<storm::RationalNumber>;
}  // namespace geometry
}  // namespace storage
}  // namespace storm
