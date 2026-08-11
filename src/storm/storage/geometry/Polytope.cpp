#include "storm/storage/geometry/Polytope.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/geometry/NativePolytope.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::create(std::vector<storm::storage::geometry::Halfspace<ValueType>> const& halfspaces) {
    return create(halfspaces, boost::none);
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::create(std::vector<Point> const& points) {
    return create(boost::none, points);
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::createUniversalPolytope() {
    return create(std::vector<Halfspace<ValueType>>(), boost::none);
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::createEmptyPolytope() {
    return create(boost::none, std::vector<Point>());
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::create(boost::optional<std::vector<Halfspace<ValueType>>> const& halfspaces,
                                                                 boost::optional<std::vector<Point>> const& points) {
    return NativePolytope<ValueType>::create(halfspaces, points);
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::createDownwardClosure(std::vector<Point> const& points) {
    if (points.empty()) {
        // In this case, the downwardclosure is empty
        return createEmptyPolytope();
    }
    // Reduce this call to a more general method
    storm::storage::BitVector dimensions(points.front().size(), true);
    return createSelectiveDownwardClosure(points, dimensions);
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::createSelectiveDownwardClosure(std::vector<Point> const& points,
                                                                                         storm::storage::BitVector const& selectedDimensions) {
    if (points.empty()) {
        // In this case, the downwardclosure is empty
        return createEmptyPolytope();
    }
    if (selectedDimensions.empty()) {
        return create(points);
    }
    STORM_LOG_ASSERT(points.front().size() == selectedDimensions.size(), "Point dimensions do not match selected dimensions.");

    std::vector<Halfspace<ValueType>> halfspaces;
    // We build the convex hull of the given points.
    // However, auxiliary points (that will always be in the selective downward closure) are added.
    // Then, the halfspaces of the resulting polytope are a superset of the halfspaces of the downward closure.
    std::vector<Point> auxiliaryPoints = points;
    auxiliaryPoints.reserve(auxiliaryPoints.size() * (1 + selectedDimensions.getNumberOfSetBits()));
    for (auto const& point : points) {
        for (auto dim : selectedDimensions) {
            auxiliaryPoints.push_back(point);
            auxiliaryPoints.back()[dim] -= storm::utility::one<ValueType>();
        }
    }
    std::vector<Halfspace<ValueType>> auxiliaryHalfspaces = create(auxiliaryPoints)->getHalfspaces();
    // The downward closure is obtained by erasing the halfspaces for which the normal vector is negative for one of the selected dimensions.
    for (auto& h : auxiliaryHalfspaces) {
        bool allGreaterEqZero = true;
        for (auto dim : selectedDimensions) {
            allGreaterEqZero &= (h.normalVector()[dim] >= storm::utility::zero<ValueType>());
        }
        if (allGreaterEqZero) {
            halfspaces.push_back(std::move(h));
        }
    }
    return create(halfspaces);
}

template<typename ValueType>
Polytope<ValueType>::Polytope() {
    // Intentionally left empty
}

template<typename ValueType>
Polytope<ValueType>::~Polytope() {
    // Intentionally left empty
}

template<>
std::vector<typename Polytope<storm::RationalNumber>::Point> Polytope<storm::RationalNumber>::getVerticesInClockwiseOrder() const {
    std::vector<Point> vertices = getVertices();
    if (vertices.size() <= 2) {
        // In this case, every ordering is clockwise
        return vertices;
    }
    STORM_LOG_THROW(vertices.front().size() == 2, storm::exceptions::IllegalFunctionCallException,
                    "Getting Vertices in clockwise order is only possible for a 2D-polytope.");

    std::vector<storm::storage::BitVector> neighborsOfVertices(vertices.size(), storm::storage::BitVector(vertices.size(), false));
    std::vector<Halfspace<storm::RationalNumber>> halfspaces = getHalfspaces();
    for (auto const& h : halfspaces) {
        storm::storage::BitVector verticesOnHalfspace(vertices.size(), false);
        for (uint_fast64_t v = 0; v < vertices.size(); ++v) {
            if (h.isPointOnBoundary(vertices[v])) {
                verticesOnHalfspace.set(v);
            }
        }
        for (auto v : verticesOnHalfspace) {
            neighborsOfVertices[v] |= verticesOnHalfspace;
            neighborsOfVertices[v].set(v, false);
        }
    }

    std::vector<Point> result;
    result.reserve(vertices.size());
    storm::storage::BitVector unprocessedVertices(vertices.size(), true);

    uint_fast64_t currentVertex = 0;
    for (uint_fast64_t v = 1; v < vertices.size(); ++v) {
        if (vertices[v].front() < vertices[currentVertex].front()) {
            currentVertex = v;
        }
    }
    STORM_LOG_ASSERT(neighborsOfVertices[currentVertex].getNumberOfSetBits() == 2,
                     "For 2D Polytopes with at least 3 vertices, each vertex should have exactly 2 neighbors.");
    uint_fast64_t firstNeighbor = neighborsOfVertices[currentVertex].getNextSetIndex(0);
    uint_fast64_t secondNeighbor = neighborsOfVertices[currentVertex].getNextSetIndex(firstNeighbor + 1);
    uint_fast64_t previousVertex = vertices[firstNeighbor].back() <= vertices[secondNeighbor].back() ? firstNeighbor : secondNeighbor;
    do {
        unprocessedVertices.set(currentVertex, false);
        result.push_back(std::move(vertices[currentVertex]));

        STORM_LOG_ASSERT(neighborsOfVertices[currentVertex].getNumberOfSetBits() == 2,
                         "For 2D Polytopes with at least 3 vertices, each vertex should have exactly 2 neighbors.");
        firstNeighbor = neighborsOfVertices[currentVertex].getNextSetIndex(0);
        secondNeighbor = neighborsOfVertices[currentVertex].getNextSetIndex(firstNeighbor + 1);
        uint_fast64_t nextVertex = firstNeighbor != previousVertex ? firstNeighbor : secondNeighbor;
        previousVertex = currentVertex;
        currentVertex = nextVertex;
    } while (!unprocessedVertices.empty());

    return result;
}

template<typename ValueType>
std::vector<typename Polytope<ValueType>::Point> Polytope<ValueType>::getVerticesInClockwiseOrder() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not implemented.");
    // Note that the implementation for RationalNumber above only works for exact ValueType since
    // checking whether the distance between halfspace and point is zero is problematic otherwise.
    return std::vector<Point>();
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::shift(Point const& b) const {
    // perform an affine transformation with identity matrix
    std::vector<Point> idMatrix(b.size(), Point(b.size(), storm::utility::zero<ValueType>()));
    for (uint64_t i = 0; i < b.size(); ++i) {
        idMatrix[i][i] = storm::utility::one<ValueType>();
    }
    return affineTransformation(idMatrix, b);
}

template<typename ValueType>
std::vector<std::shared_ptr<Polytope<ValueType>>> Polytope<ValueType>::setMinus(std::shared_ptr<Polytope<ValueType>> const& rhs) const {
    std::vector<std::shared_ptr<Polytope<ValueType>>> result;
    auto rhsHalfspaces = rhs->getHalfspaces();
    std::shared_ptr<Polytope<ValueType>> remaining = nullptr;
    for (auto const& h : rhsHalfspaces) {
        Polytope<ValueType> const& ref = (remaining == nullptr) ? *this : *remaining;
        auto next = ref.intersection(h.invert());
        if (!next->isEmpty()) {
            result.push_back(next);
        }
        remaining = ref.intersection(h);
        if (remaining->isEmpty()) {
            break;
        }
    }
    return result;
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::downwardClosure() const {
    return createDownwardClosure(this->getVertices());
}

template<typename ValueType>
std::vector<storm::expressions::Variable> Polytope<ValueType>::declareVariables(storm::expressions::ExpressionManager& manager,
                                                                                std::string const& namePrefix) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not implemented for this polytope implementation.");
    std::vector<storm::expressions::Variable> result;
    return result;
}

template<typename ValueType>
std::vector<storm::expressions::Expression> Polytope<ValueType>::getConstraints(storm::expressions::ExpressionManager const& manager,
                                                                                std::vector<storm::expressions::Variable> const& variables) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not implemented for this polytope implementation.");
    std::vector<storm::expressions::Expression> result;
    return result;
}

template<typename ValueType>
template<typename TargetType>
std::shared_ptr<Polytope<TargetType>> Polytope<ValueType>::convertNumberRepresentation() const {
    if (isEmpty()) {
        return Polytope<TargetType>::createEmptyPolytope();
    }
    auto halfspaces = getHalfspaces();
    std::vector<Halfspace<TargetType>> halfspacesPrime;
    halfspacesPrime.reserve(halfspaces.size());
    for (auto const& h : halfspaces) {
        halfspacesPrime.emplace_back(storm::utility::vector::convertNumericVector<TargetType>(h.normalVector()),
                                     storm::utility::convertNumber<TargetType>(h.offset()));
    }

    return Polytope<TargetType>::create(halfspacesPrime);
}

template<typename ValueType>
std::string Polytope<ValueType>::toString(bool numbersAsDouble) const {
    auto halfspaces = this->getHalfspaces();
    std::stringstream stream;
    stream << "Polytope with " << halfspaces.size() << " Halfspaces" << (halfspaces.empty() ? "" : ":") << '\n';
    for (auto const& h : halfspaces) {
        stream << "   " << h.toString(numbersAsDouble) << '\n';
    }
    return stream.str();
}

template<typename ValueType>
bool Polytope<ValueType>::isNativePolytope() const {
    return false;
}

template<typename ValueType>
std::shared_ptr<Polytope<ValueType>> Polytope<ValueType>::clean() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not implemented for this polytope type.");
    return nullptr;
}

template class Polytope<double>;
template std::shared_ptr<Polytope<double>> Polytope<double>::convertNumberRepresentation() const;

template class Polytope<storm::RationalNumber>;
template std::shared_ptr<Polytope<double>> Polytope<storm::RationalNumber>::convertNumberRepresentation() const;
template std::shared_ptr<Polytope<storm::RationalNumber>> Polytope<double>::convertNumberRepresentation() const;
template std::shared_ptr<Polytope<storm::RationalNumber>> Polytope<storm::RationalNumber>::convertNumberRepresentation() const;
}  // namespace geometry
}  // namespace storage
}  // namespace storm
