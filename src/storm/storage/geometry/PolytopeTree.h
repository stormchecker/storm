#pragma once

#include <memory>
#include <vector>
#include "storm/storage/geometry/Polytope.h"

namespace storm {
namespace storage {
namespace geometry {

/*!
 * Represents a set of points in Euclidean space.
 * The set is defined as the union of the polytopes at the leafs of the tree.
 * The polytope at inner nodes should always be the convex union of its children.
 * The sets described by the children of a node are disjoint.
 * A child is always non-empty, i.e., isEmpty() should only hold for the root node.
 */
template<typename ValueType>
class PolytopeTree {
   public:
    PolytopeTree(std::shared_ptr<Polytope<ValueType>> const& polytope = nullptr) : polytope(polytope) {
        // Intentionally left empty
    }

    /*!
     * Substracts the given rhs from this polytope.
     * Points that lie on the boundary of rhs might still be included.
     */
    void setMinus(std::shared_ptr<Polytope<ValueType>> const& rhs) {
        // This operation only has an effect if the intersection of this and rhs is non-empty.
        if (!isEmpty() && !polytope->intersection(rhs)->isEmpty()) {
            if (children.empty()) {
                // This is a leaf node.
                // Apply splitting.
                auto newChildren = polytope->setMinus(rhs);
                if (newChildren.empty()) {
                    // Delete this node.
                    polytope = nullptr;
                } else if (newChildren.size() == 1) {
                    // Replace this node with its only child
                    polytope = newChildren.front()->clean();
                } else {
                    // Add the new children to this node. There is no need to traverse them.
                    for (auto& c : newChildren) {
                        children.push_back(c->clean());
                    }
                }
            } else {
                // This is an inner node. Traverse the children and set this to the convex union of its children.
                std::vector<PolytopeTree<ValueType>> newChildren;
                std::vector<std::vector<ValueType>> newPolytopeVertices;
                for (auto& c : children) {
                    c.setMinus(rhs);
                    if (c.polytope != nullptr) {
                        newChildren.push_back(c);
                        auto cVertices = c.polytope->getVertices();
                        newPolytopeVertices.insert(newPolytopeVertices.end(), cVertices.begin(), cVertices.end());
                    }
                }
                if (newPolytopeVertices.empty()) {
                    polytope = nullptr;
                } else {
                    polytope = storm::storage::geometry::Polytope<ValueType>::create(newPolytopeVertices);
                }
                children = std::move(newChildren);
            }
        }
    }

    /*!
     * Substracts the downward closure of the given point from this set.
     * @param point the given point
     * @param offset coordinates that are added to the point before taking its downward closure
     */
    void substractDownwardClosure(std::vector<ValueType> const& point) {
        setMinus(Polytope<ValueType>::createDownwardClosure({point}));
    }

    /*!
     * Substracts the downward closure of the given point from this set.
     * @param point the given point
     * @param offset coordinates that are added to the point before taking its downward closure
     */
    void substractDownwardClosure(std::vector<ValueType> const& point, std::vector<ValueType> const& offset) {
        STORM_LOG_ASSERT(point.size() == offset.size(), "Point/offset size mismatch.");
        std::vector<ValueType> pointPrime(point.size());
        storm::utility::vector::addVectors(point, offset, pointPrime);
        setMinus(Polytope<ValueType>::createDownwardClosure({pointPrime}));
    }

    /*!
     * Returns true if this is the empty set.
     */
    bool isEmpty() const {
        return polytope == nullptr;
    }

    /*!
     * Clears all contents of this set, making it the empty set.
     */
    void clear() {
        children.clear();
        polytope = nullptr;
    }

    /*!
     * Gets the polytope at this node
     */
    std::shared_ptr<Polytope<ValueType>>& getPolytope() {
        return polytope;
    }

    /*!
     * Gets the children at this node.
     */
    std::vector<PolytopeTree>& getChildren() {
        return children;
    }

    std::string toId() {
        if (isEmpty()) {
            return "empty";
        }
        std::stringstream s;
        s << "p";
        auto vertices = getPolytope()->getVertices();
        for (auto const& v : vertices) {
            s << "_";
            for (auto const& vi : v) {
                s << storm::utility::convertNumber<double>(vi) << "-";
            }
        }
        s << "_id" << children.data();
        return s.str();
    }

    /*!
     * Returns a string representation of this node (for debugging purposes)
     */
    std::string toString() {
        if (isEmpty()) {
            return "Empty PolytopeTree";
        }
        std::stringstream s;
        s << "PolytopeTree node with " << getChildren().size() << " children: " << getPolytope()->toString(true) << "\nVertices: ";
        auto vertices = getPolytope()->getVertices();
        for (auto const& v : vertices) {
            s << "[";
            for (auto const& vi : v) {
                s << storm::utility::convertNumber<double>(vi) << ",";
            }
            s << "]\t";
        }
        s << '\n';
        return s.str();
    }

   private:
    std::shared_ptr<Polytope<ValueType>> polytope;
    std::vector<PolytopeTree<ValueType>> children;
};
}  // namespace geometry
}  // namespace storage
}  // namespace storm
