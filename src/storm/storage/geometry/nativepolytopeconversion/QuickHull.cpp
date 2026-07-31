#include "storm/storage/geometry/nativepolytopeconversion/QuickHull.h"

#include <algorithm>
#include <unordered_map>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/geometry/nativepolytopeconversion/HyperplaneCollector.h"
#include "storm/storage/geometry/nativepolytopeconversion/SubsetEnumerator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
void QuickHull<ValueType>::generateHalfspacesFromPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets) {
    STORM_LOG_ASSERT(!points.empty(), "Invoked QuickHull with empty set of points.");
    STORM_LOG_DEBUG("Invoked QuickHull on " << points.size() << " points");
    const uint_fast64_t dimension = points.front().rows();

    if (dimension == 1) {
        handle1DPoints(points, generateRelevantVerticesAndVertexSets);
    } else {
        // Generate initial set of d+1 affine independent points (if such a set exists)
        std::vector<uint_fast64_t> vertexIndices;
        if (this->findInitialVertices(points, vertexIndices)) {
            // compute point inside initial facet
            EigenVector insidePoint(EigenVector::Zero(dimension));
            for (uint_fast64_t vertexIndex : vertexIndices) {
                insidePoint += points[vertexIndex];
            }
            insidePoint /= storm::utility::convertNumber<ValueType>(static_cast<uint_fast64_t>(vertexIndices.size()));

            // Create the initial facets from the found vertices.
            std::vector<Facet> facets = computeInitialFacets(points, vertexIndices, insidePoint);

            // Enlarge the mesh
            storm::storage::BitVector currentFacets(facets.size(), true);
            this->extendMesh(points, facets, currentFacets, insidePoint);

            // Finally retrieve the resulting constrants
            this->getPolytopeFromMesh(points, facets, currentFacets, generateRelevantVerticesAndVertexSets);
            STORM_LOG_DEBUG("QuickHull invokation yielded " << resultMatrix.rows() << " constraints");
        } else {
            // The points are affine dependent. This special case needs to be handled accordingly.
            handleAffineDependentPoints(points, generateRelevantVerticesAndVertexSets);
        }
    }
}

template<typename ValueType>
typename QuickHull<ValueType>::EigenMatrix& QuickHull<ValueType>::getResultMatrix() {
    return this->resultMatrix;
}

template<typename ValueType>
typename QuickHull<ValueType>::EigenVector& QuickHull<ValueType>::getResultVector() {
    return this->resultVector;
}

template<typename ValueType>
std::vector<typename QuickHull<ValueType>::EigenVector>& QuickHull<ValueType>::getRelevantVertices() {
    return this->relevantVertices;
}

template<typename ValueType>
std::vector<std::vector<uint_fast64_t>>& QuickHull<ValueType>::getVertexSets() {
    return this->vertexSets;
}

template<typename ValueType>
void QuickHull<ValueType>::handle1DPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets) {
    ValueType minValue = points.front()(0);
    ValueType maxValue = points.front()(0);
    uint_fast64_t minIndex = 0;
    uint_fast64_t maxIndex = 0;
    for (uint_fast64_t pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
        ValueType const& pointValue = points[pointIndex](0);
        if (pointValue < minValue) {
            minValue = pointValue;
            minIndex = pointIndex;
        }
        if (pointValue > maxValue) {
            maxValue = pointValue;
            maxIndex = pointIndex;
        }
    }
    resultMatrix = EigenMatrix(2, 1);
    resultMatrix(0, 0) = -storm::utility::one<ValueType>();
    resultMatrix(1, 0) = storm::utility::one<ValueType>();
    resultVector = EigenVector(2);
    resultVector(0) = -minValue;
    resultVector(1) = maxValue;
    if (generateRelevantVerticesAndVertexSets) {
        relevantVertices.push_back(points[minIndex]);
        std::vector<uint_fast64_t> minVertexSet(1, 0);
        vertexSets.push_back(std::move(minVertexSet));
        if (minIndex != maxIndex && points[minIndex] != points[maxIndex]) {
            relevantVertices.push_back(points[maxIndex]);
            std::vector<uint_fast64_t> maxVertexSet(1, 1);
            vertexSets.push_back(std::move(maxVertexSet));
        } else {
            vertexSets.push_back(vertexSets.front());
        }
    }
}

