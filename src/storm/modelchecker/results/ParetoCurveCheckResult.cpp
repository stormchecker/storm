#include "storm/modelchecker/results/ParetoCurveCheckResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
ParetoCurveCheckResult<ValueType>::ParetoCurveCheckResult() {
    // Intentionally left empty.
}

template<typename ValueType>
ParetoCurveCheckResult<ValueType>::ParetoCurveCheckResult(std::vector<point_type> const& points, polytope_type const& underApproximation,
                                                          polytope_type const& overApproximation)
    : points(points), underApproximation(underApproximation), overApproximation(overApproximation) {
    // Intentionally left empty.
}

template<typename ValueType>
ParetoCurveCheckResult<ValueType>::ParetoCurveCheckResult(std::vector<point_type>&& points, polytope_type&& underApproximation,
                                                          polytope_type&& overApproximation)
    : points(points), underApproximation(underApproximation), overApproximation(overApproximation) {
    // Intentionally left empty.
}

template<typename ValueType>
bool ParetoCurveCheckResult<ValueType>::isParetoCurveCheckResult() const {
    return true;
}

template<typename ValueType>
std::vector<typename ParetoCurveCheckResult<ValueType>::point_type> const& ParetoCurveCheckResult<ValueType>::getPoints() const {
    return points;
}

template<typename ValueType>
bool ParetoCurveCheckResult<ValueType>::hasUnderApproximation() const {
    return bool(underApproximation);
}

template<typename ValueType>
bool ParetoCurveCheckResult<ValueType>::hasOverApproximation() const {
    return bool(overApproximation);
}

template<typename ValueType>
typename ParetoCurveCheckResult<ValueType>::polytope_type const& ParetoCurveCheckResult<ValueType>::getUnderApproximation() const {
    STORM_LOG_ASSERT(hasUnderApproximation(), "Requested under approx. of Pareto curve although it does not exist.");
    return underApproximation;
}

template<typename ValueType>
typename ParetoCurveCheckResult<ValueType>::polytope_type const& ParetoCurveCheckResult<ValueType>::getOverApproximation() const {
    STORM_LOG_ASSERT(hasUnderApproximation(), "Requested over approx. of Pareto curve although it does not exist.");
    return overApproximation;
}

template<typename ValueType>
std::ostream& ParetoCurveCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    out << '\n';
    if (hasUnderApproximation()) {
        out << "Underapproximation of achievable values: " << underApproximation->toString() << '\n';
    }
    if (hasOverApproximation()) {
        out << "Overapproximation of achievable values: " << overApproximation->toString() << '\n';
    }
    out << points.size() << " Pareto optimal points found:\n";
    for (auto const& p : points) {
        out << "   (";
        for (auto it = p.begin(); it != p.end(); ++it) {
            if (it != p.begin()) {
                out << ", ";
            }
            out << std::setw(storm::NumberTraits<ValueType>::IsExact ? 20 : 11) << *it;
        }
        out << " )";
        if (storm::NumberTraits<ValueType>::IsExact) {
            out << " approx. ";
            out << "   (";
            for (auto it = p.begin(); it != p.end(); ++it) {
                if (it != p.begin()) {
                    out << ", ";
                }
                out << std::setw(11) << storm::utility::convertNumber<double>(*it);
            }
            out << " )";
        }
        out << '\n';
    }
    return out;
}

template class ParetoCurveCheckResult<double>;
template class ParetoCurveCheckResult<storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace storm
