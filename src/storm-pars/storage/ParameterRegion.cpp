#include "storm-pars/storage/ParameterRegion.h"

#include <limits>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename ParametricType>
ParameterRegion<ParametricType>::ParameterRegion() {
    init();
}

template<typename ParametricType>
ParameterRegion<ParametricType>::ParameterRegion(Valuation const& lowerBoundaries, Valuation const& upperBoundaries)
    : lowerBoundaries(lowerBoundaries), upperBoundaries(upperBoundaries) {
    init();
}

template<typename ParametricType>
ParameterRegion<ParametricType>::ParameterRegion(Valuation&& lowerBoundaries, Valuation&& upperBoundaries)
    : lowerBoundaries(lowerBoundaries), upperBoundaries(upperBoundaries) {
    init();
}

template<typename ParametricType>
void ParameterRegion<ParametricType>::init() {
    // check whether both mappings map the same variables, check that lower boundary <= upper boundary,  and pre-compute the set of variables
    for (auto const& variableWithLowerBoundary : this->lowerBoundaries) {
        auto variableWithUpperBoundary = this->upperBoundaries.find(variableWithLowerBoundary.first);
        STORM_LOG_THROW((variableWithUpperBoundary != upperBoundaries.end()), storm::exceptions::InvalidArgumentException,
                        "Could not create region. No upper boundary specified for Variable " << variableWithLowerBoundary.first << ".");
        STORM_LOG_THROW((variableWithLowerBoundary.second <= variableWithUpperBoundary->second), storm::exceptions::InvalidArgumentException,
                        "Could not create region. The lower boundary for " << variableWithLowerBoundary.first << " is larger then the upper boundary.");
        this->variables.insert(variableWithLowerBoundary.first);
        this->sortedOnDifference.insert({variableWithLowerBoundary.second - variableWithUpperBoundary->second, variableWithLowerBoundary.first});
    }
    for (auto const& variableWithBoundary : this->upperBoundaries) {
        STORM_LOG_THROW((this->variables.find(variableWithBoundary.first) != this->variables.end()), storm::exceptions::InvalidArgumentException,
                        "Could not create region. No lower boundary specified for Variable " << variableWithBoundary.first << ".");
    }
    this->splitThreshold = variables.size();
}

template<typename ParametricType>
std::set<typename ParameterRegion<ParametricType>::VariableType> const& ParameterRegion<ParametricType>::getVariables() const {
    return this->variables;
}