template<typename ValueType>
bool QuickHull<ValueType>::affineFilter(std::vector<uint_fast64_t> const& subset, uint_fast64_t const& item, std::vector<EigenVector> const& points) {
    EigenMatrix vectorMatrix(points[item].rows() + 1, subset.size() + 1);
    for (uint_fast64_t i = 0; i < subset.size(); ++i) {
        vectorMatrix.col(i) << points[subset[i]], storm::utility::one<ValueType>();
    }
    vectorMatrix.col(subset.size()) << points[item], storm::utility::one<ValueType>();
    return (vectorMatrix.fullPivLu().rank() > (Eigen::Index)subset.size());
}

template<typename ValueType>
void QuickHull<ValueType>::handleAffineDependentPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets) {
    bool pointsAffineDependent = true;
    std::vector<std::pair<EigenVector, ValueType>> additionalConstraints;
    while (pointsAffineDependent) {
        // get one hyperplane that holds all points
        const uint_fast64_t dimension = points.front().rows();
        EigenVector refPoint = points.front();
        EigenVector normal;
        if (points.size() == 1) {
            normal.resize(dimension);
            normal(0) = storm::utility::one<ValueType>();
        } else {
            EigenMatrix constraints(points.size() - 1, dimension);
            for (unsigned row = 1; row < points.size(); ++row) {
                constraints.row(row - 1) = points[row] - refPoint;
            }
            normal = constraints.fullPivLu().kernel().col(0);
        }

        // Eigen returns the column vector 0...0 if the kernel is empty (i.e., there is no such hyperplane)
        if (normal.isZero()) {
            pointsAffineDependent = false;
        } else {
            points.push_back(refPoint + normal);
            ValueType offset = normal.dot(refPoint);
            additionalConstraints.emplace_back(std::move(normal), std::move(offset));
        }
    }
    // Now the points are affine independent so we can relaunch the procedure
    generateHalfspacesFromPoints(points, generateRelevantVerticesAndVertexSets);

    // Add the constraints obtained above
    uint_fast64_t numOfAdditionalConstraints = additionalConstraints.size();
    uint_fast64_t numOfRegularConstraints = resultMatrix.rows();
    EigenMatrix newA(numOfRegularConstraints + numOfAdditionalConstraints, resultMatrix.cols());
    EigenVector newb(static_cast<unsigned long int>(numOfRegularConstraints + numOfAdditionalConstraints));
    uint_fast64_t row = 0;
    for (; row < numOfRegularConstraints; ++row) {
        newA.row(row) = resultMatrix.row(row);
        newb(row) = resultVector(row);
    }
    for (; row < numOfRegularConstraints + numOfAdditionalConstraints; ++row) {
        newA.row(row) = std::move(additionalConstraints[row - numOfRegularConstraints].first);
        newb(row) = additionalConstraints[row - numOfRegularConstraints].second;
    }
    resultMatrix = std::move(newA);
    resultVector = std::move(newb);

    // clear the additionally added points. Note that the order of the points might have changed
    storm::storage::BitVector keptPoints(points.size(), true);
    for (uint_fast64_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
        for (Eigen::Index row = 0; row < resultMatrix.rows(); ++row) {
            if ((resultMatrix.row(row) * points[pointIndex])(0) > resultVector(row)) {
                keptPoints.set(pointIndex, false);
                break;
            }
        }
    }
    storm::utility::vector::filterVectorInPlace(points, keptPoints);

    if (generateRelevantVerticesAndVertexSets) {
        storm::storage::BitVector keptVertices(relevantVertices.size(), true);
        for (uint_fast64_t vertexIndex = 0; vertexIndex < relevantVertices.size(); ++vertexIndex) {
            for (Eigen::Index row = 0; row < resultMatrix.rows(); ++row) {
                if ((resultMatrix.row(row) * relevantVertices[vertexIndex])(0) > resultVector(row)) {
                    keptVertices.set(vertexIndex, false);
                    break;
                }
            }
        }
        storm::utility::vector::filterVectorInPlace(relevantVertices, keptVertices);

        STORM_LOG_WARN("Can not retrieve vertex sets for degenerated polytope (not implemented)");
        vertexSets.clear();
    }
}

