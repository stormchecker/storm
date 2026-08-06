#include <algorithm>
#include <sstream>

#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectivePostprocessing.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsParetoExplorer.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/geometry/coordinates.h"

#include "storm/io/export.h"
#include "storm/utility/solver.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::Point(std::vector<GeometryValueType> const& coordinates)
    : coordinates(coordinates), onFacet(false), paretoOptimal(false) {
    STORM_LOG_ASSERT(!this->coordinates.empty(), "Points with dimension 0 are not supported");
}

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::Point(std::vector<GeometryValueType>&& coordinates)
    : coordinates(std::move(coordinates)), onFacet(false), paretoOptimal(false) {
    STORM_LOG_ASSERT(!this->coordinates.empty(), "Points with dimension 0 are not supported");
}

template<class SparseModelType, typename GeometryValueType>
std::vector<GeometryValueType>& DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::get() {
    return coordinates;
}

template<class SparseModelType, typename GeometryValueType>
std::vector<GeometryValueType> const& DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::get() const {
    return coordinates;
}

template<class SparseModelType, typename GeometryValueType>
uint64_t DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::dimension() const {
    STORM_LOG_ASSERT(!coordinates.empty(), "Points with dimension 0 are not supported");
    return coordinates.size();
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::DominanceResult
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::getDominance(Point const& other) const {
    STORM_LOG_ASSERT(this->dimension() == other.dimension(), "Non-Equal dimensions of points: [" << this->toString() << "] vs. [" << other.toString() << "]");
    auto thisIt = this->get().begin();
    auto otherIt = other.get().begin();
    auto thisItE = this->get().end();

    // Find the first entry where the points differ
    while (*thisIt == *otherIt) {
        ++thisIt;
        ++otherIt;
        if (thisIt == thisItE) {
            return DominanceResult::Equal;
        }
    }

    if (*thisIt > *otherIt) {
        // *this might dominate other
        for (++thisIt, ++otherIt; thisIt != thisItE; ++thisIt, ++otherIt) {
            if (*thisIt < *otherIt) {
                return DominanceResult::Incomparable;
            }
        }
        return DominanceResult::Dominates;
    } else {
        assert(*thisIt < *otherIt);
        // *this might be dominated by other
        for (++thisIt, ++otherIt; thisIt != thisItE; ++thisIt, ++otherIt) {
            if (*thisIt > *otherIt) {
                return DominanceResult::Incomparable;
            }
        }
        return DominanceResult::Dominated;
    }
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::setOnFacet(bool value) {
    onFacet = value;
}

template<class SparseModelType, typename GeometryValueType>
bool DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::liesOnFacet() const {
    return onFacet;
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::setParetoOptimal(bool value) {
    paretoOptimal = value;
}

template<class SparseModelType, typename GeometryValueType>
bool DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::isParetoOptimal() const {
    return paretoOptimal;
}

template<class SparseModelType, typename GeometryValueType>
std::string DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point::toString(bool convertToDouble) const {
    std::stringstream out;
    bool first = true;
    for (auto const& pi : this->get()) {
        if (first) {
            first = false;
        } else {
            out << ", ";
        }
        if (convertToDouble) {
            out << storm::utility::convertNumber<double>(pi);
        } else {
            out << pi;
        }
    }
    return out.str();
}

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::Pointset() : currId(1) {
    // Intentionally left empty
}

template<class SparseModelType, typename GeometryValueType>
boost::optional<typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::PointId>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::addPoint(Environment const& env, Point&& point) {
    // Find dominated and dominating points
    auto pointsIt = points.begin();
    while (pointsIt != points.end()) {
        switch (point.getDominance(pointsIt->second)) {
            case Point::DominanceResult::Incomparable:
                // Nothing to be done for this point
                ++pointsIt;
                break;
            case Point::DominanceResult::Dominates:
                if (pointsIt->second.liesOnFacet()) {
                    // Do not erase points that lie on a facet. But flag it as non-optimal.
                    pointsIt->second.setParetoOptimal(false);
                    ++pointsIt;
                } else {
                    pointsIt = points.erase(pointsIt);
                }
                break;
            case Point::DominanceResult::Dominated:
                // The new point is dominated by another point.
                return boost::none;
            case Point::DominanceResult::Equal:
                if (point.liesOnFacet()) {
                    pointsIt->second.setOnFacet();
                }
                return pointsIt->first;
        }
    }
    // This point is not dominated by some other (known) point.
    point.setParetoOptimal(true);

    if (env.modelchecker().multi().isPrintResultsSet()) {
        std::cout << "## achievable point: [" << point.toString(true) << "]\n";
    }
    points.emplace_hint(points.end(), currId, std::move(point));
    return currId++;
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Point const&
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::getPoint(PointId const& id) const {
    return points.at(id);
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::iterator_type
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::begin() const {
    return points.begin();
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::iterator_type
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::end() const {
    return points.end();
}

template<class SparseModelType, typename GeometryValueType>
uint64_t DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::size() const {
    return points.size();
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Polytope
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::downwardClosure() const {
    std::vector<std::vector<GeometryValueType>> pointsAsVector;
    pointsAsVector.reserve(size());
    for (auto const& p : points) {
        pointsAsVector.push_back(p.second.get());
    }
    return storm::storage::geometry::Polytope<GeometryValueType>::createDownwardClosure(std::move(pointsAsVector));
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::collectPointsInPolytope(std::set<PointId>& collectedPoints,
                                                                                                              Polytope const& polytope) {
    for (auto const& p : points) {
        if (polytope->contains(p.second.get())) {
            collectedPoints.insert(p.first);
        }
    }
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Pointset::printToStream(std::ostream& out, bool includeIDs, bool convertToDouble) {
    for (auto const& p : this->points) {
        if (includeIDs) {
            out << p.first << ": [" << p.second.toString(convertToDouble) << "]\n";
        } else {
            out << p.second.toString(convertToDouble) << '\n';
        }
    }
}

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::Facet(storm::storage::geometry::Halfspace<GeometryValueType> const& halfspace)
    : halfspace(halfspace) {
    // Intentionally left empty
}

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::Facet(storm::storage::geometry::Halfspace<GeometryValueType>&& halfspace)
    : halfspace(std::move(halfspace)) {
    // Intentionally left empty
}

template<class SparseModelType, typename GeometryValueType>
storm::storage::geometry::Halfspace<GeometryValueType> const& DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::getHalfspace()
    const {
    return halfspace;
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::addPoint(PointId const& pointId, Point const& point) {
    GeometryValueType product = storm::utility::vector::dotProduct(getHalfspace().normalVector(), point.get());
    if (product != getHalfspace().offset()) {
        if (product < getHalfspace().offset()) {
            STORM_LOG_DEBUG("The point on the facet actually has distance "
                            << storm::utility::convertNumber<double>(getHalfspace().euclideanDistance(point.get())));
        } else {
            STORM_LOG_DEBUG("Halfspace of facet is shifted by " << storm::utility::convertNumber<double>(getHalfspace().euclideanDistance(point.get()))
                                                                << " to capture all points that are supposed to lie on the facet.");
            halfspace.offset() = product;
        }
    }
    pointsOnFacet.push_back(pointId);
}

template<class SparseModelType, typename GeometryValueType>
std::vector<typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::PointId> const&
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::getPoints() const {
    return pointsOnFacet;
}

template<class SparseModelType, typename GeometryValueType>
uint64_t DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::getNumberOfPoints() const {
    return pointsOnFacet.size();
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Polytope
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Facet::getInducedPolytope(Pointset const& pointset,
                                                                                                 std::vector<GeometryValueType> const& referenceCoordinates) {
    std::vector<std::vector<GeometryValueType>> vertices = {referenceCoordinates};
    for (auto const& pId : pointsOnFacet) {
        vertices.push_back(pointset.getPoint(pId).get());
    }
    // This facet might lie at the 'border', which means that the downward closure has to be taken in some directions
    storm::storage::BitVector dimensionsForDownwardClosure = storm::utility::vector::filterZero(this->halfspace.normalVector());
    STORM_LOG_ASSERT(dimensionsForDownwardClosure.getNumberOfSetBits() + vertices.size() >= halfspace.normalVector().size() + 1,
                     "The number of points on the facet is insufficient");
    if (dimensionsForDownwardClosure.empty()) {
        return storm::storage::geometry::Polytope<GeometryValueType>::create(vertices);
    } else {
        return storm::storage::geometry::Polytope<GeometryValueType>::createSelectiveDownwardClosure(vertices, dimensionsForDownwardClosure);
    }
}

template<class SparseModelType, typename GeometryValueType>
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::DeterministicSchedsParetoExplorer(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>& preprocessorResult)
    : model(preprocessorResult.preprocessedModel), objectives(preprocessorResult.objectives) {
    originalModelInitialState = *preprocessorResult.originalModel.getInitialStates().begin();
    objectiveHelper.reserve(objectives.size());
    for (auto const& obj : objectives) {
        objectiveHelper.emplace_back(*model, obj);
        STORM_LOG_ASSERT(!objectiveHelper.back().hasThreshold(), "Unexpected input: got a Pareto query with a thresholded objective.");
    }
    lpChecker = std::make_shared<DeterministicSchedsLpChecker<SparseModelType, GeometryValueType>>(*model, objectiveHelper);
    if (preprocessorResult.containsOnlyTotalRewardFormulas()) {
        wvChecker = storm::modelchecker::multiobjective::createWeightVectorChecker(preprocessorResult);
    } else {
        wvChecker = nullptr;
    }
}

template<class SparseModelType, typename GeometryValueType>
std::unique_ptr<CheckResult> DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::check(Environment const& env) {
    clean();
    for (auto& obj : objectiveHelper) {
        obj.computeLowerUpperBounds(env);
    }
    initializeFacets(env);

    // Compute the relative precision in each dimension.
    if (env.modelchecker().multi().getPrecisionType() == MultiObjectiveModelCheckerEnvironment::PrecisionType::RelativeToDiff) {
        std::vector<GeometryValueType> pmax, pmin;
        for (auto const& point : pointset) {
            auto const& coordinates = point.second.get();
            if (pmax.empty() && pmin.empty()) {
                pmax = coordinates;
                pmin = coordinates;
            } else {
                for (uint64_t i = 0; i < pmax.size(); ++i) {
                    pmax[i] = std::max(pmax[i], coordinates[i]);
                    pmin[i] = std::min(pmin[i], coordinates[i]);
                }
            }
        }
        GeometryValueType epsScalingFactor = storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision());
        epsScalingFactor += epsScalingFactor;
        eps.clear();
        for (uint64_t i = 0; i < pmax.size(); ++i) {
            eps.push_back((pmax[i] - pmin[i]) * epsScalingFactor);
            if (eps.back() < storm::utility::convertNumber<GeometryValueType>(1e-8)) {
                STORM_LOG_WARN("Changing relative precision of objective "
                               << i << " to 1e-8 since the difference between the highest and lowest value is below 1e-8.");
                eps.back() = storm::utility::convertNumber<GeometryValueType>(1e-8);
            }
        }
        STORM_LOG_INFO("Relative precision for deterministic scheduler Pareto explorer is "
                       << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(eps)) << '\n');
    } else {
        STORM_LOG_THROW(env.modelchecker().multi().getPrecisionType() == MultiObjectiveModelCheckerEnvironment::PrecisionType::Absolute,
                        storm::exceptions::IllegalArgumentException, "Unknown multiobjective precision type.");
        auto ei = storm::utility::convertNumber<GeometryValueType>(env.modelchecker().multi().getPrecision());
        ei += ei;
        eps = std::vector<GeometryValueType>(objectives.size(), ei);
    }
    while (!unprocessedFacets.empty()) {
        Facet f = std::move(unprocessedFacets.front());
        unprocessedFacets.pop();
        processFacet(env, f);
    }

    std::vector<std::vector<ModelValueType>> paretoPoints;
    paretoPoints.reserve(pointset.size());
    for (auto const& p : pointset) {
        if (p.second.isParetoOptimal()) {
            paretoPoints.push_back(
                storm::utility::vector::convertNumericVector<ModelValueType>(transformObjectiveValuesToOriginal(objectives, p.second.get())));
        }
    }
    return std::make_unique<storm::modelchecker::ExplicitParetoCurveCheckResult<ModelValueType>>(originalModelInitialState, std::move(paretoPoints), nullptr,
                                                                                                 nullptr);
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::clean() {
    pointset = Pointset();
    unprocessedFacets = std::queue<Facet>();
    overApproximation = storm::storage::geometry::Polytope<GeometryValueType>::createUniversalPolytope();
    unachievableAreas.clear();
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::addHalfspaceToOverApproximation(Environment const& env,
                                                                                                            std::vector<GeometryValueType> const& normalVector,
                                                                                                            GeometryValueType const& offset) {
    if (env.modelchecker().multi().isPrintResultsSet()) {
        std::cout << "## overapproximation halfspace: [";
        bool first = true;
        for (auto const& xi : normalVector) {
            if (first) {
                first = false;
            } else {
                std::cout << ",";
            }
            std::cout << storm::utility::convertNumber<double>(xi);
        }
        std::cout << "];[" << storm::utility::convertNumber<double>(offset) << "]\n";
    }
    storm::storage::geometry::Halfspace<GeometryValueType> overApproxHalfspace(normalVector, offset);
    overApproximation = overApproximation->intersection(overApproxHalfspace);
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::addUnachievableArea(Environment const& env, Polytope const& area) {
    if (env.modelchecker().multi().isPrintResultsSet()) {
        std::vector<std::vector<GeometryValueType>> vertices;
        if (objectives.size() == 2) {
            vertices = area->getVerticesInClockwiseOrder();
        } else {
            vertices = area->getVertices();
        }
        std::cout << "## unachievable polytope: ";
        bool firstVertex = true;
        for (auto const& v : vertices) {
            if (firstVertex) {
                firstVertex = false;
            } else {
                std::cout << ";";
            }
            std::cout << "[";
            bool firstEntry = true;
            for (auto const& vi : v) {
                if (firstEntry) {
                    firstEntry = false;
                } else {
                    std::cout << ",";
                }
                std::cout << storm::utility::convertNumber<double>(vi);
            }
            std::cout << "]";
        }
        std::cout << '\n';
    }
    unachievableAreas.push_back(area);
}

template<class SparseModelType, typename GeometryValueType>
typename DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::Polytope
DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::negateMinObjectives(Polytope const& polytope) const {
    std::vector<GeometryValueType> zeroRow(objectives.size(), storm::utility::zero<GeometryValueType>());
    std::vector<std::vector<GeometryValueType>> transformationMatrix(objectives.size(), zeroRow);
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        if (objectiveHelper[objIndex].minimizing()) {
            transformationMatrix[objIndex][objIndex] = -storm::utility::one<GeometryValueType>();
        } else {
            transformationMatrix[objIndex][objIndex] = storm::utility::one<GeometryValueType>();
        }
    }
    return polytope->affineTransformation(transformationMatrix, zeroRow);
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::negateMinObjectives(std::vector<GeometryValueType>& vector) const {
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        if (objectiveHelper[objIndex].minimizing()) {
            vector[objIndex] *= -storm::utility::one<GeometryValueType>();
        }
    }
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::initializeFacets(Environment const& env) {
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        std::vector<GeometryValueType> weightVector(objectives.size(), storm::utility::zero<GeometryValueType>());
        weightVector[objIndex] = storm::utility::one<GeometryValueType>();
        std::vector<GeometryValueType> pointCoord;
        GeometryValueType offset;
        if (wvChecker) {
            wvChecker->setWeightedPrecision(storm::utility::convertNumber<ModelValueType>(env.solver().minMax().getPrecision()));
            wvChecker->check(env, storm::utility::vector::convertNumericVector<ModelValueType>(weightVector));
            pointCoord = storm::utility::vector::convertNumericVector<GeometryValueType>(wvChecker->getAchievablePoint());
            negateMinObjectives(pointCoord);
            offset = storm::utility::convertNumber<GeometryValueType>(wvChecker->getOptimalWeightedSum());
        } else {
            lpChecker->setCurrentWeightVector(env, weightVector);
            auto optionalPoint = lpChecker->check(env, overApproximation);
            STORM_LOG_THROW(optionalPoint.has_value(), storm::exceptions::UnexpectedException, "Unable to find a point in the current overapproximation.");
            pointCoord = std::move(optionalPoint->first);
            offset = std::move(optionalPoint->second);
        }
        Point p(pointCoord);
        p.setOnFacet();
        // Adapt the overapproximation
        addHalfspaceToOverApproximation(env, weightVector, offset);
        pointset.addPoint(env, std::move(p));
    }

    auto initialHalfspaces = pointset.downwardClosure()->getHalfspaces();
    for (auto& h : initialHalfspaces) {
        Facet f(std::move(h));
        for (auto const& p : pointset) {
            if (f.getHalfspace().isPointOnBoundary(p.second.get())) {
                f.addPoint(p.first, p.second);
            }
        }
        STORM_LOG_ASSERT(std::count(f.getHalfspace().normalVector().begin(), f.getHalfspace().normalVector().end(), storm::utility::zero<GeometryValueType>()) +
                                 f.getNumberOfPoints() >=
                             objectives.size(),
                         "Not enough points on facet.");

        unprocessedFacets.push(std::move(f));
    }
}

template<class SparseModelType, typename GeometryValueType>
std::vector<GeometryValueType> DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::getReferenceCoordinates(Environment const& env) const {
    std::vector<GeometryValueType> result;
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        result.push_back(
            storm::utility::convertNumber<GeometryValueType>(objectiveHelper[objIndex].getLowerValueBoundAtState(*model->getInitialStates().begin())));
    }
    return result;
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::processFacet(Environment const& env, Facet& f) {
    if (!wvChecker) {
        lpChecker->setCurrentWeightVector(env, f.getHalfspace().normalVector());
    }

    if (optimizeAndSplitFacet(env, f)) {
        return;
    }

    storm::storage::geometry::PolytopeTree<GeometryValueType> polytopeTree(f.getInducedPolytope(pointset, getReferenceCoordinates(env)));
    for (auto const& point : pointset) {
        polytopeTree.substractDownwardClosure(point.second.get(), eps);
        if (polytopeTree.isEmpty()) {
            break;
        }
    }
    if (!polytopeTree.isEmpty()) {
        if (wvChecker) {
            lpChecker->setCurrentWeightVector(env, f.getHalfspace().normalVector());
        }
        auto res = lpChecker->check(env, polytopeTree, eps);
        for (auto const& infeasableArea : res.second) {
            addUnachievableArea(env, infeasableArea);
        }
        for (auto& achievablePoint : res.first) {
            pointset.addPoint(env, Point(std::move(achievablePoint)));
        }
    }
}

template<typename GeometryValueType>
bool closePoints(std::vector<GeometryValueType> const& first, std::vector<GeometryValueType> const& second, GeometryValueType const& maxDistance) {
    for (uint64_t i = 0; i < first.size(); ++i) {
        if (storm::utility::abs<GeometryValueType>(first[i] - second[i]) > maxDistance) {
            return false;
        }
    }
    return true;
}

template<class SparseModelType, typename GeometryValueType>
bool DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::optimizeAndSplitFacet(Environment const& env, Facet& f) {
    // Invoke optimization and insert the explored points
    boost::optional<PointId> optPointId;
    std::vector<GeometryValueType> pointCoord;
    GeometryValueType offset;
    if (wvChecker) {
        wvChecker->setWeightedPrecision(storm::utility::convertNumber<ModelValueType>(env.solver().minMax().getPrecision()));
        wvChecker->check(env, storm::utility::vector::convertNumericVector<ModelValueType>(f.getHalfspace().normalVector()));
        pointCoord = storm::utility::vector::convertNumericVector<GeometryValueType>(wvChecker->getAchievablePoint());
        negateMinObjectives(pointCoord);
        offset = storm::utility::convertNumber<GeometryValueType>(wvChecker->getOptimalWeightedSum());
    } else {
        auto currentArea = overApproximation->intersection(f.getHalfspace().invert());
        auto optionalPoint = lpChecker->check(env, overApproximation, eps);
        if (optionalPoint.has_value()) {
            pointCoord = std::move(optionalPoint->first);
        } else {
            // As we did not find any feasable solution in the given area, we take a point that lies on the facet
            pointCoord = pointset.getPoint(f.getPoints().front()).get();
        }
        offset = std::move(optionalPoint->second);
    }

    Point p(pointCoord);
    p.setOnFacet();
    addHalfspaceToOverApproximation(env, f.getHalfspace().normalVector(), offset);
    optPointId = pointset.addPoint(env, std::move(p));

    // Potentially generate new facets
    if (optPointId) {
        auto const& optPoint = pointset.getPoint(*optPointId);
        if (f.getHalfspace().contains(optPoint.get())) {
            // The point is contained in the halfspace which means that no more splitting is possible.
            return false;
        } else {
            // Collect the new point with its neighbors.
            // Also check whether the remamining area is already sufficiently small.
            storm::storage::geometry::PolytopeTree<GeometryValueType> remainingArea(overApproximation->intersection(f.getHalfspace().invert()));
            std::vector<std::vector<GeometryValueType>> vertices;
            vertices.push_back(optPoint.get());
            auto minmaxPrec = storm::utility::convertNumber<GeometryValueType>(env.solver().minMax().getPrecision());
            minmaxPrec += minmaxPrec;
            for (auto const& pId : f.getPoints()) {
                vertices.push_back(pointset.getPoint(pId).get());
                remainingArea.substractDownwardClosure(vertices.back(), eps);
                STORM_LOG_WARN_COND((std::is_same<ModelValueType, storm::RationalNumber>::value) || !closePoints(vertices.front(), vertices.back(), minmaxPrec),
                                    "Found Pareto optimal points that are close to each other. This can be due to numerical issues. Maybe try exact mode?");
            }
            if (remainingArea.isEmpty()) {
                return false;
            }

            // We need to generate new facets
            auto newHalfspaceCandidates = storm::storage::geometry::Polytope<GeometryValueType>::createSelectiveDownwardClosure(
                                              vertices, storm::utility::vector::filterZero(f.getHalfspace().normalVector()))
                                              ->getHalfspaces();
            for (auto& h : newHalfspaceCandidates) {
                if (!storm::utility::vector::hasNegativeEntry(h.normalVector())) {
                    STORM_LOG_ASSERT(h.isPointOnBoundary(optPoint.get()), "Unexpected facet found while splitting.");
                    Facet fNew(std::move(h));
                    fNew.addPoint(optPointId.get(), optPoint);
                    auto vertexIt = vertices.begin();
                    ++vertexIt;
                    for (auto const& pId : f.getPoints()) {
                        assert(pointset.getPoint(pId).get() == *vertexIt);
                        if (fNew.getHalfspace().isPointOnBoundary(*vertexIt)) {
                            fNew.addPoint(pId, pointset.getPoint(pId));
                        }
                        ++vertexIt;
                    }
                    assert(vertexIt == vertices.end());
                    unprocessedFacets.push(std::move(fNew));
                }
            }
            return true;
        }
    } else {
        // If the 'optimal point' was dominated by an existing point, we can not split the facet any further.
        return false;
    }
}

template<class SparseModelType, typename GeometryValueType>
void DeterministicSchedsParetoExplorer<SparseModelType, GeometryValueType>::exportPlotOfCurrentApproximation(Environment const& env) {
    /*
    STORM_LOG_ERROR_COND(objectives.size()==2, "Exporting plot requested but this is only implemented for the two-dimensional case.");

    auto transformedUnderApprox = transformPolytopeToOriginalModel(underApproximation);
    auto transformedOverApprox = transformPolytopeToOriginalModel(overApproximation);

    // Get pareto points as well as a hyperrectangle that is used to guarantee that the resulting polytopes are bounded.
    storm::storage::geometry::Hyperrectangle<GeometryValueType> boundaries(std::vector<GeometryValueType>(objectives.size(),
storm::utility::zero<GeometryValueType>()), std::vector<GeometryValueType>(objectives.size(), storm::utility::zero<GeometryValueType>()));
    std::vector<std::vector<GeometryValueType>> paretoPoints;
    paretoPoints.reserve(refinementSteps.size());
    for(auto const& step : refinementSteps) {
        paretoPoints.push_back(transformPointToOriginalModel(step.lowerBoundPoint));
        boundaries.enlarge(paretoPoints.back());
    }
    auto underApproxVertices = transformedUnderApprox->getVertices();
    for(auto const& v : underApproxVertices) {
        boundaries.enlarge(v);
    }
    auto overApproxVertices = transformedOverApprox->getVertices();
    for(auto const& v : overApproxVertices) {
        boundaries.enlarge(v);
    }

    //Further enlarge the boundaries a little
    storm::utility::vector::scaleVectorInPlace(boundaries.lowerBounds(), GeometryValueType(15) / GeometryValueType(10));
    storm::utility::vector::scaleVectorInPlace(boundaries.upperBounds(), GeometryValueType(15) / GeometryValueType(10));

    auto boundariesAsPolytope = boundaries.asPolytope();
    std::vector<std::string> columnHeaders = {"x", "y"};

    std::vector<std::vector<double>> pointsForPlotting;
    if (env.modelchecker().multi().getPlotPathUnderApproximation()) {
        underApproxVertices = transformedUnderApprox->intersection(boundariesAsPolytope)->getVerticesInClockwiseOrder();
        pointsForPlotting.reserve(underApproxVertices.size());
        for(auto const& v : underApproxVertices) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathUnderApproximation().get(), pointsForPlotting,
columnHeaders);
    }

    if (env.modelchecker().multi().getPlotPathOverApproximation()) {
        pointsForPlotting.clear();
        overApproxVertices = transformedOverApprox->intersection(boundariesAsPolytope)->getVerticesInClockwiseOrder();
        pointsForPlotting.reserve(overApproxVertices.size());
        for(auto const& v : overApproxVertices) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathOverApproximation().get(), pointsForPlotting,
columnHeaders);
    }

    if (env.modelchecker().multi().getPlotPathParetoPoints()) {
        pointsForPlotting.clear();
        pointsForPlotting.reserve(paretoPoints.size());
        for(auto const& v : paretoPoints) {
            pointsForPlotting.push_back(storm::utility::vector::convertNumericVector<double>(v));
        }
        storm::io::exportDataToCSVFile<double, std::string>(env.modelchecker().multi().getPlotPathParetoPoints().get(), pointsForPlotting, columnHeaders);
    }
};
     */
}

template class DeterministicSchedsParetoExplorer<storm::models::sparse::Mdp<double>, storm::RationalNumber>;
template class DeterministicSchedsParetoExplorer<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber>;
template class DeterministicSchedsParetoExplorer<storm::models::sparse::MarkovAutomaton<double>, storm::RationalNumber>;
template class DeterministicSchedsParetoExplorer<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>, storm::RationalNumber>;
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
