#include "storm-pars/modelchecker/results/RegionRefinementCheckResult.h"

#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
RegionRefinementCheckResult<ValueType>::RegionRefinementCheckResult(
    std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>> const& regionResults,
    storm::storage::ParameterRegion<ValueType> const& parameterSpace)
    : RegionCheckResult<ValueType>(regionResults), parameterSpace(parameterSpace) {
    this->initFractions(this->parameterSpace.area());
}

template<typename ValueType>
RegionRefinementCheckResult<ValueType>::RegionRefinementCheckResult(
    std::vector<std::pair<storm::storage::ParameterRegion<ValueType>, storm::modelchecker::RegionResult>>&& regionResults,
    storm::storage::ParameterRegion<ValueType>&& parameterSpace)
    : RegionCheckResult<ValueType>(std::move(regionResults)), parameterSpace(std::move(parameterSpace)) {
    this->initFractions(this->parameterSpace.area());
}

template<typename ValueType>
bool RegionRefinementCheckResult<ValueType>::isRegionRefinementCheckResult() const {
    return true;
}

template<typename ValueType>
storm::storage::ParameterRegion<ValueType> const& RegionRefinementCheckResult<ValueType>::getParameterSpace() const {
    return parameterSpace;
}

template<typename ValueType>
std::unique_ptr<CheckResult> RegionRefinementCheckResult<ValueType>::clone() const {
    return std::make_unique<RegionRefinementCheckResult<ValueType>>(this->regionResults, this->parameterSpace);
}

template<typename ValueType>
std::ostream& RegionRefinementCheckResult<ValueType>::writeIllustrationToStream(std::ostream& out) const {
    if (this->getParameterSpace().getVariables().size() == 2) {
        typedef typename storm::storage::ParameterRegion<ValueType>::CoefficientType CoefficientType;
        auto x = *this->getParameterSpace().getVariables().begin();
        auto y = *(this->getParameterSpace().getVariables().rbegin());

        uint_fast64_t const sizeX = 128;
        uint_fast64_t const sizeY = 64;

        out << "Region refinement Check result (visualization):\n";
        out << " \t x-axis: " << x << "  \t y-axis: " << y << "  \t S=safe, [ ]=unsafe, -=ambiguous \n";
        for (uint_fast64_t i = 0; i < sizeX + 2; ++i) {
            out << "#";
        }
        out << '\n';

        CoefficientType deltaX =
            (getParameterSpace().getUpperBoundary(x) - getParameterSpace().getLowerBoundary(x)) / storm::utility::convertNumber<CoefficientType>(sizeX);
        CoefficientType deltaY =
            (getParameterSpace().getUpperBoundary(y) - getParameterSpace().getLowerBoundary(y)) / storm::utility::convertNumber<CoefficientType>(sizeY);
        CoefficientType printedRegionArea = deltaX * deltaY;
        for (CoefficientType yUpper = getParameterSpace().getUpperBoundary(y); yUpper != getParameterSpace().getLowerBoundary(y); yUpper -= deltaY) {
            CoefficientType yLower = yUpper - deltaY;
            out << "#";
            for (CoefficientType xLower = getParameterSpace().getLowerBoundary(x); xLower != getParameterSpace().getUpperBoundary(x); xLower += deltaX) {
                CoefficientType xUpper = xLower + deltaX;
                bool currRegionSafe = false;
                bool currRegionUnSafe = false;
                bool currRegionIllDefined = false;
                bool currRegionComplete = false;
                CoefficientType coveredArea = storm::utility::zero<CoefficientType>();
                for (auto const& r : this->getRegionResults()) {
                    if (r.second != storm::modelchecker::RegionResult::AllSat && r.second != storm::modelchecker::RegionResult::AllViolated &&
                        r.second != storm::modelchecker::RegionResult::AllIllDefined) {
                        continue;
                    }
                    CoefficientType interesctionSizeY = std::min(yUpper, r.first.getUpperBoundary(y)) - std::max(yLower, r.first.getLowerBoundary(y));
                    interesctionSizeY = std::max(interesctionSizeY, storm::utility::zero<CoefficientType>());
                    CoefficientType interesctionSizeX = std::min(xUpper, r.first.getUpperBoundary(x)) - std::max(xLower, r.first.getLowerBoundary(x));
                    interesctionSizeX = std::max(interesctionSizeX, storm::utility::zero<CoefficientType>());
                    CoefficientType intersectionArea = interesctionSizeY * interesctionSizeX;
                    if (!storm::utility::isZero(intersectionArea)) {
                        currRegionSafe = currRegionSafe || r.second == storm::modelchecker::RegionResult::AllSat;
                        currRegionUnSafe = currRegionUnSafe || r.second == storm::modelchecker::RegionResult::AllViolated;
                        currRegionIllDefined = currRegionIllDefined || r.second == storm::modelchecker::RegionResult::AllIllDefined;
                        coveredArea += intersectionArea;
                        if (currRegionSafe && currRegionUnSafe) {
                            break;
                        }
                        if (coveredArea == printedRegionArea) {
                            currRegionComplete = true;
                            break;
                        }
                    }
                }
                if (currRegionComplete && currRegionSafe && !currRegionUnSafe) {
                    out << "S";
                } else if (currRegionComplete && currRegionUnSafe && !currRegionSafe) {
                    out << " ";
                } else if (currRegionComplete && currRegionIllDefined) {
                    out << "*";
                } else {
                    out << "-";
                }
            }
            out << "#\n";
        }
        for (uint_fast64_t i = 0; i < sizeX + 2; ++i) {
            out << "#";
        }
        out << '\n';
    } else {
        STORM_LOG_WARN("Writing illustration of region check result to a stream is only implemented for two parameters.");
    }
    return out;
}

template class RegionRefinementCheckResult<storm::RationalFunction>;
}  // namespace modelchecker
}  // namespace storm