template<typename ValueType>
bool QuickHull<ValueType>::findInitialVertices(std::vector<EigenVector>& points, std::vector<uint_fast64_t>& verticesOfInitialPolytope) const {
    const uint_fast64_t dimension = points.front().rows();
    if (points.size() < dimension + 1) {
        // not enough points to obtain a (non-degenerated) polytope
        return false;
    }

    // We first find some good candidates to get a (hopefully) large initial mesh.
    storm::storage::BitVector notGoodCandidates(points.size(), true);
    for (uint_fast64_t dim = 0; dim < dimension; ++dim) {
        if (!notGoodCandidates.empty()) {
            uint_fast64_t minIndex = *notGoodCandidates.begin();
            uint_fast64_t maxIndex = *notGoodCandidates.begin();
            for (uint_fast64_t pointIndex : notGoodCandidates) {
                if (points[minIndex](dim) > points[pointIndex](dim)) {
                    minIndex = pointIndex;
                }
                if (points[maxIndex](dim) < points[pointIndex](dim)) {
                    maxIndex = pointIndex;
                }
            }
            notGoodCandidates.set(minIndex, false);
            notGoodCandidates.set(maxIndex, false);
        }
    }
    storm::storage::BitVector goodCandidates = ~notGoodCandidates;

    // Found candidates. Now swap them to the front.
    const uint_fast64_t numOfGoodCandidates = goodCandidates.getNumberOfSetBits();
    for (auto goodCandidate : goodCandidates) {
        if (goodCandidate >= numOfGoodCandidates) {
            uint_fast64_t notGoodCandidate = *notGoodCandidates.begin();
            assert(notGoodCandidate < numOfGoodCandidates);
            std::swap(points[notGoodCandidate], points[goodCandidate]);
            notGoodCandidates.set(notGoodCandidate, false);
        }
    }

    storm::storage::geometry::SubsetEnumerator<std::vector<EigenVector>> subsetEnum(points.size(), dimension + 1, points, affineFilter);
    if (subsetEnum.setToFirstSubset()) {
        verticesOfInitialPolytope = subsetEnum.getCurrentSubset();
        return true;
    } else {
        return false;
    }
}

template<typename ValueType>
std::vector<typename QuickHull<ValueType>::Facet> QuickHull<ValueType>::computeInitialFacets(std::vector<EigenVector> const& points,
                                                                                             std::vector<uint_fast64_t> const& verticesOfInitialPolytope,
                                                                                             EigenVector const& insidePoint) const {
    const uint_fast64_t dimension = points.front().rows();
    assert(verticesOfInitialPolytope.size() == dimension + 1);
    std::vector<Facet> result;
    result.reserve(dimension + 1);
    storm::storage::geometry::SubsetEnumerator<> subsetEnum(verticesOfInitialPolytope.size(), dimension);
    STORM_LOG_THROW(subsetEnum.setToFirstSubset(), storm::exceptions::UnexpectedException, "Could not find an initial subset.");
    do {
        Facet newFacet;
        // set the points that lie on the new facet
        std::vector<uint_fast64_t> const& subset(subsetEnum.getCurrentSubset());
        newFacet.points.reserve(subset.size());
        for (uint_fast64_t i : subset) {
            newFacet.points.push_back(verticesOfInitialPolytope[i]);
        }
        // neighbors: these are always the remaining facets
        newFacet.neighbors.reserve(dimension);
        for (uint_fast64_t i = 0; i < dimension + 1; ++i) {
            if (i != result.size()) {  // initFacets.size() will be the index of this new facet!
                newFacet.neighbors.push_back(i);
            }
        }
        // normal and offset:
        computeNormalAndOffsetOfFacet(points, insidePoint, newFacet);

        result.push_back(std::move(newFacet));
    } while (subsetEnum.incrementSubset());
    assert(result.size() == dimension + 1);
    return result;
}

