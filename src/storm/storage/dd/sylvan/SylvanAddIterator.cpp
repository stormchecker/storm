#include "storm/storage/dd/sylvan/SylvanAddIterator.h"

#include <cmath>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/sylvan/InternalSylvanAdd.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_SYLVAN
template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType>::AddIterator() {
    // Intentionally left empty.
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType>::AddIterator(DdManager<DdType::Sylvan> const& ddManager, sylvan::Mtbdd mtbdd, sylvan::Bdd variables,
                                                    uint_fast64_t numberOfDdVariables, bool isAtEnd,
                                                    std::set<storm::expressions::Variable> const* metaVariables, bool enumerateDontCareMetaVariables)
    : ddManager(&ddManager),
      mtbdd(mtbdd),
      variables(variables),
      cube(numberOfDdVariables),
      leaf(),
      isAtEnd(isAtEnd),
      metaVariables(metaVariables),
      enumerateDontCareMetaVariables(enumerateDontCareMetaVariables),
      cubeCounter(0),
      relevantDontCareDdVariables(),
      currentValuation(ddManager.getExpressionManager().getSharedPointer()) {
    // If the given generator is not yet at its end, we need to create the current valuation from the cube from
    // scratch.
    if (!this->isAtEnd) {
        this->createGlobalToLocalIndexMapping();

        // And then get ready for treating the first cube.
        leaf = mtbdd_enum_first(mtbdd.GetMTBDD(), variables.GetBDD(), cube.data(), &mtbdd_isnonzero);

        if (leaf != mtbdd_false) {
            this->treatNewCube();
        } else {
            this->isAtEnd = true;
        }
    }
}

template<typename ValueType>
void AddIterator<DdType::Sylvan, ValueType>::createGlobalToLocalIndexMapping() {
    // Create the global to local index mapping.
    std::vector<uint_fast64_t> globalIndices;
    for (auto const& metaVariable : *this->metaVariables) {
        auto const& ddMetaVariable = this->ddManager->getMetaVariable(metaVariable);

        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            globalIndices.push_back(ddVariable.getIndex());
        }
    }

    std::sort(globalIndices.begin(), globalIndices.end());

    for (auto it = globalIndices.begin(), ite = globalIndices.end(); it != ite; ++it) {
        globalToLocalIndexMap[*it] = std::distance(globalIndices.begin(), it);
    }
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> AddIterator<DdType::Sylvan, ValueType>::createBeginIterator(DdManager<DdType::Sylvan> const& ddManager,
                                                                                                   sylvan::Mtbdd mtbdd, sylvan::Bdd variables,
                                                                                                   uint_fast64_t numberOfDdVariables,
                                                                                                   std::set<storm::expressions::Variable> const* metaVariables,
                                                                                                   bool enumerateDontCareMetaVariables) {
    return AddIterator<DdType::Sylvan, ValueType>(ddManager, mtbdd, variables, numberOfDdVariables, false, metaVariables, enumerateDontCareMetaVariables);
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType> AddIterator<DdType::Sylvan, ValueType>::createEndIterator(DdManager<DdType::Sylvan> const& ddManager) {
    return AddIterator<DdType::Sylvan, ValueType>(ddManager, sylvan::Mtbdd(), sylvan::Bdd(), 0, true, nullptr, false);
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType>& AddIterator<DdType::Sylvan, ValueType>::operator++() {
    STORM_LOG_ASSERT(!this->isAtEnd, "Illegally incrementing iterator that is already at its end.");

    // If there were no (relevant) don't cares or we have enumerated all combination, we need to eliminate the
    // found solutions and get the next "first" cube.
    if (this->relevantDontCareDdVariables.empty() || this->cubeCounter >= std::pow(2, this->relevantDontCareDdVariables.size()) - 1) {
        leaf = mtbdd_enum_next(mtbdd.GetMTBDD(), variables.GetBDD(), cube.data(), &mtbdd_isnonzero);

        if (leaf != mtbdd_false) {
            this->treatNewCube();
        } else {
            this->isAtEnd = true;
        }
    } else {
        // Treat the next solution of the cube.
        this->treatNextInCube();
    }

    return *this;
}

template<typename ValueType>
bool AddIterator<DdType::Sylvan, ValueType>::operator==(AddIterator<DdType::Sylvan, ValueType> const& other) const {
    if (this->isAtEnd && other.isAtEnd) {
        return true;
    } else {
        return this->ddManager == other.ddManager && this->mtbdd == other.mtbdd && this->variables == other.variables && this->cube == other.cube &&
               this->leaf == other.leaf && this->isAtEnd == other.isAtEnd && this->metaVariables == other.metaVariables &&
               this->cubeCounter == other.cubeCounter && this->relevantDontCareDdVariables == other.relevantDontCareDdVariables &&
               this->currentValuation == other.currentValuation;
    }
}

template<typename ValueType>
bool AddIterator<DdType::Sylvan, ValueType>::operator!=(AddIterator<DdType::Sylvan, ValueType> const& other) const {
    return !(*this == other);
}

template<typename ValueType>
std::pair<storm::expressions::SimpleValuation, ValueType> AddIterator<DdType::Sylvan, ValueType>::operator*() const {
    return std::make_pair(currentValuation, static_cast<ValueType>(InternalAdd<DdType::Sylvan, ValueType>::getValue(leaf)));
}

template<typename ValueType>
void AddIterator<DdType::Sylvan, ValueType>::treatNewCube() {
    this->relevantDontCareDdVariables.clear();

    // Now loop through the bits of all meta variables and check whether they need to be set, not set or are
    // don't cares. In the latter case, we add them to a special list, so we can iterate over their concrete
    // valuations later.
    for (auto const& metaVariable : *this->metaVariables) {
        bool metaVariableAppearsInCube = false;
        std::vector<std::tuple<storm::expressions::Variable, uint_fast64_t>> localRelenvantDontCareDdVariables;
        auto const& ddMetaVariable = this->ddManager->getMetaVariable(metaVariable);
        if (ddMetaVariable.getType() == MetaVariableType::Bool) {
            if (this->cube[globalToLocalIndexMap.at(ddMetaVariable.getDdVariables().front().getIndex())] == 0) {
                metaVariableAppearsInCube = true;
                currentValuation.setBooleanValue(metaVariable, false);
            } else if (this->cube[globalToLocalIndexMap.at(ddMetaVariable.getDdVariables().front().getIndex())] == 1) {
                metaVariableAppearsInCube = true;
                currentValuation.setBooleanValue(metaVariable, true);
            } else {
                localRelenvantDontCareDdVariables.push_back(std::make_tuple(metaVariable, 0));
            }
        } else {
            int_fast64_t intValue = 0;
            for (uint_fast64_t bitIndex = 0; bitIndex < ddMetaVariable.getNumberOfDdVariables(); ++bitIndex) {
                if (cube[globalToLocalIndexMap.at(ddMetaVariable.getDdVariables()[bitIndex].getIndex())] == 0) {
                    // Leave bit unset.
                    metaVariableAppearsInCube = true;
                } else if (cube[globalToLocalIndexMap.at(ddMetaVariable.getDdVariables()[bitIndex].getIndex())] == 1) {
                    intValue |= 1ull << (ddMetaVariable.getNumberOfDdVariables() - bitIndex - 1);
                    metaVariableAppearsInCube = true;
                } else {
                    // Temporarily leave bit unset so we can iterate trough the other option later.
                    // Add the bit to the relevant don't care bits.
                    localRelenvantDontCareDdVariables.push_back(std::make_tuple(metaVariable, ddMetaVariable.getNumberOfDdVariables() - bitIndex - 1));
                }
            }
            if (this->enumerateDontCareMetaVariables || metaVariableAppearsInCube) {
                currentValuation.setBitVectorValue(metaVariable, intValue + ddMetaVariable.getLow());
            }
        }

        // If all meta variables are to be enumerated or the meta variable appeared in the cube, we register the
        // missing bits to later enumerate all possible valuations.
        if (this->enumerateDontCareMetaVariables || metaVariableAppearsInCube) {
            relevantDontCareDdVariables.insert(relevantDontCareDdVariables.end(), localRelenvantDontCareDdVariables.begin(),
                                               localRelenvantDontCareDdVariables.end());
        }
    }

    // Finally, reset the cube counter.
    this->cubeCounter = 0;
}

template<typename ValueType>
void AddIterator<DdType::Sylvan, ValueType>::treatNextInCube() {
    // Start by increasing the counter and check which bits we need to set/unset in the new valuation.
    ++this->cubeCounter;

    for (uint_fast64_t index = 0; index < this->relevantDontCareDdVariables.size(); ++index) {
        auto const& ddMetaVariable = this->ddManager->getMetaVariable(std::get<0>(this->relevantDontCareDdVariables[index]));
        if (ddMetaVariable.getType() == MetaVariableType::Bool) {
            if ((this->cubeCounter & (1ull << index)) != 0) {
                currentValuation.setBooleanValue(std::get<0>(this->relevantDontCareDdVariables[index]), true);
            } else {
                currentValuation.setBooleanValue(std::get<0>(this->relevantDontCareDdVariables[index]), false);
            }
        } else {
            storm::expressions::Variable const& metaVariable = std::get<0>(this->relevantDontCareDdVariables[index]);
            if ((this->cubeCounter & (1ull << index)) != 0) {
                currentValuation.setBitVectorValue(metaVariable, ((currentValuation.getBitVectorValue(metaVariable) - ddMetaVariable.getLow()) |
                                                                  (1ull << std::get<1>(this->relevantDontCareDdVariables[index]))) +
                                                                     ddMetaVariable.getLow());
            } else {
                currentValuation.setBitVectorValue(metaVariable, ((currentValuation.getBitVectorValue(metaVariable) - ddMetaVariable.getLow()) &
                                                                  ~(1ull << std::get<1>(this->relevantDontCareDdVariables[index]))) +
                                                                     ddMetaVariable.getLow());
            }
        }
    }
}
#else
template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType>::AddIterator() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
AddIterator<DdType::Sylvan, ValueType>& AddIterator<DdType::Sylvan, ValueType>::operator++() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool AddIterator<DdType::Sylvan, ValueType>::operator==(AddIterator<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
bool AddIterator<DdType::Sylvan, ValueType>::operator!=(AddIterator<DdType::Sylvan, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
std::pair<storm::expressions::SimpleValuation, ValueType> AddIterator<DdType::Sylvan, ValueType>::operator*() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
#endif

template class AddIterator<DdType::Sylvan, double>;
template class AddIterator<DdType::Sylvan, uint_fast64_t>;
template class AddIterator<DdType::Sylvan, storm::RationalNumber>;
template class AddIterator<DdType::Sylvan, storm::RationalFunction>;
}  // namespace dd
}  // namespace storm