template<typename ParametricType>
std::multimap<typename ParameterRegion<ParametricType>::CoefficientType, typename ParameterRegion<ParametricType>::VariableType> const&
ParameterRegion<ParametricType>::getVariablesSorted() const {
    return this->sortedOnDifference;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType const& ParameterRegion<ParametricType>::getLowerBoundary(VariableType const& variable) const {
    auto const& result = lowerBoundaries.find(variable);
    STORM_LOG_THROW(result != lowerBoundaries.end(), storm::exceptions::InvalidArgumentException,
                    "Tried to find a lower boundary for variable " << variable << " which is not specified by this region.");
    return (*result).second;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType const& ParameterRegion<ParametricType>::getLowerBoundary(const std::string varName) const {
    for (auto itr = lowerBoundaries.begin(); itr != lowerBoundaries.end(); ++itr) {
        if (itr->first.name().compare(varName) == 0) {
            return (*itr).second;
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                    "Tried to find a lower boundary for variableName " << varName << " which is not specified by this region.");
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType const& ParameterRegion<ParametricType>::getUpperBoundary(VariableType const& variable) const {
    auto const& result = upperBoundaries.find(variable);
    STORM_LOG_THROW(result != upperBoundaries.end(), storm::exceptions::InvalidArgumentException,
                    "Tried to find an upper boundary for variable " << variable << " which is not specified by this region.");
    return (*result).second;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType const& ParameterRegion<ParametricType>::getUpperBoundary(const std::string varName) const {
    for (auto itr = upperBoundaries.begin(); itr != upperBoundaries.end(); ++itr) {
        if (itr->first.name().compare(varName) == 0) {
            return (*itr).second;
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                    "Tried to find an upper boundary for variableName " << varName << " which is not specified by this region.");
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::getDifference(VariableType const& variable) const {
    return getUpperBoundary(variable) - getLowerBoundary(variable);
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::getDifference(const std::string varName) const {
    return getUpperBoundary(varName) - getLowerBoundary(varName);
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::getCenter(VariableType const& variable) const {
    return (getLowerBoundary(variable) + getUpperBoundary(variable)) / 2;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::getCenter(const std::string varName) const {
    return (getLowerBoundary(varName) + getUpperBoundary(varName)) / 2;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation const& ParameterRegion<ParametricType>::getUpperBoundaries() const {
    return upperBoundaries;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation const& ParameterRegion<ParametricType>::getLowerBoundaries() const {
    return lowerBoundaries;
}

template<typename ParametricType>
std::vector<typename ParameterRegion<ParametricType>::Valuation> ParameterRegion<ParametricType>::getVerticesOfRegion(
    std::set<VariableType> const& consideredVariables) const {
    std::size_t const numOfVariables = consideredVariables.size();
    STORM_LOG_THROW(numOfVariables <= std::numeric_limits<std::size_t>::digits, storm::exceptions::OutOfRangeException,
                    "Number of variables " << numOfVariables << " is too high.");
    std::size_t const numOfVertices = std::pow(2, numOfVariables);
    std::vector<Valuation> resultingVector(numOfVertices);

    for (uint_fast64_t vertexId = 0; vertexId < numOfVertices; ++vertexId) {
        // interprete vertexId as a bit sequence
        // the consideredVariables.size() least significant bits of vertex will always represent the next vertex
        //(00...0 = lower boundaries for all variables, 11...1 = upper boundaries for all variables)
        uint_fast64_t variableIndex = 0;

        for (auto variable : consideredVariables) {
            if ((vertexId >> variableIndex) % 2 == 0) {
                resultingVector[vertexId].insert(std::pair<VariableType, CoefficientType>(variable, getLowerBoundary(variable)));
            } else {
                resultingVector[vertexId].insert(std::pair<VariableType, CoefficientType>(variable, getUpperBoundary(variable)));
            }
            ++variableIndex;
        }
    }
    return resultingVector;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation ParameterRegion<ParametricType>::getSomePoint() const {
    return this->getLowerBoundaries();
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation ParameterRegion<ParametricType>::getCenterPoint() const {
    Valuation result;
    for (auto const& variable : this->variables) {
        result.emplace(variable, getCenter(variable));
    }
    return result;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::area() const {
    CoefficientType result = storm::utility::one<CoefficientType>();
    for (auto const& variable : this->variables) {
        if (this->getUpperBoundary(variable) != this->getLowerBoundary(variable)) {
            result *= (this->getUpperBoundary(variable) - this->getLowerBoundary(variable));
        } else {
            // HACK to get regions with zero area to work correctly
            // (It's a hack but it's a harmless one for now, as regions with zero area do not exist without discrete parameters)
            // This area represents half of the area of the region
            result /= utility::convertNumber<CoefficientType>(2);
        }
    }
    return result;
}

template<typename ParametricType>
bool ParameterRegion<ParametricType>::contains(Valuation const& point) const {
    for (auto const& variable : this->variables) {
        STORM_LOG_ASSERT(point.count(variable) > 0,
                         "Tried to check if a point is in a region, but the point does not contain a value for variable " << variable);
        auto pointEntry = point.find(variable)->second;
        if (pointEntry < this->getLowerBoundary(variable) || pointEntry > this->getUpperBoundary(variable)) {
            return false;
        }
    }
    return true;
}

template<typename ParametricType>
void ParameterRegion<ParametricType>::split(Valuation const& splittingPoint, std::vector<ParameterRegion<ParametricType>>& regionVector) const {
    return split(splittingPoint, regionVector, variables, {});
}

template<typename ParametricType>
void ParameterRegion<ParametricType>::split(Valuation const& splittingPoint, std::vector<storm::storage::ParameterRegion<ParametricType>>& regionVector,
                                            const std::set<VariableType>& consideredVariables, const std::set<VariableType>& discreteVariables) const {
    std::set<VariableType> vertexVariables = consideredVariables;
    // Remove the discrete variables that are already unit from the considered
    // variables set, so we don't split them again
    for (auto const& var : discreteVariables) {
        if (this->getDifference(var) == storm::utility::zero<CoefficientType>()) {
            vertexVariables.erase(var);
        }
    }

    auto vertices = getVerticesOfRegion(vertexVariables);

    for (auto const& vertex : vertices) {
        // The resulting subregion is the smallest region containing vertex and splittingPoint.
        Valuation subLower, subUpper;
        for (auto variableBound : this->lowerBoundaries) {
            VariableType variable = variableBound.first;
            auto vertexEntry = vertex.find(variable);
            if (vertexEntry != vertex.end()) {
                if (discreteVariables.find(variable) != discreteVariables.end()) {
                    // As this parameter is discrete, we set this parameter to the vertex entry (splitting point does not matter)
                    // e.g. we split the region p in [0,1], q in [0,1] at center with q being discrete
                    // then we get the regions [0,0.5]x[0,1] and [0.5,1]x[0,1]
                    subLower.insert(typename Valuation::value_type(variable, vertexEntry->second));
                    subUpper.insert(typename Valuation::value_type(variable, vertexEntry->second));
                } else {
                    auto splittingPointEntry = splittingPoint.find(variable);
                    subLower.insert(typename Valuation::value_type(variable, std::min(vertexEntry->second, splittingPointEntry->second)));
                    subUpper.insert(typename Valuation::value_type(variable, std::max(vertexEntry->second, splittingPointEntry->second)));
                }
            } else {
                subLower.insert(typename Valuation::value_type(variable, getLowerBoundary(variable)));
                subUpper.insert(typename Valuation::value_type(variable, getUpperBoundary(variable)));
            }
        }

        ParameterRegion<ParametricType> subRegion(std::move(subLower), std::move(subUpper));

        if (!storm::utility::isZero(subRegion.area())) {
            regionVector.push_back(std::move(subRegion));
        }
    }
}

template<typename ParametricType>
std::string ParameterRegion<ParametricType>::toString(bool boundariesAsDouble) const {
    std::stringstream regionstringstream;
    if (boundariesAsDouble) {
        for (auto var : this->getVariables()) {
            regionstringstream << storm::utility::convertNumber<double>(this->getLowerBoundary(var));
            regionstringstream << "<=";
            regionstringstream << var;
            regionstringstream << "<=";
            regionstringstream << storm::utility::convertNumber<double>(this->getUpperBoundary(var));
            regionstringstream << ",";
        }
    } else {
        for (auto var : this->getVariables()) {
            regionstringstream << this->getLowerBoundary(var);
            regionstringstream << "<=";
            regionstringstream << var;
            regionstringstream << "<=";
            regionstringstream << this->getUpperBoundary(var);
            regionstringstream << ",";
        }
    }
    std::string regionstring = regionstringstream.str();
    // the last comma should actually be a semicolon
    regionstring = regionstring.substr(0, regionstring.length() - 1) + ";";
    return regionstring;
}

template<typename ParametricType>
bool ParameterRegion<ParametricType>::isSubRegion(ParameterRegion<ParametricType> subRegion) {
    auto varsRegion = getVariables();
    auto varsSubRegion = subRegion.getVariables();
    for (auto var : varsRegion) {
        if (std::find(varsSubRegion.begin(), varsSubRegion.end(), var) != varsSubRegion.end()) {
            if (getLowerBoundary(var) > subRegion.getLowerBoundary(var) || getUpperBoundary(var) < getUpperBoundary(var)) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation ParameterRegion<ParametricType>::getPoint(storm::solver::OptimizationDirection dir,
                                                                                              storm::analysis::MonotonicityResult<VariableType>& monRes) const {
    auto val = this->getCenterPoint();
    for (auto monResEntry : monRes.getMonotonicityResult()) {
        if (monRes.isDoneForVar(monResEntry.first)) {
            if (monResEntry.second == storm::analysis::MonotonicityResult<VariableType>::Monotonicity::Incr) {
                val[monResEntry.first] = storm::solver::minimize(dir) ? getLowerBoundary(monResEntry.first) : getUpperBoundary(monResEntry.first);
            } else if (monResEntry.second == storm::analysis::MonotonicityResult<VariableType>::Monotonicity::Decr) {
                val[monResEntry.first] = storm::solver::maximize(dir) ? getLowerBoundary(monResEntry.first) : getUpperBoundary(monResEntry.first);
            }
        }
    }
    return val;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::Valuation ParameterRegion<ParametricType>::getPoint(storm::solver::OptimizationDirection dir,
                                                                                              std::set<VariableType> const& monIncrParameters,
                                                                                              std::set<VariableType> const& monDecrParameters) const {
    auto val = this->getCenterPoint();
    for (auto var : monIncrParameters) {
        val[var] = storm::solver::minimize(dir) ? getLowerBoundary(var) : getUpperBoundary(var);
    }
    for (auto var : monDecrParameters) {
        val[var] = storm::solver::maximize(dir) ? getLowerBoundary(var) : getUpperBoundary(var);
    }

    return val;
}

template<typename ParametricType>
typename ParameterRegion<ParametricType>::CoefficientType ParameterRegion<ParametricType>::getBoundParent() {
    return parentBound;
}

template<typename ParametricType>
void ParameterRegion<ParametricType>::setBoundParent(CoefficientType bound) {
    parentBound = bound;
}

template<typename ParametricType>
std::ostream& operator<<(std::ostream& out, ParameterRegion<ParametricType> const& region) {
    out << region.toString();
    return out;
}

template class ParameterRegion<storm::RationalFunction>;
template std::ostream& operator<<(std::ostream& out, ParameterRegion<storm::RationalFunction> const& region);

}  // namespace storage
}  // namespace storm