template<typename ValueType>
void QuickHull<ValueType>::computeNormalAndOffsetOfFacet(std::vector<EigenVector> const& points, EigenVector const& insidePoint, Facet& facet) const {
    const uint_fast64_t dimension = points.front().rows();
    assert(facet.points.size() == dimension);
    EigenVector const& refPoint = points[facet.points.back()];
    EigenMatrix constraints(dimension - 1, dimension);
    for (unsigned row = 0; row < dimension - 1; ++row) {
        constraints.row(row) = (points[facet.points[row]] - refPoint);
    }
    facet.normal = constraints.fullPivLu().kernel().col(0);
    facet.offset = facet.normal.dot(refPoint);

    // invert the plane if the insidePoint is not contained in it
    if (facet.normal.dot(insidePoint) > facet.offset) {
        facet.normal = -facet.normal;
        facet.offset = -facet.offset;
    }
}

template<typename ValueType>
void QuickHull<ValueType>::extendMesh(std::vector<EigenVector>& points, std::vector<Facet>& facets, storm::storage::BitVector& currentFacets,
                                      EigenVector& insidePoint) const {
    storm::storage::BitVector currentOutsidePoints(points.size(), true);
    // Compute initial outside Sets
    for (uint_fast64_t facetIndex : currentFacets) {
        computeOutsideSetOfFacet(facets[facetIndex], currentOutsidePoints, points);
    }

    for (uint_fast64_t facetCount = currentFacets.getNextSetIndex(0); facetCount != currentFacets.size();
         facetCount = currentFacets.getNextSetIndex(facetCount + 1)) {
        // set all points to false to get rid of points that lie within the polytope after each iteration
        currentOutsidePoints.clear();
        // Find a facet with a non-empty outside set
        if (!facets[facetCount].outsideSet.empty()) {
            uint_fast64_t numberOfNewFacets = 0;
            // Now we compute the enlarged mesh
            uint_fast64_t farAwayPointIndex = facets[facetCount].maxOutsidePointIndex;
            // get Visible set from maxOutsidePoint of the current facet
            std::set<uint_fast64_t> visibleSet = getVisibleSet(facets, facetCount, points[farAwayPointIndex]);
            std::set<uint_fast64_t> invisibleSet = getInvisibleNeighbors(facets, visibleSet);
            for (auto invisFacetIt = invisibleSet.begin(); invisFacetIt != invisibleSet.end(); ++invisFacetIt) {
                for (auto visFacetIt = visibleSet.begin(); visFacetIt != visibleSet.end(); ++visFacetIt) {
                    if (std::find(facets[*invisFacetIt].neighbors.begin(), facets[*invisFacetIt].neighbors.end(), *visFacetIt) !=
                        facets[*invisFacetIt].neighbors.end()) {
                        Facet newFacet;
                        // Set points of Facet
                        newFacet.points = getCommonPoints(facets[*invisFacetIt], facets[*visFacetIt]);
                        newFacet.points.push_back(farAwayPointIndex);
                        // replace old facet index by new facet index in the current neighbor
                        replaceFacetNeighbor(facets, *visFacetIt, facets.size(), *invisFacetIt);
                        newFacet.neighbors.push_back(*invisFacetIt);
                        // Compute the normal and the offset
                        computeNormalAndOffsetOfFacet(points, insidePoint, newFacet);

                        //   add new facet
                        facets.push_back(newFacet);
                        currentFacets.resize(currentFacets.size() + 1, true);
                        // increase Number Of new Facets
                        ++numberOfNewFacets;
                    }
                }
            }

            for (auto visibleFacet : visibleSet) {
                for (uint_fast64_t outsidePoint : facets[visibleFacet].outsideSet) {
                    currentOutsidePoints.set(outsidePoint, true);
                }
                currentFacets.set(visibleFacet, false);
            }
            // compute new outside sets
            for (uint_fast64_t facetIndex : currentFacets) {
                computeOutsideSetOfFacet(facets[facetIndex], currentOutsidePoints, points);
            }

            // find neighbors in new facets
            setNeighborhoodOfNewFacets(facets, facets.size() - numberOfNewFacets, points.front().rows());
        }
    }
}

