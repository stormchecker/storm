#pragma once

#include <set>

#include "storm/adapters/EigenAdapter.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
class QuickHull {
   public:
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic> EigenMatrix;
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, 1> EigenVector;

    QuickHull() = default;
    ~QuickHull() = default;

    /*
     * Generates the halfspaces of the given set of Points by the QuickHull-algorithm
     * If the given flag is true, this method will also compute
     *  * the minimal set of vertices which represent the given polytope (can be used to remove redundant vertices), and
     *  * for each hyperplane, the set of (non-redundant) vertices that lie on each hyperplane.
     *
     * Use the provided getter methods to retrieve the results
     *
     * @return true iff conversion was successful.
     */
    void generateHalfspacesFromPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets);

    EigenMatrix& getResultMatrix();

    EigenVector& getResultVector();

    /*!
     * Returns the set of vertices which are not redundant
     * @note the returned vector is empty if the corresponding flag was false
     */
    std::vector<EigenVector>& getRelevantVertices();

    /*!
     * Returns for each hyperplane the set of vertices that lie on that hyperplane.
     * A vertex is given as an index in the relevantVertices vector.
     * @note the returned vector is empty if the corresponding flag was false
     */
    std::vector<std::vector<std::uint_fast64_t>>& getVertexSets();

   private:
    struct Facet {
        EigenVector normal;
        ValueType offset;
        std::vector<uint_fast64_t> points;
        std::vector<uint_fast64_t> neighbors;
        // maxOutsidePointIndex and outsideSet will be set in Quickhull algorithm
        std::vector<uint_fast64_t> outsideSet;
        uint_fast64_t maxOutsidePointIndex;
    };

    /*
     * Properly handles the 1D case
     *
     */
    void handle1DPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets);

    /*
     * Returns true if the vertices with index of subset and item are affine independent
     * Note that this filter also works for dimension()+1 many vertices
     */
    static bool affineFilter(std::vector<uint_fast64_t> const& subset, uint_fast64_t const& item, std::vector<EigenVector> const& vertices);

    /*
     * handles degenerated polytopes
     *
     */
    void handleAffineDependentPoints(std::vector<EigenVector>& points, bool generateRelevantVerticesAndVertexSets);

    /*!
     * finds a set of vertices that correspond to a (hopefully) large V polytope.
     *
     * @param points The set of points from which vertices are picked. Note that the order of the points might be changed when calling this!!
     * @param verticesOfInitialPolytope Will be  set to the resulting vertices (represented by indices w.r.t. the given points)
     * @return true if the method was successful. False if the given points are affine dependend, i.e. the polytope is degenerated.
     */
    bool findInitialVertices(std::vector<EigenVector>& points, std::vector<uint_fast64_t>& verticesOfInitialPolytope) const;

    /*!
     * Computes the initial facets out of the given dimension+1 initial vertices
     */
    std::vector<Facet> computeInitialFacets(std::vector<EigenVector> const& points, std::vector<uint_fast64_t> const& verticesOfInitialPolytope,
                                            EigenVector const& insidePoint) const;

    // Computes the normal vector and the offset of the given facet from the (dimension many) points specified in the facet.
    // The insidePoint specifies the orientation of the facet.
    void computeNormalAndOffsetOfFacet(std::vector<EigenVector> const& points, EigenVector const& insidePoint, Facet& facet) const;

    /*
     * Extends the given mesh using the QuickHull-algorithm
     * For optimization reasons a point thats inside of the initial polytope but on none of the facets has to be provided.

     */
    void extendMesh(std::vector<EigenVector>& points, std::vector<Facet>& facets, storm::storage::BitVector& currentFacets, EigenVector& insidePoint) const;

    /*!
     * Uses the provided mesh to generate a HPolytope (in form of a matrix and a vector)
     *  If the given flag is true, this method will also compute
     *  * the minimal set of vertices which represent the given vPoly (can be used to remove redundant vertices), and
     *  * for each hyperplane, the set of (non-redundant) vertices that lie on each hyperplane.
     *
     */
    void getPolytopeFromMesh(std::vector<EigenVector> const& points, std::vector<Facet> const& facets, storm::storage::BitVector const& currentFacets,
                             bool generateRelevantVerticesAndVertexSets);

    /*
     * Returns the set of facets visible from point starting with the facet with index startIndex and recursively testing all neighbors
     */
    std::set<uint_fast64_t> getVisibleSet(std::vector<Facet> const& facets, uint_fast64_t const& startIndex, EigenVector const& point) const;

    /*
     * Sets neighborhood for all facets with index >= firstNewFacet in facets
     */
    void setNeighborhoodOfNewFacets(std::vector<Facet>& facets, uint_fast64_t firstNewFacet, uint_fast64_t dimension) const;

    /*
     * replaces oldFacet by newFacet in the neighborhood of neighbor
     */
    void replaceFacetNeighbor(std::vector<Facet>& facets, uint_fast64_t oldFacetIndex, uint_fast64_t newFacetIndex, uint_fast64_t neighborIndex) const;

    /*
     * computes the outside set of the given facet
     */
    void computeOutsideSetOfFacet(Facet& facet, storm::storage::BitVector& currentOutsidePoints, std::vector<EigenVector> const& points) const;

    /*
     * returns common points of lhs and rhs
     */
    std::vector<uint_fast64_t> getCommonPoints(Facet const& lhs, Facet const& rhs) const;

    /*
     * computes all neighbors that are not in the visibleSet
     */
    std::set<uint_fast64_t> getInvisibleNeighbors(std::vector<Facet>& facets, std::set<uint_fast64_t> const& visibleSet) const;

    EigenMatrix resultMatrix;
    EigenVector resultVector;
    std::vector<EigenVector> relevantVertices;
    std::vector<std::vector<uint_fast64_t>> vertexSets;
};
}  // namespace geometry
}  // namespace storage
}  // namespace storm
