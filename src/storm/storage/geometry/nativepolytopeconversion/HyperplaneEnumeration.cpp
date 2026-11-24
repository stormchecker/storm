#include "storm/storage/geometry/nativepolytopeconversion/HyperplaneEnumeration.h"

#include "storm/storage/geometry/nativepolytopeconversion/HyperplaneCollector.h"
#include "storm/storage/geometry/nativepolytopeconversion/SubsetEnumerator.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
void HyperplaneEnumeration<ValueType>::generateVerticesFromConstraints(EigenMatrix const& constraintMatrix, EigenVector const& constraintVector,
                                                                       bool generateRelevantHyperplanesAndVertexSets) {
    STORM_LOG_DEBUG("Invoked Hyperplane enumeration with " << constraintMatrix.rows() << " constraints.");
    Eigen::Index dimension = constraintMatrix.cols();
    if (dimension == 0) {
        // No halfspaces means no vertices
        resultVertices.clear();
        relevantMatrix = constraintMatrix;
        relevantVector = constraintVector;
        vertexSets.clear();
        return;
    }
    std::unordered_map<EigenVector, std::set<uint_fast64_t>> vertexCollector;
    storm::storage::geometry::SubsetEnumerator<EigenMatrix> subsetEnum(constraintMatrix.rows(), dimension, constraintMatrix,
                                                                       HyperplaneEnumeration<ValueType>::linearDependenciesFilter);
    if (subsetEnum.setToFirstSubset()) {
        do {
            std::vector<uint_fast64_t> const& subset = subsetEnum.getCurrentSubset();

            EigenMatrix subMatrix(dimension, dimension);
            EigenVector subVector(dimension);
            for (Eigen::Index i = 0; i < dimension; ++i) {
                subMatrix.row(i) = constraintMatrix.row(subset[i]);
                subVector(i) = constraintVector(subset[i]);
            }

            EigenVector point = subMatrix.fullPivLu().solve(subVector);
            bool pointContained = true;
            for (Eigen::Index row = 0; row < constraintMatrix.rows(); ++row) {
                if ((constraintMatrix.row(row) * point)(0) > constraintVector(row)) {
                    pointContained = false;
                    break;
                }
            }
            if (pointContained) {
                // Note that the map avoids duplicates.
                auto hyperplaneIndices =
                    vertexCollector
                        .insert(typename std::unordered_map<EigenVector, std::set<uint_fast64_t>>::value_type(std::move(point), std::set<uint_fast64_t>()))
                        .first;
                if (generateRelevantHyperplanesAndVertexSets) {
                    hyperplaneIndices->second.insert(subset.begin(), subset.end());
                }
            }
        } while (subsetEnum.incrementSubset());
    }

    if (generateRelevantHyperplanesAndVertexSets) {
        // For each hyperplane, get the number of (unique) vertices that lie on it.
        std::vector<Eigen::Index> verticesOnHyperplaneCounter(constraintMatrix.rows(), 0);
        for (auto const& mapEntry : vertexCollector) {
            for (auto const& hyperplaneIndex : mapEntry.second) {
                ++verticesOnHyperplaneCounter[hyperplaneIndex];
            }
        }

        // Only keep the hyperplanes on which at least dimension() vertices lie.
        // Note that this will change the indices of the Hyperplanes.
        // Therefore, we additionally store the old indices for every hyperplane to be able to translate from old to new indices
        storm::storage::geometry::HyperplaneCollector<ValueType> hyperplaneCollector;
        for (uint_fast64_t hyperplaneIndex = 0; hyperplaneIndex < verticesOnHyperplaneCounter.size(); ++hyperplaneIndex) {
            if (verticesOnHyperplaneCounter[hyperplaneIndex] >= dimension) {
                std::vector<uint_fast64_t> oldIndex;
                oldIndex.push_back(hyperplaneIndex);
                hyperplaneCollector.insert(constraintMatrix.row(hyperplaneIndex), constraintVector(hyperplaneIndex), &oldIndex);
            }
        }
        auto matrixVector = hyperplaneCollector.getCollectedHyperplanesAsMatrixVector();
        relevantMatrix = std::move(matrixVector.first);
        relevantVector = std::move(matrixVector.second);

        // Get the mapping from old to new indices
        std::vector<uint_fast64_t> oldToNewIndexMapping(constraintMatrix.rows(), constraintMatrix.rows());  // Initialize with some illegal value
        std::vector<std::vector<uint_fast64_t>> newToOldIndexMapping(hyperplaneCollector.getIndexLists());
        for (uint_fast64_t newIndex = 0; newIndex < newToOldIndexMapping.size(); ++newIndex) {
            for (auto const& oldIndex : newToOldIndexMapping[newIndex]) {
                oldToNewIndexMapping[oldIndex] = newIndex;
            }
        }

        // Insert the resulting vertices and get the set of vertices that lie on each hyperplane
        std::vector<std::set<uint_fast64_t>> vertexSets(relevantMatrix.rows());
        resultVertices.clear();
        resultVertices.reserve(vertexCollector.size());
        for (auto const& mapEntry : vertexCollector) {
            for (auto const& oldHyperplaneIndex : mapEntry.second) {
                // ignore the hyperplanes which are redundant, i.e. for which there is no new index
                if ((Eigen::Index)oldToNewIndexMapping[oldHyperplaneIndex] < relevantVector.rows()) {
                    vertexSets[oldToNewIndexMapping[oldHyperplaneIndex]].insert(resultVertices.size());
                }
            }
            resultVertices.push_back(mapEntry.first);
        }
        this->vertexSets.clear();
        this->vertexSets.reserve(vertexSets.size());
        for (auto const& vertexSet : vertexSets) {
            this->vertexSets.emplace_back(vertexSet.begin(), vertexSet.end());
        }

    } else {
        resultVertices.clear();
        resultVertices.reserve(vertexCollector.size());
        for (auto const& mapEntry : vertexCollector) {
            resultVertices.push_back(mapEntry.first);
        }
        this->vertexSets.clear();
        relevantMatrix = EigenMatrix();
        relevantVector = EigenVector();
    }
}