template<typename ValueType>
void QuickHull<ValueType>::getPolytopeFromMesh(std::vector<EigenVector> const& points, std::vector<Facet> const& facets,
                                               storm::storage::BitVector const& currentFacets, bool generateRelevantVerticesAndVertexSets) {
    storm::storage::geometry::HyperplaneCollector<ValueType> hyperplaneCollector;
    for (auto facet : currentFacets) {
        hyperplaneCollector.insert(std::move(facets[facet].normal), std::move(facets[facet].offset),
                                   generateRelevantVerticesAndVertexSets ? &facets[facet].points : nullptr);
    }

    if (generateRelevantVerticesAndVertexSets) {
        // Get the mapping from a hyperplane to the set of vertices that lie on that plane, erase the duplicates, and count for each vertex the number of
        // hyperplanes on which that vertex lies
        vertexSets = hyperplaneCollector.getIndexLists();
        std::vector<uint_fast64_t> hyperplanesOnVertexCounter(points.size(), 0);
        for (auto& vertexVector : vertexSets) {
            std::set<uint_fast64_t> vertexSet;
            for (auto const& i : vertexVector) {
                if (vertexSet.insert(i).second) {
                    ++hyperplanesOnVertexCounter[i];
                }
            }
            vertexVector.assign(vertexSet.begin(), vertexSet.end());
        }
        // Now, we can erase all vertices which do not lie on at least dimension hyperplanes.
        // Note that the indices of the HyperplaneToVerticesMapping needs to be refreshed according to the new set of vertices
        // Therefore, we additionally store the old indices for every vertex to be able to translate from old to new indices
        std::unordered_map<EigenVector, std::vector<uint_fast64_t>> relevantVerticesMap;
        relevantVerticesMap.reserve(points.size());
        for (uint_fast64_t vertexIndex = 0; vertexIndex < hyperplanesOnVertexCounter.size(); ++vertexIndex) {
            if ((Eigen::Index)hyperplanesOnVertexCounter[vertexIndex] >= points.front().rows()) {
                auto mapEntry = relevantVerticesMap
                                    .insert(typename std::unordered_map<EigenVector, std::vector<uint_fast64_t>>::value_type(points[vertexIndex],
                                                                                                                             std::vector<uint_fast64_t>()))
                                    .first;
                mapEntry->second.push_back(vertexIndex);
            }
        }
        // Fill in the relevant vertices and create a translation map from old to new indices
        std::vector<uint_fast64_t> oldToNewIndexMapping(points.size(), points.size());  // Initialize with some illegal value
        relevantVertices.clear();
        relevantVertices.reserve(relevantVerticesMap.size());
        for (auto const& mapEntry : relevantVerticesMap) {
            for (auto const& oldIndex : mapEntry.second) {
                oldToNewIndexMapping[oldIndex] = relevantVertices.size();
            }
            relevantVertices.push_back(mapEntry.first);
        }
        // Actually translate and erase duplicates
        for (auto& vertexVector : vertexSets) {
            std::set<uint_fast64_t> vertexSet;
            for (auto const& oldIndex : vertexVector) {
                if ((Eigen::Index)hyperplanesOnVertexCounter[oldIndex] >= points.front().rows()) {
                    vertexSet.insert(oldToNewIndexMapping[oldIndex]);
                }
            }
            vertexVector.assign(vertexSet.begin(), vertexSet.end());
        }
    } else {
        relevantVertices.clear();
        vertexSets.clear();
    }
    auto matrixVector = hyperplaneCollector.getCollectedHyperplanesAsMatrixVector();
    resultMatrix = std::move(matrixVector.first);
    resultVector = std::move(matrixVector.second);
}

template<typename ValueType>
std::set<uint_fast64_t> QuickHull<ValueType>::getVisibleSet(std::vector<Facet> const& facets, uint_fast64_t const& startIndex, EigenVector const& point) const {
    std::set<uint_fast64_t> facetsToCheck;
    std::set<uint_fast64_t> facetsChecked;
    std::set<uint_fast64_t> visibleSet;
    facetsChecked.insert(startIndex);
    visibleSet.insert(startIndex);
    for (uint_fast64_t i = 0; i < facets[startIndex].neighbors.size(); ++i) {
        facetsToCheck.insert(facets[startIndex].neighbors[i]);
    }
    while (!facetsToCheck.empty()) {
        auto elementIt = facetsToCheck.begin();
        if (point.dot(facets[*elementIt].normal) > facets[*elementIt].offset) {
            visibleSet.insert(*elementIt);
            for (uint_fast64_t i = 0; i < facets[*elementIt].neighbors.size(); ++i) {
                if (facetsChecked.find(facets[*elementIt].neighbors[i]) == facetsChecked.end()) {
                    facetsToCheck.insert(facets[*elementIt].neighbors[i]);
                }
            }
        }
        facetsChecked.insert(*elementIt);
        facetsToCheck.erase(elementIt);
    }
    return visibleSet;
}

