#pragma once

#include <vector>

#include "storm/modelchecker/results/CheckResult.h"
#include "storm/storage/geometry/Polytope.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {
namespace modelchecker {
template<typename ValueType>
class ParetoCurveCheckResult : public CheckResult {
   public:
    typedef std::vector<ValueType> point_type;
    typedef std::vector<storm::utility::ExtendedValueType<ValueType>> ExtendedPointType;
    typedef std::shared_ptr<storm::storage::geometry::Polytope<ValueType>> polytope_type;

    ParetoCurveCheckResult();

    virtual bool isParetoCurveCheckResult() const override;

    std::vector<ExtendedPointType> const& getPoints() const;
    bool hasUnderApproximation() const;
    bool hasOverApproximation() const;
    polytope_type const& getUnderApproximation() const;
    polytope_type const& getOverApproximation() const;

    virtual std::ostream& writeToStream(std::ostream& out) const override;

   protected:
    ParetoCurveCheckResult(std::vector<ExtendedPointType> const& points, polytope_type const& underApproximation = nullptr,
                           polytope_type const& overApproximation = nullptr);
    ParetoCurveCheckResult(std::vector<ExtendedPointType>&& points, polytope_type&& underApproximation = nullptr, polytope_type&& overApproximation = nullptr);

    /*!
     * Takes points whose coordinates are all finite.
     */
    ParetoCurveCheckResult(std::vector<point_type> const& points, polytope_type const& underApproximation = nullptr,
                           polytope_type const& overApproximation = nullptr)
        requires(!std::is_same_v<ExtendedPointType, point_type>);

    // The pareto optimal points that have been found.
    std::vector<ExtendedPointType> points;

    // An underapproximation of the set of achievable values
    polytope_type underApproximation;

    // An overapproximation of the set of achievable values
    polytope_type overApproximation;
};
}  // namespace modelchecker
}  // namespace storm