template<typename ValueType>
bool HyperplaneEnumeration<ValueType>::linearDependenciesFilter(std::vector<uint_fast64_t> const& subset, uint_fast64_t const& item, EigenMatrix const& A) {
    EigenMatrix subMatrix(subset.size() + 1, A.cols());
    for (uint_fast64_t i = 0; i < subset.size(); ++i) {
        subMatrix.row((Eigen::Index)i) = A.row(Eigen::Index(subset[i]));
    }
    subMatrix.row(subset.size()) = A.row(item);
    Eigen::FullPivLU<EigenMatrix> lUMatrix(subMatrix);
    if (lUMatrix.rank() < subMatrix.rows()) {
        // Linear dependent!
        return false;
    } else {
        return true;
    }
}

template<typename ValueType>
std::vector<typename HyperplaneEnumeration<ValueType>::EigenVector>& HyperplaneEnumeration<ValueType>::getResultVertices() {
    return resultVertices;
}

template<typename ValueType>
typename HyperplaneEnumeration<ValueType>::EigenMatrix& HyperplaneEnumeration<ValueType>::getRelevantMatrix() {
    return relevantMatrix;
}

template<typename ValueType>
typename HyperplaneEnumeration<ValueType>::EigenVector& HyperplaneEnumeration<ValueType>::getRelevantVector() {
    return relevantVector;
}

template<typename ValueType>
std::vector<std::vector<uint_fast64_t>>& HyperplaneEnumeration<ValueType>::getVertexSets() {
    return this->vertexSets;
}

template class HyperplaneEnumeration<double>;
template class HyperplaneEnumeration<storm::RationalNumber>;

}  // namespace geometry
}  // namespace storage
}  // namespace storm