template<typename ValueType>
void QuickHull<ValueType>::setNeighborhoodOfNewFacets(std::vector<Facet>& facets, uint_fast64_t firstNewFacet, uint_fast64_t dimension) const {
    for (uint_fast64_t currentFacet = firstNewFacet; currentFacet < facets.size(); ++currentFacet) {
        for (uint_fast64_t otherFacet = currentFacet + 1; otherFacet < facets.size(); ++otherFacet) {
            if (getCommonPoints(facets[currentFacet], facets[otherFacet]).size() >= dimension - 1) {
                facets[currentFacet].neighbors.push_back(otherFacet);
                facets[otherFacet].neighbors.push_back(currentFacet);
            }
        }
    }
}

template<typename ValueType>
void QuickHull<ValueType>::replaceFacetNeighbor(std::vector<Facet>& facets, uint_fast64_t oldFacetIndex, uint_fast64_t newFacetIndex,
                                                uint_fast64_t neighborIndex) const {
    auto facetInNeighborListIt = std::find(facets[neighborIndex].neighbors.begin(), facets[neighborIndex].neighbors.end(), oldFacetIndex);
    if (facetInNeighborListIt != facets[neighborIndex].neighbors.end()) {
        *facetInNeighborListIt = newFacetIndex;
    }
}

template<typename ValueType>
void QuickHull<ValueType>::computeOutsideSetOfFacet(Facet& facet, storm::storage::BitVector& currentOutsidePoints,
                                                    std::vector<EigenVector> const& points) const {
    ValueType maxMultiplicationResult = facet.offset;
    for (uint_fast64_t pointIndex : currentOutsidePoints) {
        ValueType multiplicationResult = points[pointIndex].dot(facet.normal);
        if (multiplicationResult > facet.offset) {
            currentOutsidePoints.set(pointIndex, false);  // we already know that the point lies outside so it can be ignored for future facets
            facet.outsideSet.push_back(pointIndex);
            if (multiplicationResult > maxMultiplicationResult) {
                maxMultiplicationResult = multiplicationResult;
                facet.maxOutsidePointIndex = pointIndex;
            }
        }
    }
}

template<typename ValueType>
std::vector<uint_fast64_t> QuickHull<ValueType>::getCommonPoints(Facet const& lhs, Facet const& rhs) const {
    std::vector<uint_fast64_t> commonPoints;
    for (uint_fast64_t lhsPoint : lhs.points) {
        for (uint_fast64_t rhsPoint : rhs.points) {
            if (lhsPoint == rhsPoint) {
                commonPoints.push_back(lhsPoint);
            }
        }
    }
    return commonPoints;
}

template<typename ValueType>
std::set<uint_fast64_t> QuickHull<ValueType>::getInvisibleNeighbors(std::vector<Facet>& facets, std::set<uint_fast64_t> const& visibleSet) const {
    std::set<uint_fast64_t> invisibleNeighbors;
    for (auto currentFacetIt = visibleSet.begin(); currentFacetIt != visibleSet.end(); ++currentFacetIt) {
        for (uint_fast64_t currentNeighbor = 0; currentNeighbor < facets[*currentFacetIt].neighbors.size(); ++currentNeighbor) {
            if (visibleSet.find(facets[*currentFacetIt].neighbors[currentNeighbor]) == visibleSet.end()) {
                invisibleNeighbors.insert(facets[*currentFacetIt].neighbors[currentNeighbor]);
            }
        }
    }
    return invisibleNeighbors;
}

template class QuickHull<double>;
template class QuickHull<storm::RationalNumber>;

}  // namespace geometry
}  // namespace storage
}  // namespace storm
