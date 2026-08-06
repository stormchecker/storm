#pragma once

#include <memory>
#include "storm/adapters/EigenAdapter.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/geometry/Polytope.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
class NativePolytope : public Polytope<ValueType> {
   public:
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic> EigenMatrix;
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, 1> EigenVector;

    enum class EmptyStatus {
        Unknown,  // It is unknown whether the polytope is empty or not
        Empty,    // The polytope is empty
        Nonempty  // the polytope is not empty
    };

    typedef typename Polytope<ValueType>::Point Point;

    /*!
     * Creates a NativePolytope from the given halfspaces or points.
     * If both representations are given, one of them might be ignored
     */
    static std::shared_ptr<Polytope<ValueType>> create(boost::optional<std::vector<Halfspace<ValueType>>> const& halfspaces,
                                                       boost::optional<std::vector<Point>> const& points);

    /*!
     * Creates a NativePolytope from the given halfspaces
     * The resulting polytope is defined as the intersection of the halfspaces.
     */
    NativePolytope(std::vector<Halfspace<ValueType>> const& halfspaces);

    /*!
     * Creates a NativePolytope from the given points.
     * The resulting polytope is defined as the convex hull of the points'
     */
    NativePolytope(std::vector<Point> const& points);

    /*!
     * Copy and move constructors
     */
    NativePolytope(NativePolytope<ValueType> const& other);
    NativePolytope(NativePolytope<ValueType>&& other);

    /*!
     * Construction from intern data
     */
    NativePolytope(EmptyStatus const& emptyStatus, EigenMatrix const& halfspaceMatrix, EigenVector const& halfspaceVector);
    NativePolytope(EmptyStatus&& emptyStatus, EigenMatrix&& halfspaceMatrix, EigenVector&& halfspaceVector);

    virtual ~NativePolytope();

    /*!
     * Returns the vertices of this polytope.
     */
    virtual std::vector<Point> getVertices() const override;

    /*!
     * Returns the halfspaces of this polytope.
     */
    virtual std::vector<Halfspace<ValueType>> getHalfspaces() const override;

    /*!
     * Returns whether this polytope is the empty set.
     */
    virtual bool isEmpty() const override;

    /*!
     * Returns whether this polytope is universal (i.e., equals R^n).
     */
    virtual bool isUniversal() const override;

    /*!
     * Returns true iff the given point is inside of the polytope.
     */
    virtual bool contains(Point const& point) const override;

    /*!
     * Returns true iff the given polytope is a subset of this polytope.
     */
    virtual bool contains(std::shared_ptr<Polytope<ValueType>> const& other) const override;

    /*!
     * Intersects this polytope with rhs and returns the result.
     */
    virtual std::shared_ptr<Polytope<ValueType>> intersection(std::shared_ptr<Polytope<ValueType>> const& rhs) const override;
    virtual std::shared_ptr<Polytope<ValueType>> intersection(Halfspace<ValueType> const& halfspace) const override;

    /*!
     * Returns the convex union of this polytope and rhs.
     */
    virtual std::shared_ptr<Polytope<ValueType>> convexUnion(std::shared_ptr<Polytope<ValueType>> const& rhs) const override;

    /*!
     * Returns the minkowskiSum of this polytope and rhs.
     */
    virtual std::shared_ptr<Polytope<ValueType>> minkowskiSum(std::shared_ptr<Polytope<ValueType>> const& rhs) const override;

    /*!
     * Returns the affine transformation of this polytope P w.r.t. the given matrix A and vector b.
     * The result is the set {A*x+b | x \in P}
     *
     * @param matrix the transformation matrix, given as vector of rows
     * @param vector the transformation offset
     */
    virtual std::shared_ptr<Polytope<ValueType>> affineTransformation(std::vector<Point> const& matrix, Point const& vector) const override;

    /*!
     * Finds an optimal point inside this polytope w.r.t. the given direction, i.e.,
     * a point that maximizes dotPorduct(point, direction).
     * If such a point does not exist, the returned bool is false. There are two reasons for this:
     * - The polytope is empty
     * - The polytope is not bounded in the given direction
     */
    virtual std::pair<Point, bool> optimize(Point const& direction) const override;

    /*!
     * declares one variable for each dimension and returns the obtained variables.
     * @param manager The expression manager that keeps track of the variables
     * @param namePrefix The prefix that is prepanded to the variable index
     */
    virtual std::vector<storm::expressions::Variable> declareVariables(storm::expressions::ExpressionManager& manager,
                                                                       std::string const& namePrefix) const override;

    /*!
     * returns the constrains defined by this polytope as an expression over the given variables
     */
    virtual std::vector<storm::expressions::Expression> getConstraints(storm::expressions::ExpressionManager const& manager,
                                                                       std::vector<storm::expressions::Variable> const& variables) const override;

    virtual bool isNativePolytope() const override;

    virtual std::shared_ptr<Polytope<ValueType>> clean() override;

   private:
    // returns the vertices of this polytope as EigenVectors
    std::vector<EigenVector> getEigenVertices() const;

    // As optimize(..) but with EigenVectors
    std::pair<EigenVector, bool> optimize(EigenVector const& direction) const;

    // Stores whether the polytope is empty or not
    mutable EmptyStatus emptyStatus;

    // Intern representation of the polytope as { x | Ax<=b }
    EigenMatrix A;
    EigenVector b;
};

}  // namespace geometry
}  // namespace storage
}  // namespace storm
