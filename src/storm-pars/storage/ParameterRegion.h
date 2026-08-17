#pragma once

#include <map>

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/utility/parametric.h"
#include "storm/adapters/RationalFunctionAdapter.h"  // TODO: use forward header
#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace storage {
template<typename ParametricType>
class ParameterRegion {
   public:
    typedef typename storm::utility::parametric::VariableType<ParametricType>::type VariableType;
    typedef typename storm::utility::parametric::CoefficientType<ParametricType>::type CoefficientType;
    typedef typename storm::utility::parametric::Valuation<ParametricType> Valuation;

    ParameterRegion();
    ParameterRegion(Valuation const& lowerBoundaries, Valuation const& upperBoundaries);
    ParameterRegion(Valuation&& lowerBoundaries, Valuation&& upperBoundaries);
    ParameterRegion(ParameterRegion<ParametricType> const& other) = default;
    ParameterRegion(ParameterRegion<ParametricType>&& other) = default;
    ParameterRegion<ParametricType>& operator=(ParameterRegion<ParametricType> const& other) = default;

    virtual ~ParameterRegion() = default;

    std::set<VariableType> const& getVariables() const;
    std::multimap<CoefficientType, VariableType> const& getVariablesSorted() const;
    CoefficientType const& getLowerBoundary(VariableType const& variable) const;
    CoefficientType const& getLowerBoundary(const std::string varName) const;
    CoefficientType const& getUpperBoundary(VariableType const& variable) const;
    CoefficientType const& getUpperBoundary(const std::string varName) const;
    CoefficientType getDifference(const std::string varName) const;
    CoefficientType getDifference(VariableType const& variable) const;
    CoefficientType getCenter(const std::string varName) const;
    CoefficientType getCenter(VariableType const& variable) const;
    Valuation const& getLowerBoundaries() const;
    Valuation const& getUpperBoundaries() const;

    /*!
     * Returns a vector of all possible combinations of lower and upper bounds of the given variables.
     * The first entry of the returned vector will map every variable to its lower bound
     * The second entry will map every variable to its lower bound, except the first one (i.e. *getVariables.begin())
     * ...
     * The last entry will map every variable to its upper bound
     *
     * If the given set of variables is empty, the returned vector will contain an empty map
     */
    std::vector<Valuation> getVerticesOfRegion(std::set<VariableType> const& consideredVariables) const;

    /*!
     * Returns some point that lies within this region
     */
    Valuation getSomePoint() const;

    /*!
     * Returns the center point of this region
     */
    Valuation getCenterPoint() const;

    /*!
     * Returns the area of this region
     */
    CoefficientType area() const;

    /*!
     * Returns whether the given point is in this region
     */
    bool contains(Valuation const& point) const;

    /*!
     * Splits the region at the given point and inserts the resulting subregions at the end of the given vector.
     * It is assumed that the point lies within this region.
     * Subregions with area()==0 are not inserted in the vector.
     */
    void split(Valuation const& splittingPoint, std::vector<ParameterRegion<ParametricType>>& regionVector) const;
    void split(Valuation const& splittingPoint, std::vector<ParameterRegion<ParametricType>>& regionVector, std::set<VariableType> const& consideredVariables,
               std::set<VariableType> const& discreteVariables) const;

    Valuation getPoint(storm::solver::OptimizationDirection dir, storm::analysis::MonotonicityResult<VariableType>& monRes) const;
    Valuation getPoint(storm::solver::OptimizationDirection dir, std::set<VariableType> const& possibleMonotoneIncrParameters,
                       std::set<VariableType> const& monDecrParameters) const;

    // returns the region as string in the format 0.3<=p<=0.4,0.2<=q<=0.5;
    std::string toString(bool boundariesAsDouble = false) const;

    bool isSubRegion(ParameterRegion<ParametricType> region);

    CoefficientType getBoundParent();
    void setBoundParent(CoefficientType bound);

   private:
    void init();

    bool lastSplitMonotone = false;
    uint_fast64_t splitThreshold;

    Valuation lowerBoundaries;
    Valuation upperBoundaries;
    std::set<VariableType> variables;
    std::multimap<CoefficientType, VariableType> sortedOnDifference;
    CoefficientType parentBound;
};

template<typename ParametricType>
std::ostream& operator<<(std::ostream& out, ParameterRegion<ParametricType> const& region);

}  // namespace storage
}  // namespace storm
