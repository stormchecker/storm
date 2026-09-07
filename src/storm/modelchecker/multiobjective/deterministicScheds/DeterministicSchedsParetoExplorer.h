#pragma once

#include <memory>
#include <queue>

#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsLpChecker.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsObjectiveHelper.h"
#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/geometry/Halfspace.h"
#include "storm/storage/geometry/Polytope.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace multiobjective {

/*!
 * Implements the exploration of the Pareto front.
 * @see http://doi.org/10.18154/RWTH-2023-09669 Chapter 8.5
 */
template<class SparseModelType, typename GeometryValueType>
class DeterministicSchedsParetoExplorer {
   public:
    typedef uint64_t PointId;
    typedef typename std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>> Polytope;
    typedef typename SparseModelType::ValueType ModelValueType;

    class Point {
       public:
        Point(std::vector<GeometryValueType> const& coordinates);
        Point(std::vector<GeometryValueType>&& coordinates);

        std::vector<GeometryValueType> const& get() const;
        std::vector<GeometryValueType>& get();

        uint64_t dimension() const;

        enum class DominanceResult { Incomparable, Dominates, Dominated, Equal };
        DominanceResult getDominance(Point const& other) const;

        void setOnFacet(bool value = true);
        bool liesOnFacet() const;
        void setParetoOptimal(bool value = true);
        bool isParetoOptimal() const;

        std::string toString(bool convertToDouble = false) const;

       private:
        std::vector<GeometryValueType> coordinates;
        bool onFacet;
        bool paretoOptimal;
    };

    class Pointset {
       public:
        typedef typename std::map<PointId, Point>::const_iterator iterator_type;

        Pointset();

        /*!
         * If the given point is not dominated by another point in the set, it is added
         * to the set and its ID is returned.
         * If the point is dominated by another point, boost::none is returned.
         * Erases all points in the set, that are dominated by the given point.
         * If the same point is already contained in the set, its id is returned
         */
        boost::optional<PointId> addPoint(Environment const& env, Point&& point);

        /*!
         * Returns the point with the given ID
         */
        Point const& getPoint(PointId const& id) const;

        iterator_type begin() const;
        iterator_type end() const;

        /*!
         * Returns the number of points currently contained in the set
         */
        uint64_t size() const;

        /*!
         * Returns the downward closure of the contained points.
         */
        Polytope downwardClosure() const;

        void collectPointsInPolytope(std::set<PointId>& collectedPoints, Polytope const& polytope);

        void printToStream(std::ostream& out, bool includeIDs = true, bool convertToDouble = false);

       private:
        std::map<PointId, Point> points;
        PointId currId;
    };

    class Facet {
       public:
        Facet(storm::storage::geometry::Halfspace<GeometryValueType> const& halfspace);
        Facet(storm::storage::geometry::Halfspace<GeometryValueType>&& halfspace);
        storm::storage::geometry::Halfspace<GeometryValueType> const& getHalfspace() const;
        void addPoint(PointId const& pointId, Point const& point);
        std::vector<PointId> const& getPoints() const;
        uint64_t getNumberOfPoints() const;

        /*!
         * Creates a polytope that captures all points that lie 'under' the facet
         */
        Polytope getInducedPolytope(Pointset const& pointset, std::vector<GeometryValueType> const& referenceCoordinates);

       private:
        storm::storage::geometry::Halfspace<GeometryValueType> halfspace;
        std::vector<PointId> pointsOnFacet;
    };

    DeterministicSchedsParetoExplorer(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>& preprocessorResult);

    virtual std::unique_ptr<CheckResult> check(Environment const& env);

    void exportPlotOfCurrentApproximation(Environment const& env);

   private:
    /*!
     * Cleans up all cached results from a previous check call
     */
    void clean();

    /*!
     * Intersects the overapproximation with the given halfspace
     */
    void addHalfspaceToOverApproximation(Environment const& env, std::vector<GeometryValueType> const& normalVector, GeometryValueType const& offset);

    /*!
     * Adds a polytope which consists of unachievable points
     */
    void addUnachievableArea(Environment const& env, Polytope const& area);

    /*!
     *   Builds the initial facets by optimizing the objectives individually.
     *   Adds the facets that need further processing to unprocessedFacets
     */
    void initializeFacets(Environment const& env);

    /*!
     *  Gets reference coordinates used to subdividing the downwardclosure
     */
    std::vector<GeometryValueType> getReferenceCoordinates(Environment const& env) const;

    /*!
     * Processes the given facet
     */
    void processFacet(Environment const& env, Facet& f);

    /*!
     * Optimizes in the facet direction. If this results in a point that does not lie on the facet,
     * 1. The new Pareto optimal point is added
     * 2. New facets are generated and (if not already precise enough) added to unprocessedFacets
     * 3. true is returned
     */
    bool optimizeAndSplitFacet(Environment const& env, Facet& f);

    Polytope negateMinObjectives(Polytope const& polytope) const;
    void negateMinObjectives(std::vector<GeometryValueType>& vector) const;

    Pointset pointset;
    std::queue<Facet> unprocessedFacets;
    Polytope overApproximation;
    std::vector<Polytope> unachievableAreas;
    std::vector<GeometryValueType> eps;
    std::shared_ptr<DeterministicSchedsLpChecker<SparseModelType, GeometryValueType>> lpChecker;
    std::unique_ptr<PcaaWeightVectorChecker<SparseModelType>> wvChecker;
    std::vector<DeterministicSchedsObjectiveHelper<SparseModelType>> objectiveHelper;

    std::shared_ptr<SparseModelType> const& model;
    uint64_t originalModelInitialState;
    std::vector<Objective<ModelValueType>> const& objectives;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm